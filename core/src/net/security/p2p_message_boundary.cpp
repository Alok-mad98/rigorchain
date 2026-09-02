// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-076 P2P security boundary implementation.
// Wire firewall only. No consensus formulas. No chainstate. No Kernel.

#include <net/security/p2p_message_boundary.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace {

class ByteReader
{
    const uint8_t* m_p;
    const uint8_t* m_end;
    bool m_ok{true};

public:
    ByteReader(const uint8_t* data, size_t size)
        : m_p(data), m_end(data + size) {}

    bool Ok() const { return m_ok; }
    size_t Remaining() const { return m_ok ? static_cast<size_t>(m_end - m_p) : 0; }
    const uint8_t* Cursor() const { return m_p; }

    bool Read(void* dst, size_t n)
    {
        if (!m_ok || n > Remaining()) {
            m_ok = false;
            return false;
        }
        if (n && dst) std::memcpy(dst, m_p, n);
        m_p += n;
        return true;
    }

    bool Skip(size_t n) { return Read(nullptr, n); }

    bool U8(uint8_t& v) { return Read(&v, 1); }

    bool U16LE(uint16_t& v)
    {
        uint8_t b[2];
        if (!Read(b, 2)) return false;
        v = static_cast<uint16_t>(b[0] | (static_cast<uint16_t>(b[1]) << 8));
        return true;
    }

    bool U32LE(uint32_t& v)
    {
        uint8_t b[4];
        if (!Read(b, 4)) return false;
        v = static_cast<uint32_t>(b[0]) |
            (static_cast<uint32_t>(b[1]) << 8) |
            (static_cast<uint32_t>(b[2]) << 16) |
            (static_cast<uint32_t>(b[3]) << 24);
        return true;
    }

    bool I32LE(int32_t& v)
    {
        uint32_t u = 0;
        if (!U32LE(u)) return false;
        v = static_cast<int32_t>(u);
        return true;
    }

    bool I64LE(int64_t& v)
    {
        uint8_t b[8];
        if (!Read(b, 8)) return false;
        uint64_t u = 0;
        for (int i = 0; i < 8; ++i) u |= static_cast<uint64_t>(b[i]) << (8 * i);
        v = static_cast<int64_t>(u);
        return true;
    }

    bool CompactSize(uint64_t& n)
    {
        uint8_t ch = 0;
        if (!U8(ch)) return false;
        if (ch < 253) {
            n = ch;
            return true;
        }
        if (ch == 253) {
            uint16_t v = 0;
            if (!U16LE(v)) return false;
            if (v < 253) {
                m_ok = false;
                return false;
            }
            n = v;
            return true;
        }
        if (ch == 254) {
            uint32_t v = 0;
            if (!U32LE(v)) return false;
            if (v < 0x10000u) {
                m_ok = false;
                return false;
            }
            n = v;
            return true;
        }
        uint8_t b[8];
        if (!Read(b, 8)) return false;
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(b[i]) << (8 * i);
        if (v < 0x100000000ULL) {
            m_ok = false;
            return false;
        }
        n = v;
        return true;
    }

    bool Bytes(std::vector<uint8_t>& out, uint64_t n)
    {
        if (!m_ok || n > Remaining()) {
            m_ok = false;
            return false;
        }
        out.assign(m_p, m_p + static_cast<size_t>(n));
        m_p += static_cast<size_t>(n);
        return true;
    }
};

bool CmdEq(const std::string& a, const char* b)
{
    return a == b;
}

P2PValidationResult PeerAbuse(const P2PSecurityInput& in)
{
    if (in.messages_in_window > P2PSecurityLimits::MAX_MESSAGES_PER_WINDOW) {
        return P2PValidationResult::Ban("excessive-messages");
    }
    if (in.invalid_blocks_from_peer >= P2PSecurityLimits::MAX_INVALID_BLOCKS_BEFORE_BAN) {
        return P2PValidationResult::Ban("repeated-invalid-blocks");
    }
    if (in.invalid_tx_from_peer >= P2PSecurityLimits::MAX_INVALID_TX_BEFORE_BAN) {
        return P2PValidationResult::Ban("repeated-invalid-tx");
    }
    return P2PValidationResult::Accept(P2PAdmissionTarget::OTHER, "peer-ok");
}

