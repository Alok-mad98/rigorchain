// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/replay_snapshot.h>

#include <chain.h>
#include <coins.h>
#include <logging.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <tinyformat.h>
#include <uint256.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>

namespace Consensus {

namespace {

//! Fixed serialized record length: magic(4) + height(4) + block_hash(32) +
//! state_hash(32) + best_block_hash(32) + utxo_count(8) + mint_count(4) +
//! lock_count(4) + converge_count(4) + res_balance(8) + other_balance(8).
constexpr size_t kSerializedRecordSize =
    4 + 4 + 32 + 32 + 32 + 8 + 4 + 4 + 4 + 8 + 8;

inline void WriteLE32(std::string& buf, uint32_t v)
{
    char b[4] = {
        static_cast<char>((v >> 0) & 0xFF),
        static_cast<char>((v >> 8) & 0xFF),
        static_cast<char>((v >> 16) & 0xFF),
        static_cast<char>((v >> 24) & 0xFF),
    };
    buf.append(b, 4);
}

inline void WriteLE64(std::string& buf, uint64_t v)
{
    char b[8];
    for (int i = 0; i < 8; ++i) {
        b[i] = static_cast<char>((v >> (8 * i)) & 0xFF);
    }
    buf.append(b, 8);
}

inline void WriteHash256(std::string& buf, const uint256& h)
{
    buf.append(reinterpret_cast<const char*>(h.begin()), 32);
}

inline bool ReadLE32(const std::string& buf, size_t& pos, uint32_t& out)
{
    if (pos + 4 > buf.size()) return false;
    out = static_cast<uint32_t>(static_cast<unsigned char>(buf[pos + 0])) |
          (static_cast<uint32_t>(static_cast<unsigned char>(buf[pos + 1])) << 8) |
          (static_cast<uint32_t>(static_cast<unsigned char>(buf[pos + 2])) << 16) |
          (static_cast<uint32_t>(static_cast<unsigned char>(buf[pos + 3])) << 24);
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

//! Witness-V1 lock-program shape per RFC-003 §5: exactly `OP_1 0x20 <32>`.
//! This matches the SegWit V1 witness program prefix, which in RIGOR is the
//! container for both RES (`ORB2`) and ORU-convergence (`CVG2`) outputs.
//! Counting these UTXOs is informational only.
inline bool HasV1LockProgramShape(const CScript& spk)
{
    return spk.size() == 34 &&
           static_cast<uint8_t>(spk[0]) == 0x51 /*OP_1*/ &&
           static_cast<uint8_t>(spk[1]) == 0x20 /*push 32*/;
}

} // namespace

std::string ReplaySnapshot::Serialize() const
{
    std::string out;
    out.reserve(kSerializedRecordSize);

    WriteLE32(out, kWireMagic);
    WriteLE32(out, static_cast<uint32_t>(height));
    WriteHash256(out, block_hash);
    WriteHash256(out, state_hash);
    WriteHash256(out, best_block_hash);
    WriteLE64(out, utxo_count);
    WriteLE32(out, mint_count);
    WriteLE32(out, lock_count);
    WriteLE32(out, converge_count);
    WriteLE64(out, res_balance);
    WriteLE64(out, other_balance);

    return out;
}

bool ReplaySnapshot::Deserialize(const std::string& data)
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
    if (!ReadHash256(data, pos, best_block_hash)) return false;
    if (!ReadLE64(data, pos, utxo_count)) return false;
    if (!ReadLE32(data, pos, mint_count)) return false;
    if (!ReadLE32(data, pos, lock_count)) return false;
    if (!ReadLE32(data, pos, converge_count)) return false;
    if (!ReadLE64(data, pos, res_balance)) return false;
    if (!ReadLE64(data, pos, other_balance)) return false;

