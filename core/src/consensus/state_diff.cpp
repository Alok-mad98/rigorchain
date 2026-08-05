// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/state_diff.h>

#include <chain.h>
#include <coins.h>
#include <consensus/state_snapshot.h>
#include <crypto/muhash.h>
#include <hash.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <serialize.h>
#include <span.h>
#include <streams.h>
#include <tinyformat.h>
#include <uint256.h>

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <algorithm>

namespace Consensus {

namespace {

//! Wire layout offsets for `ChainstateSummary::Serialize` (CSM1). Must stay in
//! lock-step with `Serialize()` / `Deserialize()` and `kSerializedRecordSize`.
constexpr size_t kOffHeight = 4;
constexpr size_t kOffBlockHash = 8;
constexpr size_t kOffStateHash = 40;
constexpr size_t kOffUtxoCount = 72;
constexpr size_t kOffResBalance = 80;
constexpr size_t kOffOtherBalance = 88;
constexpr size_t kOffMintCount = 96;
constexpr size_t kOffLockCount = 100;
constexpr size_t kOffConvergeCount = 104;
constexpr size_t kOffBucketBase = 108;
constexpr size_t kOffSelectionPresent = kOffBucketBase + ChainstateSummary::kBucketCount * 32;
constexpr size_t kOffSelectionHasWinner = kOffSelectionPresent + 1;
constexpr size_t kOffSelectionWinnerIndex = kOffSelectionHasWinner + 1;
constexpr size_t kOffSelectionCandidateCount = kOffSelectionWinnerIndex + 4;
constexpr size_t kOffSelectionWinnerWeight = kOffSelectionCandidateCount + 4;
constexpr size_t kOffSelectionConvergenceRoot = kOffSelectionWinnerWeight + 8;
constexpr size_t kOffSelectionWinnerTxid = kOffSelectionConvergenceRoot + 32;
constexpr size_t kOffSelectionSigmasHash = kOffSelectionWinnerTxid + 32;
constexpr size_t kOffSelectionWeightsHash = kOffSelectionSigmasHash + 32;

//! Fixed serialized record size for ChainstateSummary (CSM1).
constexpr size_t kSerializedRecordSize =
    4 + 4 + 32 + 32 + 8 + 8 + 8 + 4 + 4 + 4 +
    (256 * 32) +
    1 + 1 + 4 + 4 + 8 + 32 + 32 + 32 + 32;

static_assert(kOffSelectionWeightsHash + 32 == kSerializedRecordSize,
              "CSM1 wire offset table out of sync with kSerializedRecordSize");

std::string DescribeWireSegmentAt(size_t off)
{
    if (off <= 3) return "magic";
    if (off <= 7) return "height";
    if (off <= 39) return "block_hash";
    if (off <= 71) return "state_hash";
    if (off <= 79) return "utxo_count";
    if (off <= 87) return "res_balance";
    if (off <= 95) return "other_balance";
    if (off <= 99) return "mint_count";
    if (off <= 103) return "lock_count";
    if (off <= 107) return "converge_count";
    if (off < kOffSelectionPresent) {
        return strprintf("utxo_bucket[%u]", static_cast<unsigned>((off - kOffBucketBase) / 32));
    }
    if (off == kOffSelectionPresent) return "selection_present";
    if (off == kOffSelectionHasWinner) return "selection_has_winner";
    if (off <= kOffSelectionWinnerIndex + 3) return "selection_winner_index";
    if (off <= kOffSelectionCandidateCount + 3) return "selection_candidate_count";
    if (off <= kOffSelectionWinnerWeight + 7) return "selection_winner_weight";
    if (off <= kOffSelectionConvergenceRoot + 31) return "selection_convergence_root";
    if (off <= kOffSelectionWinnerTxid + 31) return "selection_winner_txid";
    if (off <= kOffSelectionSigmasHash + 31) return "selection_sigmas_hash";
    if (off <= kOffSelectionWeightsHash + 31) return "selection_weights_hash";
    return "past_end_of_csm1";
}

void AnnotateSerializedByteLocation(DiffResult& r)
{
    if (r.identical) {
        r.serialized_byte_offset = -1;
        r.wire_segment.clear();
        return;
    }
    const std::string& f = r.field;
    if (f == "height") {
        r.serialized_byte_offset = static_cast<int64_t>(kOffHeight);
        r.wire_segment = "height";
    } else if (f == "block_hash") {
        r.serialized_byte_offset = static_cast<int64_t>(kOffBlockHash);
        r.wire_segment = "block_hash";
    } else if (f == "state_hash") {
        r.serialized_byte_offset = static_cast<int64_t>(kOffStateHash);
        r.wire_segment = "state_hash";
    } else if (f == "utxo_count") {
        r.serialized_byte_offset = static_cast<int64_t>(kOffUtxoCount);
        r.wire_segment = "utxo_count";
    } else if (f == "res_balance") {
        r.serialized_byte_offset = static_cast<int64_t>(kOffResBalance);
        r.wire_segment = "res_balance";
    } else if (f == "other_balance") {
        r.serialized_byte_offset = static_cast<int64_t>(kOffOtherBalance);
        r.wire_segment = "other_balance";
    } else if (f == "mint_count") {
        r.serialized_byte_offset = static_cast<int64_t>(kOffMintCount);
        r.wire_segment = "mint_count";
    } else if (f == "lock_count") {
        r.serialized_byte_offset = static_cast<int64_t>(kOffLockCount);
        r.wire_segment = "lock_count";
    } else if (f == "converge_count") {
        r.serialized_byte_offset = static_cast<int64_t>(kOffConvergeCount);
        r.wire_segment = "converge_count";
    } else if (f == "utxo_bucket_hashes" && r.bucket_index >= 0) {
        r.serialized_byte_offset = static_cast<int64_t>(kOffBucketBase) + static_cast<int64_t>(r.bucket_index) * 32;
        r.wire_segment = strprintf("utxo_bucket[%d]", r.bucket_index);
    } else if (f == "selection_present") {
        r.serialized_byte_offset = static_cast<int64_t>(kOffSelectionPresent);
        r.wire_segment = "selection_present";
    } else if (f == "selection_convergence_root") {
        r.serialized_byte_offset = static_cast<int64_t>(kOffSelectionConvergenceRoot);
        r.wire_segment = "selection_convergence_root";
    } else if (f == "selection_has_winner") {
        r.serialized_byte_offset = static_cast<int64_t>(kOffSelectionHasWinner);
        r.wire_segment = "selection_has_winner";
    } else if (f == "selection_winner_index") {
        r.serialized_byte_offset = static_cast<int64_t>(kOffSelectionWinnerIndex);
        r.wire_segment = "selection_winner_index";
    } else if (f == "selection_winner_txid") {
        r.serialized_byte_offset = static_cast<int64_t>(kOffSelectionWinnerTxid);
        r.wire_segment = "selection_winner_txid";
    } else if (f == "selection_winner_weight") {
        r.serialized_byte_offset = static_cast<int64_t>(kOffSelectionWinnerWeight);
        r.wire_segment = "selection_winner_weight";
    } else if (f == "selection_candidate_count") {
        r.serialized_byte_offset = static_cast<int64_t>(kOffSelectionCandidateCount);
        r.wire_segment = "selection_candidate_count";
    } else if (f == "selection_sigmas_hash") {
        r.serialized_byte_offset = static_cast<int64_t>(kOffSelectionSigmasHash);
        r.wire_segment = "selection_sigmas_hash";
    } else if (f == "selection_weights_hash") {
        r.serialized_byte_offset = static_cast<int64_t>(kOffSelectionWeightsHash);
        r.wire_segment = "selection_weights_hash";
    } else if (f == "serialized_byte" || f == "wire_length") {
        // Offsets / segments already filled by DiffSerializedChainstateSummaries.
    } else {
        r.serialized_byte_offset = -1;
        r.wire_segment.clear();
    }
}


inline void WriteLE32(std::string& buf, uint32_t v)
{
    char b[4];
    for (int i = 0; i < 4; ++i) b[i] = static_cast<char>((v >> (8 * i)) & 0xFF);
    buf.append(b, 4);
}

inline void WriteLE64(std::string& buf, uint64_t v)
{
    char b[8];
    for (int i = 0; i < 8; ++i) b[i] = static_cast<char>((v >> (8 * i)) & 0xFF);
    buf.append(b, 8);
}

inline void WriteHash256(std::string& buf, const uint256& h)
{
    buf.append(reinterpret_cast<const char*>(h.begin()), 32);
}

inline void WriteByte(std::string& buf, uint8_t b)
{
    buf.push_back(static_cast<char>(b));
}

inline bool ReadLE32(const std::string& buf, size_t& pos, uint32_t& out)
{
    if (pos + 4 > buf.size()) return false;
    out = 0;
    for (int i = 0; i < 4; ++i) {
        out |= static_cast<uint32_t>(static_cast<unsigned char>(buf[pos + i])) << (8 * i);
    }
    pos += 4;
    return true;
}

inline bool ReadLE64(const std::string& buf, size_t& pos, uint64_t& out)
{
    if (pos + 8 > buf.size()) return false;
    out = 0;
    for (int i = 0; i < 8; ++i) {
        out |= static_cast<uint64_t>(static_cast<unsigned char>(buf[pos + i])) << (8 * i);
    }
    pos += 8;
    return true;
}

inline bool ReadHash256(const std::string& buf, size_t& pos, uint256& out)
{
    if (pos + 32 > buf.size()) return false;
    std::memcpy(out.begin(), buf.data() + pos, 32);
    pos += 32;
    return true;
}

inline bool ReadByte(const std::string& buf, size_t& pos, uint8_t& out)
{
    if (pos + 1 > buf.size()) return false;
    out = static_cast<uint8_t>(buf[pos]);
    pos += 1;
    return true;
}

//! Canonical leaf bytes for one (outpoint, coin) pair. Matches the layout
//! used by `state_snapshot.cpp` so the bucket MuHashes are derived from the
//! same canonical leaves as the authoritative `state_hash`.
DataStream MakeLeafBytes(const COutPoint& op, const Coin& coin)
{
    DataStream ss{};
    ss << op;
    ss << coin;
    return ss;
}

//! SHA256d over the canonical concatenation of a uint256 sequence.
uint256 HashUint256Sequence(const std::vector<uint256>& xs)
{
    HashWriter hw;
    for (const auto& x : xs) hw << x;
    return hw.GetHash();
}

//! SHA256d over the canonical concatenation of LE64-encoded uint64 sequence.
uint256 HashUint64SequenceLE(const std::vector<uint64_t>& xs)
{
    HashWriter hw;
    for (uint64_t v : xs) {
        // serialize.h emits uint64_t as little-endian fixed-width.
        hw << v;
    }
    return hw.GetHash();
}

//! Render a fixed-width decimal string of an integer for diff display.
//! Only the overloads actually used by the field comparators below are
//! retained. The `int64_t` overload was previously declared but had no
//! caller and tripped `-Wunused-function`; removing it is a no-op for
//! consumers (no field of `ChainstateSummary` is `int64_t`).
std::string DecStr(uint64_t v) { return strprintf("%llu", static_cast<unsigned long long>(v)); }
std::string DecStr(uint32_t v) { return strprintf("%u", v); }
std::string DecStr(int32_t v)  { return strprintf("%d", v); }
std::string DecStr(bool v)     { return v ? "true" : "false"; }

} // namespace

std::string ChainstateSummary::Serialize() const
{
    std::string out;
    out.reserve(kSerializedRecordSize);

    WriteLE32(out, kWireMagic);
    WriteLE32(out, static_cast<uint32_t>(height));
    WriteHash256(out, block_hash);
    WriteHash256(out, state_hash);
    WriteLE64(out, utxo_count);
    WriteLE64(out, res_balance);
    WriteLE64(out, other_balance);
    WriteLE32(out, mint_count);
    WriteLE32(out, lock_count);
    WriteLE32(out, converge_count);

    for (const auto& bh : utxo_bucket_hashes) {
        WriteHash256(out, bh);
    }

    WriteByte(out, selection_present ? 1u : 0u);
    WriteByte(out, selection_has_winner ? 1u : 0u);
    WriteLE32(out, static_cast<uint32_t>(selection_winner_index));
    WriteLE32(out, selection_candidate_count);
    WriteLE64(out, selection_winner_weight);
    WriteHash256(out, selection_convergence_root);
    WriteHash256(out, selection_winner_txid);
    WriteHash256(out, selection_sigmas_hash);
    WriteHash256(out, selection_weights_hash);

    // Constant size invariant: if this fires, kSerializedRecordSize is out
    // of sync with the Serialize() body. Both must be updated together.
    assert(out.size() == kSerializedRecordSize &&
           "ChainstateSummary::Serialize size mismatch");
    return out;
}

bool ChainstateSummary::Deserialize(const std::string& data)
{
    if (data.size() != kSerializedRecordSize) return false;

    size_t pos = 0;
    uint32_t magic = 0;
    if (!ReadLE32(data, pos, magic)) return false;
    if (magic != kWireMagic) return false;

    uint32_t h_le = 0;
    if (!ReadLE32(data, pos, h_le)) return false;
    height = static_cast<int32_t>(h_le);

    if (!ReadHash256(data, pos, block_hash)) return false;
    if (!ReadHash256(data, pos, state_hash)) return false;
    if (!ReadLE64(data, pos, utxo_count)) return false;
    if (!ReadLE64(data, pos, res_balance)) return false;
    if (!ReadLE64(data, pos, other_balance)) return false;
    if (!ReadLE32(data, pos, mint_count)) return false;
    if (!ReadLE32(data, pos, lock_count)) return false;
    if (!ReadLE32(data, pos, converge_count)) return false;

    for (auto& bh : utxo_bucket_hashes) {
        if (!ReadHash256(data, pos, bh)) return false;
    }

    uint8_t b = 0;
    if (!ReadByte(data, pos, b)) return false;
    selection_present = (b != 0);
    if (!ReadByte(data, pos, b)) return false;
    selection_has_winner = (b != 0);

    uint32_t widx_le = 0;
    if (!ReadLE32(data, pos, widx_le)) return false;
    selection_winner_index = static_cast<int32_t>(widx_le);

    if (!ReadLE32(data, pos, selection_candidate_count)) return false;
    if (!ReadLE64(data, pos, selection_winner_weight)) return false;
    if (!ReadHash256(data, pos, selection_convergence_root)) return false;
    if (!ReadHash256(data, pos, selection_winner_txid)) return false;
    if (!ReadHash256(data, pos, selection_sigmas_hash)) return false;
    if (!ReadHash256(data, pos, selection_weights_hash)) return false;

    return pos == data.size();
}

ChainstateSummary CaptureChainstateSummary(const CCoinsViewCache& view,
                                           const CBlockIndex* pindex,
                                           const SelectionContextView* selection)
{
    ChainstateSummary s;

    // Identity. We let ComputeDeterministicStateHash assert the
    // (view.GetBestBlock() == pindex->GetBlockHash()) invariant.
    if (pindex != nullptr) {
        s.height = pindex->nHeight;
        s.block_hash = pindex->GetBlockHash();
    }
    s.state_hash = Consensus::ComputeDeterministicStateHash(view, pindex);

    // Second cursor pass: 256-bucket MuHash + counters + balances.
    // O(1) memory: 256 small MuHash3072 accumulators (~768 bytes each
    // fully populated) plus a fixed leaf DataStream.
    std::array<MuHash3072, ChainstateSummary::kBucketCount> bucket_muhash{};

    std::unique_ptr<CCoinsViewCursor> pcursor(view.Cursor());
    if (pcursor) {
        while (pcursor->Valid()) {
            COutPoint key;
            Coin coin;
            if (pcursor->GetKey(key) && pcursor->GetValue(coin) && !coin.IsSpent()) {
                ++s.utxo_count;

                // Bucket by the MOST-SIGNIFICANT byte of the txid as stored.
                // `uint256::begin()` exposes the same byte order as on-disk
                // serialization, so bucket indices are byte-stable across
                // platforms.
                const uint8_t bucket = static_cast<uint8_t>(key.hash.begin()[0]);

                DataStream leaf = MakeLeafBytes(key, coin);
                bucket_muhash[bucket].Insert(MakeUCharSpan(leaf));

                // Diagnostic classification for bucket drilldown. The same
                // predicates are folded into STEP-0101 `state_hash` (v2) via
                // `ComputeDeterministicStateHash`; drift here changes both the
                // summary counters and the authoritative digest.
                const CScript& spk = coin.out.scriptPubKey;
                const bool is_res = IsResScript(spk);
                const bool is_convergence = IsConvergenceScript(spk);
                const bool is_v1_lock_shape =
                    spk.size() == 34 &&
                    static_cast<uint8_t>(spk[0]) == 0x51 &&
                    static_cast<uint8_t>(spk[1]) == 0x20;

                if (is_res) {
                    ++s.mint_count;
                    s.res_balance += static_cast<uint64_t>(coin.out.nValue);
                } else {
                    s.other_balance += static_cast<uint64_t>(coin.out.nValue);
                }
                if (is_v1_lock_shape) ++s.lock_count;
                if (is_convergence) ++s.converge_count;
            }
            pcursor->Next();
        }
    }

    for (size_t i = 0; i < ChainstateSummary::kBucketCount; ++i) {
        bucket_muhash[i].Finalize(s.utxo_bucket_hashes[i]);
    }

    // Selection context projection. The caller supplies a fully-formed
    // SelectionContextView; this module never reads blocks from disk.
    if (selection != nullptr) {
        assert(selection->sigmas.size() == selection->weights.size() &&
               "SelectionContextView: sigmas/weights size mismatch");
        assert((selection->winner_index < 0 ||
                static_cast<size_t>(selection->winner_index) < selection->sigmas.size()) &&
               "SelectionContextView: winner_index out of range");

        s.selection_present = true;
        s.selection_has_winner = selection->has_winner;
        s.selection_winner_index = selection->winner_index;
        s.selection_candidate_count = static_cast<uint32_t>(selection->sigmas.size());
        s.selection_winner_weight = selection->winner_weight;
        s.selection_convergence_root = selection->convergence_root;
        s.selection_winner_txid = selection->winner_txid;
        s.selection_sigmas_hash = HashUint256Sequence(selection->sigmas);
        s.selection_weights_hash = HashUint64SequenceLE(selection->weights);
    }

    return s;
}

DiffResult DiffChainstateSummaries(const ChainstateSummary& a, const ChainstateSummary& b)
{
    DiffResult r;
    r.identical = true;

#define DIFF_RETURN_SCALAR(name_str, expr_a, expr_b)                        \
    do {                                                                    \
        if ((expr_a) != (expr_b)) {                                         \
            r.identical = false;                                            \
            r.field = (name_str);                                           \
            r.value_a = DecStr(expr_a);                                     \
            r.value_b = DecStr(expr_b);                                     \
            AnnotateSerializedByteLocation(r);                               \
            return r;                                                       \
        }                                                                   \
    } while (0)

#define DIFF_RETURN_HASH(name_str, expr_a, expr_b)                          \
    do {                                                                    \
        if ((expr_a) != (expr_b)) {                                         \
            r.identical = false;                                            \
            r.field = (name_str);                                           \
            r.value_a = (expr_a).ToString();                                \
            r.value_b = (expr_b).ToString();                                \
            AnnotateSerializedByteLocation(r);                               \
            return r;                                                       \
        }                                                                   \
    } while (0)

    // ----- Coarse fields (1..9) ------------------------------------------
    DIFF_RETURN_SCALAR("height", a.height, b.height);
    DIFF_RETURN_HASH("block_hash", a.block_hash, b.block_hash);
    DIFF_RETURN_HASH("state_hash", a.state_hash, b.state_hash);
    DIFF_RETURN_SCALAR("utxo_count", a.utxo_count, b.utxo_count);
    DIFF_RETURN_SCALAR("res_balance", a.res_balance, b.res_balance);
    DIFF_RETURN_SCALAR("other_balance", a.other_balance, b.other_balance);
    DIFF_RETURN_SCALAR("mint_count", a.mint_count, b.mint_count);
    DIFF_RETURN_SCALAR("lock_count", a.lock_count, b.lock_count);
    DIFF_RETURN_SCALAR("converge_count", a.converge_count, b.converge_count);

    // ----- UTXO bucket drilldown (10) ------------------------------------
    // Reached only when all coarse fields agree; if state_hash itself was
    // unequal the comparison above would have already returned. Therefore
    // a divergence here would indicate either (i) inconsistency between the
    // captured state_hash and the bucket hashes -- which would itself be a
    // diagnostic-tool bug we want to surface -- or (ii) a node that
    // serialized a tampered summary. Either way the bucket index narrows
    // the search.
    for (size_t i = 0; i < ChainstateSummary::kBucketCount; ++i) {
        if (a.utxo_bucket_hashes[i] != b.utxo_bucket_hashes[i]) {
            r.identical = false;
            r.field = "utxo_bucket_hashes";
            r.value_a = a.utxo_bucket_hashes[i].ToString();
            r.value_b = b.utxo_bucket_hashes[i].ToString();
            r.bucket_index = static_cast<int32_t>(i);
            AnnotateSerializedByteLocation(r);
            return r;
        }
    }

    // ----- Selection-context fields (11..19) -----------------------------
    DIFF_RETURN_SCALAR("selection_present", a.selection_present, b.selection_present);
    // If neither summary captured a selection context, the remaining
    // selection_* fields are by construction zero/false on both sides.
    if (!a.selection_present) {
        return r; // identical
    }

    DIFF_RETURN_HASH("selection_convergence_root",
                     a.selection_convergence_root, b.selection_convergence_root);
    DIFF_RETURN_SCALAR("selection_has_winner",
                       a.selection_has_winner, b.selection_has_winner);
    DIFF_RETURN_SCALAR("selection_winner_index",
                       a.selection_winner_index, b.selection_winner_index);
    DIFF_RETURN_HASH("selection_winner_txid",
                     a.selection_winner_txid, b.selection_winner_txid);
    DIFF_RETURN_SCALAR("selection_winner_weight",
                       a.selection_winner_weight, b.selection_winner_weight);
    DIFF_RETURN_SCALAR("selection_candidate_count",
                       a.selection_candidate_count, b.selection_candidate_count);
    DIFF_RETURN_HASH("selection_sigmas_hash",
                     a.selection_sigmas_hash, b.selection_sigmas_hash);
    DIFF_RETURN_HASH("selection_weights_hash",
                     a.selection_weights_hash, b.selection_weights_hash);

#undef DIFF_RETURN_SCALAR
#undef DIFF_RETURN_HASH

    return r;
}

DiffResult DiffSerializedChainstateSummaries(std::string_view wire_a,
                                             std::string_view wire_b)
{
    DiffResult r;
    r.identical = true;
    const size_t n = std::min(wire_a.size(), wire_b.size());
    for (size_t i = 0; i < n; ++i) {
        if (static_cast<unsigned char>(wire_a[i]) != static_cast<unsigned char>(wire_b[i])) {
            r.identical = false;
            r.field = "serialized_byte";
            r.serialized_byte_offset = static_cast<int64_t>(i);
            r.wire_segment = DescribeWireSegmentAt(i);
            r.value_a = strprintf("%02x", static_cast<unsigned int>(static_cast<unsigned char>(wire_a[i])));
            r.value_b = strprintf("%02x", static_cast<unsigned int>(static_cast<unsigned char>(wire_b[i])));
            r.bucket_index = -1;
            return r;
        }
    }
    if (wire_a.size() != wire_b.size()) {
        r.identical = false;
        r.field = "wire_length";
        r.serialized_byte_offset = static_cast<int64_t>(n);
        r.wire_segment = DescribeWireSegmentAt(n);
        r.value_a = strprintf("%llu", static_cast<unsigned long long>(wire_a.size()));
        r.value_b = strprintf("%llu", static_cast<unsigned long long>(wire_b.size()));
        r.bucket_index = -1;
        return r;
    }
    return r;
}

std::string DiffResult::ToString() const
{
    if (identical) return "ChainstateSummary: identical";
    const std::string loc =
        (serialized_byte_offset >= 0)
            ? strprintf(" @wire_byte=%lld (%s)",
                        static_cast<long long>(serialized_byte_offset),
                        wire_segment.empty() ? "?" : wire_segment.c_str())
            : std::string{};
    if (bucket_index >= 0) {
        return strprintf(
            "ChainstateSummary divergence: field=%s bucket=%d a=%s b=%s%s",
            field, bucket_index, value_a, value_b, loc);
    }
    return strprintf(
        "ChainstateSummary divergence: field=%s a=%s b=%s%s",
        field, value_a, value_b, loc);
}

} // namespace Consensus
