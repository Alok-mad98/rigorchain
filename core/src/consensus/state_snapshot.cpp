// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/state_snapshot.h>

#include <chain.h>
#include <coins.h>
#include <crypto/muhash.h>
#include <hash.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <serialize.h>
#include <span.h>
#include <streams.h>
#include <uint256.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string_view>

namespace Consensus {

namespace {

//! Domain-separation tag. The trailing "v2" pins the on-the-wire format of
//! the digest. Any change to leaf layout, preamble layout, or trailer layout
//! MUST bump this to "v3" (and so on). See state_snapshot.h for the full format.
constexpr std::string_view kStateHashDomainTag{"RIGOR/STATE_HASH/v2"};

//! Build the canonical leaf bytes for one (outpoint, coin) pair.
//! Layout (matches Coin::Serialize on disk, identical to kernel/coinstats.cpp's
//! TxOutSer + Coin's nFlags tail):
//!   COutPoint::Serialize(op)
//!   || VARINT(coin.nHeight*2 + coin.fCoinBase)
//!   || TxOutCompression(coin.out)
//!   || VARINT(coin.nFlags)
//!
//! DataStream is intentionally constructed without an explicit (type, version)
//! pair: Coin::Serialize and TxOutCompression are version-independent, and
//! pinning to PROTOCOL_VERSION would risk silent drift on future bumps. This
//! matches the pattern used by `kernel/coinstats.cpp::TxOutSer`, which is the
//! production-validated canonical form for the assumeutxo / UTXO snapshot
//! commitment.
DataStream MakeLeafBytes(const COutPoint& op, const Coin& coin)
{
    DataStream ss{};
    ss << op;
    ss << coin;
    return ss;
}

//! Witness-V1 lock-program shape per RFC-003 §5: exactly `OP_1 0x20 <32>`.
//! Matches `CaptureChainstateSummary` / `CaptureReplaySnapshot` lock counting.
inline bool HasV1LockProgramShape(const CScript& spk)
{
    return spk.size() == 34 &&
           static_cast<uint8_t>(spk[0]) == 0x51 /*OP_1*/ &&
           static_cast<uint8_t>(spk[1]) == 0x20 /*push 32*/;
}

struct UtxoAggregates {
    uint64_t utxo_count{0};
    uint64_t res_balance{0};
    uint64_t other_balance{0};
    uint32_t mint_count{0};
    uint32_t lock_count{0};
    uint32_t converge_count{0};
};

void WritePindexConsensusTrailer(HashWriter& hw, const CBlockIndex* pindex)
{
    if (pindex != nullptr) {
        hw << pindex->Observational().GetCumulativeConvergence();
        hw << pindex->m_draw_candidate_root;
        hw << static_cast<uint8_t>(pindex->has_res);
        hw << pindex->last_res_mint_block_hash;
        hw << static_cast<int64_t>(pindex->m_block_res_mint_sum);
        hw << static_cast<int64_t>(pindex->Observational().GetChainResMintTotal());
        hw << static_cast<int64_t>(pindex->m_block_converge_sum);
        hw << static_cast<int64_t>(pindex->Observational().GetChainOruTotal());
    } else {
        const uint256 zero_hash;
        hw << uint64_t{0};
        hw << zero_hash;
        hw << uint8_t{0};
        hw << zero_hash;
        hw << int64_t{0};
        hw << int64_t{0};
        hw << int64_t{0};
        hw << int64_t{0};
    }
}

void WriteAggregateTrailer(HashWriter& hw, const UtxoAggregates& agg)
{
    hw << agg.utxo_count;
    hw << agg.res_balance;
    hw << agg.other_balance;
    hw << agg.mint_count;
    hw << agg.lock_count;
    hw << agg.converge_count;
}

uint256 FinalizeDigestV2(uint32_t height_le,
                        const uint256& tip_hash,
                        const uint256& utxo_hash,
                        const CBlockIndex* pindex,
                        const UtxoAggregates& agg)
{
    HashWriter hw;
    hw.write(Span<const std::byte>{
        reinterpret_cast<const std::byte*>(kStateHashDomainTag.data()),
        kStateHashDomainTag.size()});
    hw << height_le;
    hw << tip_hash;
    hw << utxo_hash;
    WritePindexConsensusTrailer(hw, pindex);
    WriteAggregateTrailer(hw, agg);
    return hw.GetHash();
}

} // namespace

uint256 ComputeDeterministicStateHash(const CCoinsViewCache& view, const CBlockIndex* pindex)
{
    UtxoAggregates agg{};

    // Pre-genesis: fixed preamble with zero tip / zero UTXO MuHash and an
    // all-zero 141-byte trailer (105-byte index projection + aggregates).
    if (pindex == nullptr) {
        return FinalizeDigestV2(/*height_le=*/uint32_t{0},
                                /*tip_hash=*/uint256{},
                                /*utxo_hash=*/uint256{},
                                /*pindex=*/nullptr,
                                agg);
    }

    const uint256 tip_hash = pindex->GetBlockHash();

    assert(view.GetBestBlock() == tip_hash &&
           "ComputeDeterministicStateHash: view.GetBestBlock() != pindex->GetBlockHash()");

    MuHash3072 utxo_muhash;

    std::unique_ptr<CCoinsViewCursor> pcursor(view.Cursor());
    if (pcursor) {
        while (pcursor->Valid()) {
            COutPoint key;
            Coin coin;
            if (pcursor->GetKey(key) && pcursor->GetValue(coin)) {
                if (!coin.IsSpent()) {
                    DataStream leaf = MakeLeafBytes(key, coin);
                    utxo_muhash.Insert(MakeUCharSpan(leaf));

                    ++agg.utxo_count;
                    const CScript& spk = coin.out.scriptPubKey;
                    const bool is_res = IsResScript(spk);
                    const bool is_convergence = IsConvergenceScript(spk);
                    if (is_res) {
                        ++agg.mint_count;
                        agg.res_balance += static_cast<uint64_t>(coin.out.nValue);
                    } else {
                        agg.other_balance += static_cast<uint64_t>(coin.out.nValue);
                    }
                    if (HasV1LockProgramShape(spk)) {
                        ++agg.lock_count;
                    }
                    if (is_convergence) {
                        ++agg.converge_count;
                    }
                }
            }
            pcursor->Next();
        }
    }

    uint256 utxo_hash;
    utxo_muhash.Finalize(utxo_hash);

    return FinalizeDigestV2(static_cast<uint32_t>(pindex->nHeight),
                           tip_hash,
                           utxo_hash,
                           pindex,
                           agg);
}

} // namespace Consensus
