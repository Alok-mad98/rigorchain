// Copyright (c) 2021-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <txorphanage.h>

#include <consensus/validation.h>
#include <logging.h>
#include <policy/policy.h>

#include <algorithm>
#include <cassert>
#include <limits>
#include <map>
#include <random.h>

/** Expiration time for orphan transactions in seconds */
static constexpr int64_t ORPHAN_TX_EXPIRE_TIME = 20 * 60;
/** Minimum time between orphan transactions expire time checks in seconds */
static constexpr int64_t ORPHAN_TX_EXPIRE_INTERVAL = 5 * 60;
/** Bitcoin Core #31829: announcements + unique inputs/10 must stay under this. */
static constexpr unsigned int MAX_ORPHANAGE_LATENCY_SCORE = 3000;
/** Bitcoin Core #31829: 404,000 WU reserved per announcing peer. */
static constexpr int64_t MAX_ORPHANAGE_WEIGHT_PER_PEER = 404'000;


bool TxOrphanage::AddTx(const CTransactionRef& tx, NodeId peer)
{
    LOCK(m_mutex);

    const uint256& hash = tx->GetHash();
    
    // If orphan already exists, add peer as announcer (lock is already held).
    auto it = m_orphans.find(hash);
    if (it != m_orphans.end()) {
        const auto ret = it->second.announcers.insert(peer);
        if (ret.second) {
            LogPrint(BCLog::MEMPOOL, "added peer=%d as announcer of orphan tx %s\n", peer, hash.ToString());
        }
        return ret.second;
    }

    // Ignore big transactions, to avoid a
    // send-big-orphans memory exhaustion attack. If a peer has a legitimate
    // large transaction with a missing parent then we assume
    // it will rebroadcast it later, after the parent transaction(s)
    // have been mined or received.
    // 100 orphans, each of which is at most 100,000 bytes big is
    // at most 10 megabytes of orphans and somewhat more byprev index (in the worst case):
    unsigned int sz = GetTransactionWeight(*tx);
    if (sz > MAX_STANDARD_TX_WEIGHT)
    {
        LogPrint(BCLog::MEMPOOL, "ignoring large orphan tx (size: %u, hash: %s)\n", sz, hash.ToString());
        return false;
    }

    std::set<NodeId> announcers;
    announcers.insert(peer);
    auto ret = m_orphans.emplace(hash, OrphanTx{tx, announcers, GetTime() + ORPHAN_TX_EXPIRE_TIME, m_orphan_list.size()});
    assert(ret.second);
    m_orphan_list.push_back(ret.first);
    // Allow for lookups in the orphan pool by wtxid, as well as txid
    m_wtxid_to_orphan_it.emplace(tx->GetWitnessHash(), ret.first);
    for (const CTxIn& txin : tx->vin) {
        m_outpoint_to_orphan_it[txin.prevout].insert(ret.first);
    }

    LogPrint(BCLog::MEMPOOL, "stored orphan tx %s (mapsz %u outsz %u)\n", hash.ToString(),
             m_orphans.size(), m_outpoint_to_orphan_it.size());
    return true;
}

bool TxOrphanage::AddAnnouncer(const uint256& txid, NodeId peer)
{
    LOCK(m_mutex);
    
    auto it = m_orphans.find(txid);
    if (it != m_orphans.end()) {
        const auto ret = it->second.announcers.insert(peer);
        if (ret.second) {
            LogPrint(BCLog::MEMPOOL, "added peer=%d as announcer of orphan tx %s\n", peer, txid.ToString());
            return true;
        }
    }
    return false;
}

std::set<NodeId> TxOrphanage::GetAnnouncers(const uint256& txid)
{
    LOCK(m_mutex);
    
    auto it = m_orphans.find(txid);
    if (it != m_orphans.end()) {
        return it->second.announcers;
    }
    return {};
}

int TxOrphanage::EraseTx(const uint256& txid)
{
    LOCK(m_mutex);
    return _EraseTx(txid);
}

int TxOrphanage::_EraseTx(const uint256& txid)
{
    AssertLockHeld(m_mutex);
    std::map<uint256, OrphanTx>::iterator it = m_orphans.find(txid);
    if (it == m_orphans.end())
        return 0;
    for (const CTxIn& txin : it->second.tx->vin)
    {
        auto itPrev = m_outpoint_to_orphan_it.find(txin.prevout);
        if (itPrev == m_outpoint_to_orphan_it.end())
            continue;
        itPrev->second.erase(it);
        if (itPrev->second.empty())
            m_outpoint_to_orphan_it.erase(itPrev);
    }

    size_t old_pos = it->second.list_pos;
    assert(m_orphan_list[old_pos] == it);
    if (old_pos + 1 != m_orphan_list.size()) {
        // Unless we're deleting the last entry in m_orphan_list, move the last
        // entry to the position we're deleting.
        auto it_last = m_orphan_list.back();
        m_orphan_list[old_pos] = it_last;
        it_last->second.list_pos = old_pos;
    }
    m_orphan_list.pop_back();
    m_wtxid_to_orphan_it.erase(it->second.tx->GetWitnessHash());

    m_orphans.erase(it);
    return 1;
}

