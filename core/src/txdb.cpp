// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <txdb.h>

#include <chain.h>
#include <logging.h>
#include <pow.h>
#include <random.h>
#include <shutdown.h>
#include <uint256.h>
#include <util/translation.h>
#include <util/vector.h>

#include <cassert>
#include <stdint.h>

static constexpr uint8_t DB_COIN{'C'};
static constexpr uint8_t DB_BLOCK_FILES{'f'};
static constexpr uint8_t DB_BLOCK_INDEX{'b'};

static constexpr uint8_t DB_BEST_BLOCK{'B'};
static constexpr uint8_t DB_HEAD_BLOCKS{'H'};
static constexpr uint8_t DB_FLAG{'F'};
static constexpr uint8_t DB_REINDEX_FLAG{'R'};
static constexpr uint8_t DB_LAST_BLOCK{'l'};

static constexpr uint8_t DB_TXINDEX_BLOCK{'T'};

std::optional<bilingual_str> CheckLegacyTxindex(CBlockTreeDB& block_tree_db)
{
    CBlockLocator ignored{};
    if (block_tree_db.Read(DB_TXINDEX_BLOCK, ignored)) {
        return _("The -txindex upgrade started by a previous version cannot be completed. Restart with the previous version or run a full -reindex.");
    }
    bool txindex_legacy_flag{false};
    block_tree_db.ReadFlag("txindex", txindex_legacy_flag);
    if (txindex_legacy_flag) {
        // Disable legacy txindex and warn once about occupied disk space
        if (!block_tree_db.WriteFlag("txindex", false)) {
            return Untranslated("Failed to write block index db flag 'txindex'='0'");
        }
        return _("The block index db contains a legacy 'txindex'. To clear the occupied disk space, run a full -reindex, otherwise ignore this error. This error message will not be displayed again.");
    }
    return std::nullopt;
}

namespace {

struct CoinEntry {
    COutPoint* outpoint;
    uint8_t key;
    explicit CoinEntry(const COutPoint* ptr) : outpoint(const_cast<COutPoint*>(ptr)), key(DB_COIN)  {}

    SERIALIZE_METHODS(CoinEntry, obj) { READWRITE(obj.key, obj.outpoint->hash, VARINT(obj.outpoint->n)); }
};

} // namespace

bool CCoinsViewDB::NeedsUpgrade()
{
    // RIGOR has no inherited Bitcoin coin formats. Wipe / -reindex is the
    // only recovery. Deserialize failure of any record fail-closes.
    return false;
}

CCoinsViewDB::CCoinsViewDB(DBParams db_params, CoinsViewOptions options) :
    m_db_params{std::move(db_params)},
    m_options{std::move(options)},
    m_db{std::make_unique<CDBWrapper>(m_db_params)} { }

void CCoinsViewDB::ResizeCache(size_t new_cache_size)
{
    // We can't do this operation with an in-memory DB since we'll lose all the coins upon
    // reset.
    if (!m_db_params.memory_only) {
        // Have to do a reset first to get the original `m_db` state to release its
        // filesystem lock.
        m_db.reset();
        m_db_params.cache_bytes = new_cache_size;
        m_db_params.wipe_data = false;
        m_db = std::make_unique<CDBWrapper>(m_db_params);
    }
}

bool CCoinsViewDB::GetCoin(const COutPoint &outpoint, Coin &coin) const {
    auto wrap = Using<CoinDiskFormatter>(coin);
    return m_db->Read(CoinEntry(&outpoint), wrap);
}

bool CCoinsViewDB::HaveCoin(const COutPoint &outpoint) const {
    return m_db->Exists(CoinEntry(&outpoint));
}

uint256 CCoinsViewDB::GetBestBlock() const {
    uint256 hashBestChain;
    if (!m_db->Read(DB_BEST_BLOCK, hashBestChain))
        return uint256();
    return hashBestChain;
}

std::vector<uint256> CCoinsViewDB::GetHeadBlocks() const {
    std::vector<uint256> vhashHeadBlocks;
    if (!m_db->Read(DB_HEAD_BLOCKS, vhashHeadBlocks)) {
        return std::vector<uint256>();
    }
    return vhashHeadBlocks;
}