bool ReadScriptBytes(ByteReader& r, uint32_t max_script, const char*& fail)
{
    uint64_t n = 0;
    if (!r.CompactSize(n)) {
        fail = "malformed-script-size";
        return false;
    }
    if (n > max_script) {
        fail = "oversized-script-data";
        return false;
    }
    if (!r.Skip(static_cast<size_t>(n))) {
        fail = "truncated-script";
        return false;
    }
    return true;
}

bool ReadTxIn(ByteReader& r, const char*& fail)
{
    if (!r.Skip(36)) { // prevout: 32-byte hash + 4-byte n
        fail = "malformed-txin-prevout";
        return false;
    }
    if (!ReadScriptBytes(r, P2PSecurityLimits::MAX_SCRIPT_SIZE, fail)) return false;
    uint32_t seq = 0;
    if (!r.U32LE(seq)) {
        fail = "malformed-txin-sequence";
        return false;
    }
    return true;
}

bool ReadTxOut(ByteReader& r, const char*& fail)
{
    int64_t value = 0;
    if (!r.I64LE(value)) {
        fail = "malformed-txout-value";
        return false;
    }
    if (!ReadScriptBytes(r, P2PSecurityLimits::MAX_SCRIPT_SIZE, fail)) return false;
    return true;
}

bool ReadWitnessStack(ByteReader& r, const char*& fail)
{
    uint64_t n_items = 0;
    if (!r.CompactSize(n_items)) {
        fail = "malformed-witness";
        return false;
    }
    if (n_items > 10000) {
        fail = "invalid-witness";
        return false;
    }
    for (uint64_t i = 0; i < n_items; ++i) {
        uint64_t n = 0;
        if (!r.CompactSize(n)) {
            fail = "malformed-witness";
            return false;
        }
        if (n > P2PSecurityLimits::MAX_SCRIPT_ELEMENT_SIZE) {
            fail = "invalid-witness";
            return false;
        }
        if (!r.Skip(static_cast<size_t>(n))) {
            fail = "malformed-witness";
            return false;
        }
    }
    return true;
}

struct ParsedTx {
    int32_t nVersion{0};
    uint64_t vin{0};
    uint64_t vout{0};
    bool has_witness{false};
    const uint8_t* begin{nullptr};
    const uint8_t* end{nullptr};
};

bool ParseOneTx(ByteReader& r, ParsedTx& tx, const char*& fail)
{
    tx.begin = r.Cursor();
    if (!r.I32LE(tx.nVersion)) {
        fail = "malformed-transaction";
        return false;
    }

    uint64_t first = 0;
    if (!r.CompactSize(first)) {
        fail = "malformed-transaction";
        return false;
    }

    uint8_t flags = 0;
    if (first == 0) {
        if (!r.U8(flags)) {
            fail = "malformed-transaction";
            return false;
        }
        if (flags == 0) {
            fail = "empty-input";
            return false;
        }
        if (!r.CompactSize(tx.vin)) {
            fail = "malformed-transaction-vector";
            return false;
        }
        if (tx.vin == 0) {
            fail = "empty-input";
            return false;
        }
        for (uint64_t i = 0; i < tx.vin; ++i) {
            if (!ReadTxIn(r, fail)) return false;
        }
        if (!r.CompactSize(tx.vout)) {
            fail = "malformed-transaction-vector";
            return false;
        }
        if (tx.vout == 0) {
            fail = "empty-output";
            return false;
        }
        for (uint64_t i = 0; i < tx.vout; ++i) {
            if (!ReadTxOut(r, fail)) return false;
        }
        if (flags & 1) {
            tx.has_witness = true;
            flags = static_cast<uint8_t>(flags ^ 1);
            bool any_witness = false;
            for (uint64_t i = 0; i < tx.vin; ++i) {
                const uint8_t* before = r.Cursor();
                if (!ReadWitnessStack(r, fail)) return false;
                // CompactSize 0 encodes as a single 0x00 — empty stack / IsNull.
                if (!(r.Cursor() == before + 1 && *before == 0)) {
                    any_witness = true;
                }
            }
            if (!any_witness) {
                fail = "invalid-witness";
                return false;
            }
        }
        if (flags) {
            fail = "malformed-transaction";
            return false;
        }
    } else {
        tx.vin = first;
        if (tx.vin == 0) {
            fail = "empty-input";
            return false;
        }
        for (uint64_t i = 0; i < tx.vin; ++i) {
            if (!ReadTxIn(r, fail)) return false;
        }
        if (!r.CompactSize(tx.vout)) {
            fail = "malformed-transaction-vector";
            return false;
        }
        if (tx.vout == 0) {
            fail = "empty-output";
            return false;
        }
        for (uint64_t i = 0; i < tx.vout; ++i) {
            if (!ReadTxOut(r, fail)) return false;
        }
    }

    uint32_t locktime = 0;
    if (!r.U32LE(locktime)) {
        fail = "malformed-transaction";
        return false;
    }
    tx.end = r.Cursor();
    return true;
}