void TxOrphanage::EraseForPeer(NodeId peer)
{
    LOCK(m_mutex);

    m_peer_work_set.erase(peer);

    int nErased = 0;
    std::map<uint256, OrphanTx>::iterator iter = m_orphans.begin();
    while (iter != m_orphans.end())
    {
        std::map<uint256, OrphanTx>::iterator maybeErase = iter++; // increment to avoid iterator becoming invalid
        auto announcer_it = maybeErase->second.announcers.find(peer);
        if (announcer_it != maybeErase->second.announcers.end()) {
            // Remove peer from announcers set
            maybeErase->second.announcers.erase(peer);
            
            // If no announcers remain, erase the orphan
            if (maybeErase->second.announcers.empty()) {
                nErased += _EraseTx(maybeErase->second.tx->GetHash());
            }
        }
    }
    if (nErased > 0) LogPrint(BCLog::MEMPOOL, "Erased %d orphan tx from peer=%d\n", nErased, peer);
}

void TxOrphanage::ExpireOrphans()
{
    AssertLockHeld(m_mutex);

    static int64_t nNextSweep;
    int64_t nNow = GetTime();
    if (nNextSweep > nNow) return;

    int nErased = 0;
    int64_t nMinExpTime = nNow + ORPHAN_TX_EXPIRE_TIME - ORPHAN_TX_EXPIRE_INTERVAL;
    auto iter = m_orphans.begin();
    while (iter != m_orphans.end()) {
        auto maybeErase = iter++;
        if (maybeErase->second.nTimeExpire <= nNow) {
            nErased += _EraseTx(maybeErase->second.tx->GetHash());
        } else {
            nMinExpTime = std::min(maybeErase->second.nTimeExpire, nMinExpTime);
        }
    }
    nNextSweep = nMinExpTime + ORPHAN_TX_EXPIRE_INTERVAL;
    if (nErased > 0) LogPrint(BCLog::MEMPOOL, "Erased %d orphan tx due to expiration\n", nErased);
}

bool TxOrphanage::ExceedsGlobalLimits() const
{
    AssertLockHeld(m_mutex);

    size_t announcements = 0;
    size_t unique_inputs = 0;
    int64_t total_weight = 0;
    std::set<NodeId> peers;
    for (const auto& entry : m_orphans) {
        announcements += entry.second.announcers.size();
        unique_inputs += entry.second.tx->vin.size();
        total_weight += GetTransactionWeight(*entry.second.tx);
        peers.insert(entry.second.announcers.begin(), entry.second.announcers.end());
    }
    const size_t n_peers = std::max<size_t>(peers.size(), 1);
    const int64_t max_weight = MAX_ORPHANAGE_WEIGHT_PER_PEER * static_cast<int64_t>(n_peers);
    const unsigned int latency = static_cast<unsigned int>(announcements + unique_inputs / 10);
    return latency > MAX_ORPHANAGE_LATENCY_SCORE || total_weight > max_weight;
}

NodeId TxOrphanage::PickDoSiestPeer() const
{
    AssertLockHeld(m_mutex);

    std::map<NodeId, size_t> announcements;
    std::map<NodeId, int64_t> weight;
    for (const auto& entry : m_orphans) {
        const int64_t w = GetTransactionWeight(*entry.second.tx);
        for (NodeId peer : entry.second.announcers) {
            announcements[peer] += 1;
            weight[peer] += w;
        }
    }
    NodeId worst = -1;
    double worst_score = -1;
    for (const auto& [peer, n_ann] : announcements) {
        const double cpu = static_cast<double>(n_ann);
        const double mem = static_cast<double>(weight[peer]) / static_cast<double>(MAX_ORPHANAGE_WEIGHT_PER_PEER);
        const double score = std::max(cpu, mem);
        if (score > worst_score) {
            worst_score = score;
            worst = peer;
        }
    }
    return worst;
}

bool TxOrphanage::EvictOneAnnouncement(NodeId peer)
{
    AssertLockHeld(m_mutex);

    uint256 victim;
    int64_t oldest = std::numeric_limits<int64_t>::max();
    bool found = false;
    for (const auto& entry : m_orphans) {
        if (!entry.second.announcers.count(peer)) continue;
        if (entry.second.nTimeExpire < oldest) {
            oldest = entry.second.nTimeExpire;
            victim = entry.first;
            found = true;
        }
    }
    if (!found) return false;

    auto it = m_orphans.find(victim);
    if (it == m_orphans.end()) return false;
    it->second.announcers.erase(peer);
    m_peer_work_set[peer].erase(victim);
    if (it->second.announcers.empty()) {
        _EraseTx(victim);
    }
    return true;
}