bool CCoinsViewDB::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock, bool erase) {
    CDBBatch batch(*m_db);
    size_t count = 0;
    size_t changed = 0;
 // / H-3-1: count of mid-batch partial flushes that LevelDB
    // actually performed during this call. Diagnostic only; the value is
    // never read by any control-flow branch. Surfacing it under
    // `BCLog::COINDB` lets the operator observe how many on-disk write
    // points a single chainstate transition actually crossed, which
    // affects the size of the crash-recovery window a `SIGKILL` may land
    // in. This does NOT change ordering, error handling, or fsync
    // semantics.
    size_t partial_flush_count = 0;
    assert(!hashBlock.IsNull());

    uint256 old_tip = GetBestBlock();
    if (old_tip.IsNull()) {
        // We may be in the middle of replaying.
        std::vector<uint256> old_heads = GetHeadBlocks();
        if (old_heads.size() == 2) {
            assert(old_heads[0] == hashBlock);
            old_tip = old_heads[1];
        }
    }

    // In the first batch, mark the database as being in the middle of a
    // transition from old_tip to hashBlock.
    // A vector is used for future extensibility, as we may want to support
    // interrupting after partial writes from multiple independent reorgs.
    batch.Erase(DB_BEST_BLOCK);
    batch.Write(DB_HEAD_BLOCKS, Vector(hashBlock, old_tip));

    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) {
            CoinEntry entry(&it->first);
            if (it->second.coin.IsSpent())
                batch.Erase(entry);
            else
                batch.Write(entry, Using<CoinDiskFormatter>(it->second.coin));
            changed++;
        }
        count++;
        it = erase ? mapCoins.erase(it) : std::next(it);
        if (batch.SizeEstimate() > m_options.batch_write_bytes) {
            LogPrint(BCLog::COINDB, "Writing partial batch of %.2f MiB\n", batch.SizeEstimate() * (1.0 / 1048576.0));
            m_db->WriteBatch(batch);
            batch.Clear();
 ++partial_flush_count; // / H-3-1: diagnostic only.
            if (m_options.simulate_crash_ratio) {
                static FastRandomContext rng;
                if (rng.randrange(m_options.simulate_crash_ratio) == 0) {
                    LogPrintf("Simulating a crash. Goodbye.\n");
                    _Exit(0);
                }
            }
        }
    }

    // In the last batch, mark the database as consistent with hashBlock again.
    batch.Erase(DB_HEAD_BLOCKS);
    batch.Write(DB_BEST_BLOCK, hashBlock);

    LogPrint(BCLog::COINDB, "Writing final batch of %.2f MiB\n", batch.SizeEstimate() * (1.0 / 1048576.0));
    bool ret = m_db->WriteBatch(batch);
    LogPrint(BCLog::COINDB, "Committed %u changed transaction outputs (out of %u) to coin database...\n", (unsigned int)changed, (unsigned int)count);
 // / H-3-1: emit a single end-of-call diagnostic line
    // capturing how many mid-batch partial flushes this transition
    // produced. Each partial flush is an independent crash-recovery
    // point, so the count is the size of the `SIGKILL`-vulnerable window
    // for this `BatchWrite` call. Logged under `BCLog::COINDB` only;
    // adds no consensus side effects.
    LogPrint(BCLog::COINDB, "BatchWrite produced %u mid-batch partial flush(es)\n", (unsigned int)partial_flush_count);
    return ret;
}

size_t CCoinsViewDB::EstimateSize() const
{
    return m_db->EstimateSize(DB_COIN, uint8_t(DB_COIN + 1));
}

bool CBlockTreeDB::ReadBlockFileInfo(int nFile, CBlockFileInfo &info) {
    return Read(std::make_pair(DB_BLOCK_FILES, nFile), info);
}

bool CBlockTreeDB::WriteReindexing(bool fReindexing) {
    if (fReindexing)
        return Write(DB_REINDEX_FLAG, uint8_t{'1'});
    else
        return Erase(DB_REINDEX_FLAG);
}

void CBlockTreeDB::ReadReindexing(bool &fReindexing) {
    fReindexing = Exists(DB_REINDEX_FLAG);
}

bool CBlockTreeDB::ReadLastBlockFile(int &nFile) {
    return Read(DB_LAST_BLOCK, nFile);
}

/** Specialization of CCoinsViewCursor to iterate over a CCoinsViewDB */
class CCoinsViewDBCursor: public CCoinsViewCursor
{
public:
    // Prefer using CCoinsViewDB::Cursor() since we want to perform some
    // cache warmup on instantiation.
    CCoinsViewDBCursor(CDBIterator* pcursorIn, const uint256&hashBlockIn):
        CCoinsViewCursor(hashBlockIn), pcursor(pcursorIn) {}
    ~CCoinsViewDBCursor() = default;

    bool GetKey(COutPoint &key) const override;
    bool GetValue(Coin &coin) const override;

    bool Valid() const override;
    void Next() override;

private:
    std::unique_ptr<CDBIterator> pcursor;
    std::pair<char, COutPoint> keyTmp;

    friend class CCoinsViewDB;
};

