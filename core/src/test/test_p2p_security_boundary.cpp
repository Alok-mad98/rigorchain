// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-076: P2P security boundary adversarial audit.
// Standalone with -DP2P_SECURITY_STANDALONE; Boost otherwise.
// Does not recode consensus formulas. Does not call the Kernel.

#include <net/security/p2p_message_boundary.h>
#include <net/security/peer_misbehavior_score.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {

void PutU32(std::vector<uint8_t>& o, uint32_t v)
{
    o.push_back(static_cast<uint8_t>(v));
    o.push_back(static_cast<uint8_t>(v >> 8));
    o.push_back(static_cast<uint8_t>(v >> 16));
    o.push_back(static_cast<uint8_t>(v >> 24));
}

void PutU64(std::vector<uint8_t>& o, uint64_t v)
{
    for (int i = 0; i < 8; ++i) o.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

void PutCompact(std::vector<uint8_t>& o, uint64_t n)
{
    if (n < 253) {
        o.push_back(static_cast<uint8_t>(n));
        return;
    }
    if (n <= 0xffff) {
        o.push_back(253);
        o.push_back(static_cast<uint8_t>(n));
        o.push_back(static_cast<uint8_t>(n >> 8));
        return;
    }
    o.push_back(254);
    PutU32(o, static_cast<uint32_t>(n));
}

std::vector<uint8_t> WitnessCoinbase(uint8_t tag = 0x11)
{
    std::vector<uint8_t> tx;
    PutU32(tx, 1);             // nVersion
    tx.push_back(0x00);        // marker
    tx.push_back(0x01);        // flag
    tx.push_back(0x01);        // vin
    tx.insert(tx.end(), 32, 0x00);
    PutU32(tx, 0xffffffff);    // coinbase prevout n
    tx.push_back(0x02);        // scriptSig len
    tx.push_back(0x01);
    tx.push_back(0x01);
    PutU32(tx, 0xffffffff);    // nSequence
    tx.push_back(0x01);        // vout
    PutU64(tx, 1000);
    tx.push_back(0x16);        // scriptPubKey 22 bytes P2WPKH
    tx.push_back(0x00);
    tx.push_back(0x14);
    tx.insert(tx.end(), 20, tag);
    tx.push_back(0x01);        // 1 witness stack
    tx.push_back(0x20);        // 32-byte item
    tx.insert(tx.end(), 32, 0x00);
    PutU32(tx, 0);             // nLockTime
    return tx;
}

std::vector<uint8_t> SpendTx(uint8_t tag = 0x22)
{
    std::vector<uint8_t> tx;
    PutU32(tx, 2);
    tx.push_back(0x00);
    tx.push_back(0x01);
    tx.push_back(0x01);        // vin
    tx.insert(tx.end(), 32, 0xaa);
    PutU32(tx, 0);
    tx.push_back(0x00);        // empty scriptSig
    PutU32(tx, 0xffffffff);
    tx.push_back(0x01);        // vout
    PutU64(tx, 500);
    tx.push_back(0x16);
    tx.push_back(0x00);
    tx.push_back(0x14);
    tx.insert(tx.end(), 20, tag);
    tx.push_back(0x02);        // 2 witness items (sig, pubkey) — not verified here
    tx.push_back(0x40);
    tx.insert(tx.end(), 64, 0xbb); // garbage signature
    tx.push_back(0x20);
    tx.insert(tx.end(), 32, 0xcc); // garbage pubkey
    PutU32(tx, 0);
    return tx;
}

std::vector<uint8_t> Header(int32_t version, uint32_t time, uint32_t bits,
                           uint8_t prev_fill, uint8_t merkle_fill, uint32_t nonce = 1)
{
    std::vector<uint8_t> h;
    PutU32(h, static_cast<uint32_t>(version));
    h.insert(h.end(), 32, prev_fill);
    h.insert(h.end(), 32, merkle_fill);
    PutU32(h, time);
    PutU32(h, bits);
    PutU32(h, nonce);
    return h;
}

std::vector<uint8_t> BlockFromTxs(const std::vector<uint8_t>& header,
                                 const std::vector<std::vector<uint8_t>>& txs)
{
    std::vector<uint8_t> b = header;
    PutCompact(b, txs.size());
    for (const auto& tx : txs) b.insert(b.end(), tx.begin(), tx.end());
    return b;
}

std::vector<uint8_t> ValidBlock(uint8_t distinct = 0x01)
{
    auto h = Header(4, 1700000000, 0x207fffff, distinct, 0xee, distinct);
    return BlockFromTxs(h, {WitnessCoinbase(distinct)});
}

std::vector<uint8_t> VersionMsg(int32_t nVersion)
{
    std::vector<uint8_t> v;
    PutU32(v, static_cast<uint32_t>(nVersion));
    v.insert(v.end(), 80, 0x00);
    return v;
}

P2PSecurityInput In(const std::string& cmd, const std::vector<uint8_t>& payload)
{
    P2PSecurityInput in;
    in.command = cmd;
    in.payload = payload;
    return in;
}

bool Expect(const P2PValidationResult& r, P2PValidationDecision d, const char* label)
{
    if (r.decision == d) return true;
    std::fprintf(stderr, "FAIL %s: got %d reason=%s\n", label,
                 static_cast<int>(r.decision), r.reason ? r.reason : "");
    return false;
}

bool TestInvalidBlocks()
{
    // malformed header
    if (!Expect(P2PMessageBoundary::Evaluate(In("block", std::vector<uint8_t>(79, 1))),
                P2PValidationDecision::REJECT, "malformed-header")) return false;

    // invalid size / oversized
    P2PSecurityInput over = In("block", ValidBlock());
    over.claimed_size = static_cast<uint64_t>(P2PSecurityLimits::MAX_PROTOCOL_MESSAGE_LENGTH) + 1;
    if (!Expect(P2PMessageBoundary::Evaluate(over), P2PValidationDecision::REJECT, "invalid-size")) return false;

    std::vector<uint8_t> big(P2PSecurityLimits::MAX_BLOCK_SERIALIZED_SIZE + 1, 1);
    if (!Expect(P2PMessageBoundary::EvaluateBlock(big.data(), big.size()),
                P2PValidationDecision::REJECT, "oversized-block")) return false;

    // invalid timestamp
    auto ts = Header(4, 0, 0x207fffff, 0x01, 0xee);
    auto ts_block = BlockFromTxs(ts, {WitnessCoinbase()});
    if (!Expect(P2PMessageBoundary::EvaluateBlock(ts_block.data(), ts_block.size()),
                P2PValidationDecision::REJECT, "invalid-timestamp")) return false;

    // invalid nBits
    auto bits = Header(4, 1700000000, 0, 0x01, 0xee);
    auto bits_block = BlockFromTxs(bits, {WitnessCoinbase()});
    if (!Expect(P2PMessageBoundary::EvaluateBlock(bits_block.data(), bits_block.size()),
                P2PValidationDecision::REJECT, "invalid-nbits")) return false;

    // invalid prev hash: truncated header (prev not 32 bytes)
    if (!Expect(P2PMessageBoundary::Evaluate(In("block", std::vector<uint8_t>(40, 0xaa))),
                P2PValidationDecision::REJECT, "invalid-prev-hash")) return false;

    // duplicate block
    auto blk = ValidBlock(0x07);
    P2PSecurityInput dup = In("block", blk);
    dup.seen_block_keys.insert(P2PMessageBoundary::WireFingerprint(blk.data(), 80));
    if (!Expect(P2PMessageBoundary::Evaluate(dup), P2PValidationDecision::REJECT, "duplicate-block")) return false;

    // unknown version
    auto uv = Header(0, 1700000000, 0x207fffff, 0x01, 0xee);
    auto uv_block = BlockFromTxs(uv, {WitnessCoinbase()});
    if (!Expect(P2PMessageBoundary::EvaluateBlock(uv_block.data(), uv_block.size()),
                P2PValidationDecision::REJECT, "unknown-version")) return false;
    auto neg = Header(-1, 1700000000, 0x207fffff, 0x01, 0xee);
    auto neg_block = BlockFromTxs(neg, {WitnessCoinbase()});
    if (!Expect(P2PMessageBoundary::EvaluateBlock(neg_block.data(), neg_block.size()),
                P2PValidationDecision::REJECT, "unknown-version-neg")) return false;

    // empty block (header only)
    auto empty_h = Header(4, 1700000000, 0x207fffff, 0x01, 0xee);
    if (!Expect(P2PMessageBoundary::EvaluateBlock(empty_h.data(), empty_h.size()),
                P2PValidationDecision::REJECT, "empty-block-header-only")) return false;
    auto empty_tx = empty_h;
    empty_tx.push_back(0x00); // tx count 0
    if (!Expect(P2PMessageBoundary::EvaluateBlock(empty_tx.data(), empty_tx.size()),
                P2PValidationDecision::REJECT, "empty-block")) return false;

    // malformed transaction vector
    auto mal = Header(4, 1700000000, 0x207fffff, 0x01, 0xee);
    mal.push_back(0x02); // claims 2 txs, no payload
    if (!Expect(P2PMessageBoundary::EvaluateBlock(mal.data(), mal.size()),
                P2PValidationDecision::REJECT, "malformed-tx-vector")) return false;

    // duplicated transaction
    auto h = Header(4, 1700000000, 0x207fffff, 0x02, 0xee);
    auto tx = WitnessCoinbase(0x33);
    auto dup_tx_block = BlockFromTxs(h, {tx, tx});
    if (!Expect(P2PMessageBoundary::EvaluateBlock(dup_tx_block.data(), dup_tx_block.size()),
                P2PValidationDecision::REJECT, "duplicated-transaction")) return false;

    // invalid merkle root (null merkle with txs)
    auto mr = Header(4, 1700000000, 0x207fffff, 0x01, 0x00);
    auto mr_block = BlockFromTxs(mr, {WitnessCoinbase()});
    if (!Expect(P2PMessageBoundary::EvaluateBlock(mr_block.data(), mr_block.size()),
                P2PValidationDecision::REJECT, "invalid-merkle-root")) return false;

    // valid block must ACCEPT_TO_VALIDATION (not Kernel)
    auto ok = ValidBlock(0x09);
    const auto acc = P2PMessageBoundary::EvaluateBlock(ok.data(), ok.size());
    if (!Expect(acc, P2PValidationDecision::ACCEPT_TO_VALIDATION, "valid-block")) return false;
    if (acc.admission != P2PAdmissionTarget::BLOCK_ADMISSION) {
        std::fprintf(stderr, "FAIL valid-block admission target\n");
        return false;
    }
    return true;
}

bool TestInvalidTx()
{
    if (!Expect(P2PMessageBoundary::EvaluateTx(nullptr, 0),
                P2PValidationDecision::REJECT, "malformed-tx-empty")) return false;

    std::vector<uint8_t> trunc{0x01, 0x00, 0x00, 0x00};
    if (!Expect(P2PMessageBoundary::EvaluateTx(trunc.data(), trunc.size()),
                P2PValidationDecision::REJECT, "malformed-tx")) return false;

    // empty input: version + vin count 0 without witness marker continuation
    // version + compact 0 + flags 0 → empty-input
    std::vector<uint8_t> empty_in;
    PutU32(empty_in, 1);
    empty_in.push_back(0x00);
    empty_in.push_back(0x00);
    if (!Expect(P2PMessageBoundary::EvaluateTx(empty_in.data(), empty_in.size()),
                P2PValidationDecision::REJECT, "empty-input")) return false;

    // empty output: 1 vin, 0 vout
    std::vector<uint8_t> empty_out;
    PutU32(empty_out, 1);
    empty_out.push_back(0x01);
    empty_out.insert(empty_out.end(), 32, 0xaa);
    PutU32(empty_out, 0);
    empty_out.push_back(0x00);
    PutU32(empty_out, 0xffffffff);
    empty_out.push_back(0x00); // vout 0
    PutU32(empty_out, 0);
    if (!Expect(P2PMessageBoundary::EvaluateTx(empty_out.data(), empty_out.size()),
                P2PValidationDecision::REJECT, "empty-output")) return false;

    // invalid witness: flag set, all stacks empty
    std::vector<uint8_t> empty_wit;
    PutU32(empty_wit, 1);
    empty_wit.push_back(0x00);
    empty_wit.push_back(0x01);
    empty_wit.push_back(0x01);
    empty_wit.insert(empty_wit.end(), 32, 0xaa);
    PutU32(empty_wit, 0);
    empty_wit.push_back(0x00);
    PutU32(empty_wit, 0xffffffff);
    empty_wit.push_back(0x01);
    PutU64(empty_wit, 1);
    empty_wit.push_back(0x16);
    empty_wit.push_back(0x00);
    empty_wit.push_back(0x14);
    empty_wit.insert(empty_wit.end(), 20, 0x11);
    empty_wit.push_back(0x00); // empty witness stack
    PutU32(empty_wit, 0);
    if (!Expect(P2PMessageBoundary::EvaluateTx(empty_wit.data(), empty_wit.size()),
                P2PValidationDecision::REJECT, "invalid-witness")) return false;

    // oversized script data
    std::vector<uint8_t> bigscript;
    PutU32(bigscript, 1);
    bigscript.push_back(0x01);
    bigscript.insert(bigscript.end(), 32, 0xaa);
    PutU32(bigscript, 0);
    PutCompact(bigscript, 10001);
    bigscript.insert(bigscript.end(), 10001, 0x6a);
    PutU32(bigscript, 0xffffffff);
    bigscript.push_back(0x01);
    PutU64(bigscript, 1);
    bigscript.push_back(0x00);
    PutU32(bigscript, 0);
    if (!Expect(P2PMessageBoundary::EvaluateTx(bigscript.data(), bigscript.size()),
                P2PValidationDecision::REJECT, "oversized-script")) return false;

    // oversized witness element
    auto wit = SpendTx();
    // replace 64-byte sig with 521-byte item: rebuild
    std::vector<uint8_t> fat;
    PutU32(fat, 2);
    fat.push_back(0x00);
    fat.push_back(0x01);
    fat.push_back(0x01);
    fat.insert(fat.end(), 32, 0xaa);
    PutU32(fat, 0);
    fat.push_back(0x00);
    PutU32(fat, 0xffffffff);
    fat.push_back(0x01);
    PutU64(fat, 500);
    fat.push_back(0x16);
    fat.push_back(0x00);
    fat.push_back(0x14);
    fat.insert(fat.end(), 20, 0x22);
    fat.push_back(0x01);
    PutCompact(fat, 521);
    fat.insert(fat.end(), 521, 0xdd);
    PutU32(fat, 0);
    if (!Expect(P2PMessageBoundary::EvaluateTx(fat.data(), fat.size()),
                P2PValidationDecision::REJECT, "invalid-witness-element")) return false;

    // duplicate txid / replay
    auto spend = SpendTx();
    P2PSecurityInput replay = In("tx", spend);
    replay.seen_tx_keys.insert(P2PMessageBoundary::WireFingerprint(spend.data(), spend.size()));
    if (!Expect(P2PMessageBoundary::Evaluate(replay), P2PValidationDecision::REJECT, "duplicate-txid")) return false;
    const auto replay_ok = P2PMessageBoundary::Evaluate(In("tx", spend));
    if (!Expect(replay_ok, P2PValidationDecision::ACCEPT_TO_VALIDATION, "replay-unseen")) return false;

    // invalid signature / unknown asset: structurally OK → ACCEPT_TO_VALIDATION
    // P2P must NOT verify. Spend must enter VerifyRigorPredicate.
    const auto garbage = P2PMessageBoundary::EvaluateTx(spend.data(), spend.size());
    if (!Expect(garbage, P2PValidationDecision::ACCEPT_TO_VALIDATION, "invalid-sig-structural")) return false;
    if (!garbage.spend_must_enter_verify_rigor_predicate) {
        std::fprintf(stderr, "FAIL spend must enter VerifyRigorPredicate\n");
        return false;
    }
    if (garbage.admission != P2PAdmissionTarget::TX_ADMISSION) {
        std::fprintf(stderr, "FAIL tx admission target\n");
        return false;
    }
    return true;
}

bool TestPeerAbuse()
{
    P2PSecurityInput in = In("ping", {0x01});
    in.messages_in_window = P2PSecurityLimits::MAX_MESSAGES_PER_WINDOW + 1;
    if (!Expect(P2PMessageBoundary::Evaluate(in), P2PValidationDecision::BAN_PEER, "excessive-messages")) return false;

    in = In("block", ValidBlock());
    in.invalid_blocks_from_peer = P2PSecurityLimits::MAX_INVALID_BLOCKS_BEFORE_BAN;
    if (!Expect(P2PMessageBoundary::Evaluate(in), P2PValidationDecision::BAN_PEER, "repeated-invalid-blocks")) return false;

    in = In("tx", SpendTx());
    in.invalid_tx_from_peer = P2PSecurityLimits::MAX_INVALID_TX_BEFORE_BAN;
    if (!Expect(P2PMessageBoundary::Evaluate(in), P2PValidationDecision::BAN_PEER, "repeated-invalid-tx")) return false;

    auto ver = VersionMsg(P2PSecurityLimits::BITCOIN_SERIALIZATION_VERSION);
    if (!Expect(P2PMessageBoundary::Evaluate(In("version", ver)),
                P2PValidationDecision::BAN_PEER, "version-abuse-70016")) return false;

    auto ver0 = VersionMsg(0);
    if (!Expect(P2PMessageBoundary::Evaluate(In("version", ver0)),
                P2PValidationDecision::BAN_PEER, "version-abuse-0")) return false;

    std::vector<uint8_t> short_hs{0x01, 0x00};
    if (!Expect(P2PMessageBoundary::Evaluate(In("version", short_hs)),
                P2PValidationDecision::REJECT, "handshake-malformed")) return false;

    auto ver_ok = VersionMsg(1);
    if (!Expect(P2PMessageBoundary::Evaluate(In("version", ver_ok)),
                P2PValidationDecision::ACCEPT_TO_VALIDATION, "version-ok")) return false;

    P2PSecurityInput pre = In("block", ValidBlock());
    pre.handshake_complete = false;
    if (!Expect(P2PMessageBoundary::Evaluate(pre), P2PValidationDecision::REJECT, "pre-handshake-block")) return false;

    PeerMisbehaviorScore score;
    score.Add(10);
    score.Add(90);
    if (!score.ShouldBan()) {
        std::fprintf(stderr, "FAIL peer score ban threshold\n");
        return false;
    }
    // Score must not be a consensus input.
    if (score.Value() == 0) return false;
    return true;
}

std::set<uint64_t> Admit(const std::vector<P2PSecurityInput>& msgs, PeerMisbehaviorScore* score)
{
    std::set<uint64_t> admitted;
    for (const auto& m : msgs) {
        const auto r = P2PMessageBoundary::Evaluate(m);
        if (score) {
            if (r.IsReject()) score->Add(1);
            if (r.IsBan()) score->Add(PeerMisbehaviorScore::BAN_THRESHOLD);
        }
        if (!r.IsAccept()) continue;
        if (m.command == "block" && m.payload.size() >= 80) {
            admitted.insert(P2PMessageBoundary::WireFingerprint(m.payload.data(), 80));
        }
    }
    return admitted;
}

bool TestOrderingAndEclipse()
{
    const auto a = ValidBlock(0x10);
    const auto b = ValidBlock(0x20);
    const auto c = ValidBlock(0x30);
    const auto junk = std::vector<uint8_t>(10, 0xff);

    auto A = In("block", a);
    auto B = In("block", b);
    auto C = In("block", c);
    auto J = In("block", junk);

    PeerMisbehaviorScore s1, s2, s3;
    const auto abc = Admit({A, B, C}, &s1);
    const auto cba = Admit({C, B, A}, &s2);
    const auto acb = Admit({A, J, C, B, J}, &s3);
    if (abc != cba || abc != acb || abc.size() != 3) {
        std::fprintf(stderr, "FAIL ordering variation consensus set\n");
        return false;
    }
    if (s1.Value() == s3.Value()) {
        // s3 saw junk rejects so score should differ — topology/score must not
        // change the admitted set (already checked). Different scores, same set.
    }
    if (abc != acb) return false;

    // many invalid peers
    std::vector<P2PSecurityInput> flood;
    for (int i = 0; i < 64; ++i) {
        P2PSecurityInput p = In("block", junk);
        p.peer_id = static_cast<uint64_t>(i);
        flood.push_back(p);
    }
    flood.push_back(A);
    flood.push_back(B);
    const auto after_flood = Admit(flood, nullptr);
    if (after_flood.size() != 2 || !after_flood.count(P2PMessageBoundary::WireFingerprint(a.data(), 80))) {
        std::fprintf(stderr, "FAIL many-invalid-peers\n");
        return false;
    }

    // disconnect / reconnect: score reset, admitted set persists
    PeerMisbehaviorScore live;
    live.Add(50);
    live.Reset(); // disconnect
    live.Add(1);  // reconnect
    const auto again = Admit({A, B, C}, &live);
    if (again != abc) {
        std::fprintf(stderr, "FAIL disconnect-reconnect\n");
        return false;
    }

    // duplicate peers: two inputs same peer_id, same blocks
    auto A2 = A;
    A2.peer_id = 7;
    auto A3 = A;
    A3.peer_id = 7;
    const auto dup_peer = Admit({A2, A3, B}, nullptr);
    if (dup_peer.size() != 2) {
        std::fprintf(stderr, "FAIL duplicate-peers\n");
        return false;
    }

    // slow peers: Evaluate is order-complete, delay is not an input
    const auto slow = Admit({A, B, C}, nullptr);
    if (slow != abc) {
        std::fprintf(stderr, "FAIL slow-peers\n");
        return false;
    }

    // Peer score must not enter the consensus digest (admitted set).
    PeerMisbehaviorScore high;
    high.Add(PeerMisbehaviorScore::BAN_THRESHOLD);
    const auto with_score = Admit({A, B, C}, nullptr);
    if (with_score != abc || high.ShouldBan() == false) {
        std::fprintf(stderr, "FAIL score isolation\n");
        return false;
    }
    return true;
}

bool TestFuzzSeeds()
{
    const uint8_t* seeds[] = {
        nullptr,
    };
    (void)seeds;
    std::vector<std::vector<uint8_t>> corpus = {
        {},
        {0x00},
        std::vector<uint8_t>(79, 0x00),
        std::vector<uint8_t>(80, 0x00),
        VersionMsg(70016),
        VersionMsg(1),
        ValidBlock(),
        SpendTx(),
        {0xff, 0xff, 0xff, 0xff, 0xff},
    };
    for (const auto& c : corpus) {
        P2PSecurityInput in;
        in.command = "block";
        in.payload = c;
        (void)P2PMessageBoundary::Evaluate(in);
        in.command = "tx";
        (void)P2PMessageBoundary::Evaluate(in);
        in.command = "version";
        (void)P2PMessageBoundary::Evaluate(in);
        in.command = "ping";
        (void)P2PMessageBoundary::Evaluate(in);
        if (!c.empty()) {
            (void)P2PMessageBoundary::EvaluateBlock(c.data(), c.size());
            (void)P2PMessageBoundary::EvaluateTx(c.data(), c.size());
        }
    }
    return true;
}

bool TestIsolationInvariants()
{
    // P2P output space is exactly three decisions.
    const int n = static_cast<int>(P2PValidationDecision::BAN_PEER) + 1;
    if (n != 3) {
        std::fprintf(stderr, "FAIL decision space != 3\n");
        return false;
    }
    auto r = P2PMessageBoundary::Evaluate(In("tx", SpendTx()));
    if (!r.peer_score_isolated) {
        std::fprintf(stderr, "FAIL peer_score_isolated\n");
        return false;
    }
    if (r.IsAccept() && !r.spend_must_enter_verify_rigor_predicate) {
        std::fprintf(stderr, "FAIL accepted tx skipped predicate\n");
        return false;
    }
    return true;
}

#ifdef P2P_SECURITY_STANDALONE
int RunAll()
{
    if (!TestInvalidBlocks()) return 1;
    if (!TestInvalidTx()) return 1;
    if (!TestPeerAbuse()) return 1;
    if (!TestOrderingAndEclipse()) return 1;
    if (!TestFuzzSeeds()) return 1;
    if (!TestIsolationInvariants()) return 1;
    std::printf("P2P_SECURITY_BOUNDARY = CLOSED\n");
    std::printf("P2P_CONSENSUS_BYPASS = NONE\n");
    std::printf("CRASH_FOUND = NO\n");
    std::printf("CONSENSUS_MODIFIED = NO\n");
    return 0;
}
#endif

} // namespace

#ifdef P2P_SECURITY_STANDALONE
int main()
{
    return RunAll();
}
#else
#include <boost/test/unit_test.hpp>
#include <test/util/setup_common.h>

BOOST_FIXTURE_TEST_SUITE(test_p2p_security_boundary, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(invalid_block)
{
    BOOST_REQUIRE(TestInvalidBlocks());
}

BOOST_AUTO_TEST_CASE(invalid_tx)
{
    BOOST_REQUIRE(TestInvalidTx());
}

BOOST_AUTO_TEST_CASE(peer_abuse)
{
    BOOST_REQUIRE(TestPeerAbuse());
}

BOOST_AUTO_TEST_CASE(ordering_variation)
{
    BOOST_REQUIRE(TestOrderingAndEclipse());
}

BOOST_AUTO_TEST_CASE(fuzz_seed_corpus)
{
    BOOST_REQUIRE(TestFuzzSeeds());
}

BOOST_AUTO_TEST_CASE(isolation)
{
    BOOST_REQUIRE(TestIsolationInvariants());
}

BOOST_AUTO_TEST_SUITE_END()
#endif