void TxOrphanage::LimitOrphans()
{
    LOCK(m_mutex);
    ExpireOrphans();
    unsigned int nEvicted = 0;
    while (ExceedsGlobalLimits()) {
        const NodeId worst = PickDoSiestPeer();
        if (worst == -1) break;
        if (!EvictOneAnnouncement(worst)) break;
        ++nEvicted;
    }
    if (nEvicted > 0) LogPrint(BCLog::MEMPOOL, "orphanage DoS trim, removed %u announcements\n", nEvicted);
}

void TxOrphanage::LimitOrphans(unsigned int max_orphans)
{
    LOCK(m_mutex);

    ExpireOrphans();
    unsigned int nEvicted = 0;
    while (m_orphans.size() > max_orphans) {
        const NodeId worst = PickDoSiestPeer();
        if (worst == -1) {
            // Fall back to random eviction if scores are empty.
            FastRandomContext rng;
            size_t randompos = rng.randrange(m_orphan_list.size());
            _EraseTx(m_orphan_list[randompos]->first);
        } else if (!EvictOneAnnouncement(worst)) {
            break;
        }
        ++nEvicted;
    }
    if (nEvicted > 0) LogPrint(BCLog::MEMPOOL, "orphanage overflow, removed %u announcements\n", nEvicted);
}

void TxOrphanage::AddChildrenToWorkSet(const CTransaction& tx)
{
    LOCK(m_mutex);


    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        const auto it_by_prev = m_outpoint_to_orphan_it.find(COutPoint(tx.GetHash(), i));
        if (it_by_prev != m_outpoint_to_orphan_it.end()) {
            for (const auto& elem : it_by_prev->second) {
                // Add this tx to the work set for all announcers of this orphan
                for (NodeId announcer : elem->second.announcers) {
                    std::set<uint256>& orphan_work_set = m_peer_work_set.try_emplace(announcer).first->second;
                    orphan_work_set.insert(elem->first);
                }
            }
        }
    }
}

bool TxOrphanage::HaveTx(const GenTxid& gtxid) const
{
    LOCK(m_mutex);
    if (gtxid.IsWtxid()) {
        return m_wtxid_to_orphan_it.count(gtxid.GetHash());
    } else {
        return m_orphans.count(gtxid.GetHash());
    }
}

CTransactionRef TxOrphanage::GetTx(const uint256& hash) const
{
    LOCK(m_mutex);
    auto it = m_orphans.find(hash);
    if (it != m_orphans.end()) return it->second.tx;
    auto wit = m_wtxid_to_orphan_it.find(hash);
    if (wit != m_wtxid_to_orphan_it.end()) return wit->second->second.tx;
    return nullptr;
}

CTransactionRef TxOrphanage::GetTxToReconsider(NodeId peer)
{
    LOCK(m_mutex);

    auto work_set_it = m_peer_work_set.find(peer);
    if (work_set_it != m_peer_work_set.end()) {
        auto& work_set = work_set_it->second;
        while (!work_set.empty()) {
            uint256 txid = *work_set.begin();
            work_set.erase(work_set.begin());

            const auto orphan_it = m_orphans.find(txid);
            if (orphan_it != m_orphans.end()) {
                return orphan_it->second.tx;
            }
        }
    }
    return nullptr;
}

bool TxOrphanage::HaveTxToReconsider(NodeId peer)
{
    LOCK(m_mutex);

    auto work_set_it = m_peer_work_set.find(peer);
    if (work_set_it != m_peer_work_set.end()) {
        auto& work_set = work_set_it->second;
        return !work_set.empty();
    }
    return false;
}

void TxOrphanage::EraseForBlock(const CBlock& block)
{
    LOCK(m_mutex);

    std::vector<uint256> vOrphanErase;

    for (const CTransactionRef& ptx : block.vtx) {
        const CTransaction& tx = *ptx;

        // Which orphan pool entries must we evict?
        for (const auto& txin : tx.vin) {
            auto itByPrev = m_outpoint_to_orphan_it.find(txin.prevout);
            if (itByPrev == m_outpoint_to_orphan_it.end()) continue;
            for (auto mi = itByPrev->second.begin(); mi != itByPrev->second.end(); ++mi) {
                const CTransaction& orphanTx = *(*mi)->second.tx;
                const uint256& orphanHash = orphanTx.GetHash();
                vOrphanErase.push_back(orphanHash);
            }
        }
    }

    // Erase orphan transactions included or precluded by this block
    if (vOrphanErase.size()) {
        int nErased = 0;
        for (const uint256& orphanHash : vOrphanErase) {
            nErased += _EraseTx(orphanHash);
        }
        LogPrint(BCLog::MEMPOOL, "Erased %d orphan tx included or conflicted by block\n", nErased);
    }
}