std::unique_ptr<CCoinsViewCursor> CCoinsViewDB::Cursor() const
{
    auto i = std::make_unique<CCoinsViewDBCursor>(
        const_cast<CDBWrapper&>(*m_db).NewIterator(), GetBestBlock());
    /* It seems that there are no "const iterators" for LevelDB.  Since we
       only need read operations on it, use a const-cast to get around
       that restriction.  */
    i->pcursor->Seek(DB_COIN);
    if (i->pcursor->Valid() && i->pcursor->HasKeyType(DB_COIN)) {
        CoinEntry entry(&i->keyTmp.second);
        i->pcursor->GetKey(entry);
        i->keyTmp.first = entry.key;
    } else {
        i->keyTmp.first = 0;
    }
    return i;
}

bool CCoinsViewDBCursor::GetKey(COutPoint &key) const
{
    // Return cached key
    if (keyTmp.first == DB_COIN) {
        key = keyTmp.second;
        return true;
    }
    return false;
}

bool CCoinsViewDBCursor::GetValue(Coin &coin) const
{
    auto wrap = Using<CoinDiskFormatter>(coin);
    return pcursor->GetValue(wrap);
}

bool CCoinsViewDBCursor::Valid() const
{
    return keyTmp.first == DB_COIN;
}

void CCoinsViewDBCursor::Next()
{
    pcursor->Next();
    if (!pcursor->Valid() || !pcursor->HasKeyType(DB_COIN)) {
        keyTmp.first = 0;
        return;
    }
    CoinEntry entry(&keyTmp.second);
    pcursor->GetKey(entry);
    keyTmp.first = entry.key;
}

bool CBlockTreeDB::WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile, const std::vector<const CBlockIndex*>& blockinfo) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<int, const CBlockFileInfo*> >::const_iterator it=fileInfo.begin(); it != fileInfo.end(); it++) {
        batch.Write(std::make_pair(DB_BLOCK_FILES, it->first), *it->second);
    }
    batch.Write(DB_LAST_BLOCK, nLastFile);
    for (std::vector<const CBlockIndex*>::const_iterator it=blockinfo.begin(); it != blockinfo.end(); it++) {
        batch.Write(std::make_pair(DB_BLOCK_INDEX, (*it)->GetBlockHash()), CDiskBlockIndex(*it));
    }
    return WriteBatch(batch, true);
}

bool CBlockTreeDB::WriteFlag(const std::string &name, bool fValue) {
    return Write(std::make_pair(DB_FLAG, name), fValue ? uint8_t{'1'} : uint8_t{'0'});
}

bool CBlockTreeDB::ReadFlag(const std::string &name, bool &fValue) {
    uint8_t ch;
    if (!Read(std::make_pair(DB_FLAG, name), ch))
        return false;
    fValue = ch == uint8_t{'1'};
    return true;
}

bool CBlockTreeDB::LoadBlockIndexGuts(const Consensus::Params& consensusParams, std::function<CBlockIndex*(const uint256&)> insertBlockIndex)
{
    AssertLockHeld(::cs_main);
    (void)consensusParams;
    std::unique_ptr<CDBIterator> pcursor(NewIterator());
    pcursor->Seek(std::make_pair(DB_BLOCK_INDEX, uint256()));

    // Load m_block_index
    while (pcursor->Valid()) {
        if (ShutdownRequested()) return false;
        if (!pcursor->HasKeyType(DB_BLOCK_INDEX)) {
            break;
        }
        std::pair<uint8_t, uint256> key;
        CDiskBlockIndex diskindex;
        try {
            pcursor->GetKey(key);
            assert(key.first == DB_BLOCK_INDEX);
            pcursor->GetValue(diskindex);
        } catch (const std::exception& e) {
            return error("%s: failed to read block index (%s)", __func__, e.what());
        }
        CBlockIndex* pindexNew = insertBlockIndex(diskindex.ConstructBlockHash());
        pindexNew->pprev          = insertBlockIndex(diskindex.hashPrev);
        pindexNew->nHeight        = diskindex.nHeight;
        pindexNew->nFile          = diskindex.nFile;
        pindexNew->nDataPos       = diskindex.nDataPos;
        pindexNew->nUndoPos       = diskindex.nUndoPos;
        pindexNew->nVersion       = diskindex.nVersion;
        pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
        pindexNew->nTime          = diskindex.nTime;
        pindexNew->nBits          = diskindex.nBits;
        pindexNew->nNonce         = diskindex.nNonce;
        pindexNew->Observational().SetCumulativeConvergence(diskindex.Observational().GetCumulativeConvergence());
        pindexNew->has_res = diskindex.has_res;
        pindexNew->last_res_mint_block_hash = diskindex.last_res_mint_block_hash;
        pindexNew->m_block_res_mint_sum = diskindex.m_block_res_mint_sum;
        pindexNew->m_block_converge_sum = diskindex.m_block_converge_sum;
        pindexNew->nStatus        = diskindex.nStatus;
        pindexNew->nTx            = diskindex.nTx;

        pcursor->Next();
    }

    return true;
}