bool HeaderNull(const uint8_t* p, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        if (p[i] != 0) return false;
    }
    return true;
}

P2PValidationResult EvalVersion(const uint8_t* data, size_t size)
{
    if (size < 4) return P2PValidationResult::Reject("handshake-malformed");
    int32_t nVersion = 0;
    ByteReader r(data, size);
    if (!r.I32LE(nVersion)) return P2PValidationResult::Reject("handshake-malformed");
    if (nVersion == P2PSecurityLimits::BITCOIN_SERIALIZATION_VERSION) {
        return P2PValidationResult::Ban("version-negotiation-abuse");
    }
    if (nVersion < P2PSecurityLimits::PROTOCOL_MIN_VERSION ||
        nVersion > P2PSecurityLimits::PROTOCOL_CURRENT_VERSION) {
        return P2PValidationResult::Ban("version-negotiation-abuse");
    }
    if (size < 80) return P2PValidationResult::Reject("handshake-malformed");
    return P2PValidationResult::Accept(P2PAdmissionTarget::HANDSHAKE, "version-ok");
}

P2PValidationResult EvalTx(const uint8_t* data, size_t size, const P2PSecurityInput* ctx)
{
    if (size == 0) return P2PValidationResult::Reject("malformed-transaction");
    if (size > P2PSecurityLimits::MAX_PROTOCOL_MESSAGE_LENGTH) {
        return P2PValidationResult::Reject("invalid-size");
    }
    ByteReader r(data, size);
    ParsedTx tx;
    const char* fail = "malformed-transaction";
    if (!ParseOneTx(r, tx, fail)) return P2PValidationResult::Reject(fail);
    if (r.Remaining() != 0) return P2PValidationResult::Reject("malformed-transaction");

    const uint64_t key = P2PMessageBoundary::WireFingerprint(tx.begin,
        static_cast<size_t>(tx.end - tx.begin));
    if (ctx && ctx->seen_tx_keys.count(key)) {
        return P2PValidationResult::Reject("duplicate-txid");
    }

    // Structural accept only. Signature / asset / replay-at-consensus stay
    // on VerifyRigorPredicate / CheckTransaction. P2P does not verify them.
    return P2PValidationResult::Accept(P2PAdmissionTarget::TX_ADMISSION,
                                       "tx-accept-to-validation",
                                       /*spend_predicate=*/true);
}