    return pos == data.size();
}

ReplaySnapshot CaptureReplaySnapshot(const CCoinsViewCache& view,
                                     const CBlockIndex* pindex,
                                     const uint256& state_hash)
{
    ReplaySnapshot snap;
    snap.state_hash = state_hash;

    if (pindex != nullptr) {
        snap.height = pindex->nHeight;
        snap.block_hash = pindex->GetBlockHash();
        snap.best_block_hash = pindex->GetBlockHash();
    }

    // Single streaming cursor pass. Diagnostic counters are accumulated
    // incrementally; the UTXO set is never materialized in RAM.
    std::unique_ptr<CCoinsViewCursor> pcursor(view.Cursor());
    if (pcursor) {
        while (pcursor->Valid()) {
            COutPoint key;
            Coin coin;
            if (pcursor->GetKey(key) && pcursor->GetValue(coin) && !coin.IsSpent()) {
                ++snap.utxo_count;

                const CScript& spk = coin.out.scriptPubKey;
                const bool is_res = IsResScript(spk);
                const bool is_convergence = IsConvergenceScript(spk);
                const bool is_v1_lock_shape = HasV1LockProgramShape(spk);

                if (is_res) {
                    ++snap.mint_count;
                    snap.res_balance += static_cast<uint64_t>(coin.out.nValue);
                } else {
                    snap.other_balance += static_cast<uint64_t>(coin.out.nValue);
                }

                if (is_v1_lock_shape) {
                    ++snap.lock_count;
                }

                if (is_convergence) {
                    ++snap.converge_count;
                }
            }
            pcursor->Next();
        }
    }

    // Chainstate invariant: convergence outputs are dropped by `AddCoins` and
    // must never appear in the UTXO set. A non-zero count here indicates a
    // chainstate-integrity bug (not a snapshot bug); log loudly. We do NOT
    // abort: the snapshot is diagnostic and the caller needs to see the
    // anomaly captured in the record.
    if (snap.converge_count != 0) {
        LogPrintf("ReplaySnapshot: WARNING converge_count=%u at height=%d hash=%s "
                  "(chainstate invariant violated; convergence outputs should be "
                  "filtered by AddCoins)\n",
                  snap.converge_count,
                  snap.height,
                  snap.block_hash.ToString());
    }

    return snap;
}

std::string CompareReplaySnapshots(const ReplaySnapshot& a, const ReplaySnapshot& b)
{
    std::ostringstream r;

    if (a.height != b.height) {
        r << strprintf("Height mismatch: %d vs %d", a.height, b.height);
        return r.str();
    }
    if (a.block_hash != b.block_hash) {
        r << strprintf("Block hash mismatch: %s vs %s",
                       a.block_hash.ToString(), b.block_hash.ToString());
        return r.str();
    }
    if (a.state_hash != b.state_hash) {
        r << strprintf("State hash mismatch: %s vs %s",
                       a.state_hash.ToString(), b.state_hash.ToString());
        return r.str();
    }
    if (a.best_block_hash != b.best_block_hash) {
        r << strprintf("Best block hash mismatch: %s vs %s",
                       a.best_block_hash.ToString(), b.best_block_hash.ToString());
        return r.str();
    }
    if (a.utxo_count != b.utxo_count) {
        r << strprintf("UTXO count mismatch: %llu vs %llu",
                       static_cast<unsigned long long>(a.utxo_count),
                       static_cast<unsigned long long>(b.utxo_count));
        return r.str();
    }
    if (a.res_balance != b.res_balance) {
        r << strprintf("RES balance mismatch: %llu vs %llu",
                       static_cast<unsigned long long>(a.res_balance),
                       static_cast<unsigned long long>(b.res_balance));
        return r.str();
    }
    if (a.other_balance != b.other_balance) {
        r << strprintf("Other-asset balance mismatch: %llu vs %llu",
                       static_cast<unsigned long long>(a.other_balance),
                       static_cast<unsigned long long>(b.other_balance));
        return r.str();
    }
    if (a.mint_count != b.mint_count) {
        r << strprintf("Mint (RES-UTXO) count mismatch: %u vs %u",
                       a.mint_count, b.mint_count);
        return r.str();
    }
    if (a.lock_count != b.lock_count) {
        r << strprintf("Lock (V1-shape) count mismatch: %u vs %u",
                       a.lock_count, b.lock_count);
        return r.str();
    }
    if (a.converge_count != b.converge_count) {
        r << strprintf("Converge count mismatch: %u vs %u",
                       a.converge_count, b.converge_count);
        return r.str();
    }

    return "Snapshots are identical";
}

} // namespace Consensus