P2PValidationResult EvalBlock(const uint8_t* data, size_t size, const P2PSecurityInput* ctx)
{
    if (size > P2PSecurityLimits::MAX_PROTOCOL_MESSAGE_LENGTH) {
        return P2PValidationResult::Reject("invalid-size");
    }
    if (size > P2PSecurityLimits::MAX_BLOCK_SERIALIZED_SIZE) {
        return P2PValidationResult::Reject("oversized-block");
    }
    if (size < P2PSecurityLimits::BLOCK_HEADER_SIZE) {
        return P2PValidationResult::Reject("malformed-header");
    }

    ByteReader r(data, size);
    int32_t nVersion = 0;
    uint8_t prev[32];
    uint8_t merkle[32];
    uint32_t nTime = 0;
    uint32_t nBits = 0;
    uint32_t nNonce = 0;
    if (!r.I32LE(nVersion) || !r.Read(prev, 32) || !r.Read(merkle, 32) ||
        !r.U32LE(nTime) || !r.U32LE(nBits) || !r.U32LE(nNonce)) {
        return P2PValidationResult::Reject("malformed-header");
    }

    if (nVersion <= 0) return P2PValidationResult::Reject("unknown-version");
    if (nTime == 0) return P2PValidationResult::Reject("invalid-timestamp");
    if (nBits == 0) return P2PValidationResult::Reject("invalid-nbits");
    // All-zero prev is legal only as genesis; P2P cannot see chainstate.
    // Truncation is already malformed-header. A 32-byte field is always
    // "valid length"; consensus decides whether the parent exists.

    const uint64_t block_key = P2PMessageBoundary::WireFingerprint(data, 80);
    if (ctx && ctx->seen_block_keys.count(block_key)) {
        return P2PValidationResult::Reject("duplicate-block");
    }

    if (r.Remaining() == 0) return P2PValidationResult::Reject("empty-block");

    uint64_t tx_count = 0;
    if (!r.CompactSize(tx_count)) {
        return P2PValidationResult::Reject("malformed-transaction-vector");
    }
    if (tx_count == 0) return P2PValidationResult::Reject("empty-block");
    if (tx_count > (P2PSecurityLimits::MAX_BLOCK_SERIALIZED_SIZE / P2PSecurityLimits::MIN_TX_SERIALIZED)) {
        return P2PValidationResult::Reject("oversized-block");
    }
    if (tx_count > r.Remaining()) {
        return P2PValidationResult::Reject("malformed-transaction-vector");
    }

    if (HeaderNull(merkle, 32)) {
        return P2PValidationResult::Reject("invalid-merkle-root");
    }

    std::vector<uint64_t> tx_keys;
    tx_keys.reserve(static_cast<size_t>(std::min<uint64_t>(tx_count, 1024)));
    for (uint64_t i = 0; i < tx_count; ++i) {
        ParsedTx tx;
        const char* fail = "malformed-transaction-vector";
        if (!ParseOneTx(r, tx, fail)) {
            return P2PValidationResult::Reject(fail);
        }
        const uint64_t k = P2PMessageBoundary::WireFingerprint(tx.begin,
            static_cast<size_t>(tx.end - tx.begin));
        for (uint64_t prevk : tx_keys) {
            if (prevk == k) return P2PValidationResult::Reject("duplicated-transaction");
        }
        tx_keys.push_back(k);
    }
    if (r.Remaining() != 0) {
        return P2PValidationResult::Reject("malformed-transaction-vector");
    }

    return P2PValidationResult::Accept(P2PAdmissionTarget::BLOCK_ADMISSION,
                                       "block-accept-to-validation");
}

} // namespace

uint64_t P2PMessageBoundary::WireFingerprint(const uint8_t* data, size_t size)
{
    // FNV-1a 64. Network cache key only — not HASH256, not a consensus id.
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < size; ++i) {
        h ^= data[i];
        h *= 1099511628211ULL;
    }
    return h;
}

P2PValidationResult P2PMessageBoundary::EvaluateBlock(const uint8_t* data, size_t size,
                                                      const P2PSecurityInput* ctx)
{
    return EvalBlock(data, size, ctx);
}

P2PValidationResult P2PMessageBoundary::EvaluateTx(const uint8_t* data, size_t size,
                                                   const P2PSecurityInput* ctx)
{
    return EvalTx(data, size, ctx);
}

P2PValidationResult P2PMessageBoundary::EvaluatePeer(const P2PSecurityInput& in)
{
    return PeerAbuse(in);
}

P2PValidationResult P2PMessageBoundary::Evaluate(const P2PSecurityInput& in)
{
    const P2PValidationResult abuse = PeerAbuse(in);
    if (abuse.IsBan()) return abuse;

    const uint64_t claimed = in.claimed_size != 0 ? in.claimed_size : in.payload.size();
    if (claimed > P2PSecurityLimits::MAX_PROTOCOL_MESSAGE_LENGTH) {
        return P2PValidationResult::Reject("invalid-size");
    }

    const uint8_t* data = in.payload.empty() ? nullptr : in.payload.data();
    const size_t size = in.payload.size();

    if (!in.handshake_complete && !CmdEq(in.command, "version") &&
        !CmdEq(in.command, "verack") && !CmdEq(in.command, "wtxidrelay") &&
        !CmdEq(in.command, "sendaddrv2") && !CmdEq(in.command, "rigorcap")) {
        return P2PValidationResult::Reject("handshake-malformed");
    }

    if (CmdEq(in.command, "version")) {
        return EvalVersion(data, size);
    }
    if (CmdEq(in.command, "block")) {
        return EvalBlock(data, size, &in);
    }
    if (CmdEq(in.command, "tx")) {
        return EvalTx(data, size, &in);
    }

    // Other commands: size already gated. Do not invent a second protocol.
    return P2PValidationResult::Accept(P2PAdmissionTarget::OTHER, "pass-through");
}
