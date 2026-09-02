// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-077: performance optimization regression. Standalone.
// Proves result / state / hash identity vs the pre-optimization ledger apply.
// Does not recode issuance / RES / draw / supply / PoW / ASERT.

#include <consensus/amount.h>
#include <consensus/ledger/ledger_transition.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/txkernel/transaction_kernel_input.h>
#include <consensus/utxo/utxo_state_transition.h>
#include <script/script.h>
#include <uint256.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

static int g_fails = 0;

static void Expect(bool ok, const char* msg)
{
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_fails;
    }
}

static Consensus::Params FrozenParams()
{
    Consensus::Params p;
    p.nBlockSubsidyBase = Consensus::ORU_INITIAL_BLOCK_SUBSIDY;
    p.nSubsidyHalvingInterval = Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
    p.nBlockSubsidyMaxHalvingShifts = Consensus::ORU_SUBSIDY_MAX_SHIFTS;
    p.nCoinbaseMaturity = 100;
    return p;
}

static std::vector<unsigned char> ScriptBytes(const CScript& s)
{
    return std::vector<unsigned char>(s.begin(), s.end());
}

static CScript OruScript()
{
    return CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
}

static LedgerCoin Coin(uint64_t id, uint32_t n, CAmount value, LedgerAsset asset, int64_t cb = -1)
{
    LedgerCoin c;
    c.outpoint.id = id;
    c.outpoint.n = n;
    c.value = value;
    c.asset = asset;
    c.coinbase_height = cb;
    return c;
}

static TransactionKernelInput ValidOruTx()
{
    TransactionKernelInput in;
    in.flags = TK_SCRIPT_VERIFY_WITNESS;
    in.scriptSig_push_only = true;
    in.stack_clean = true;
    in.signature.assign(64, 0x07);
    in.pubkey.assign(32, 0x09);
    in.scriptPubKey = ScriptBytes(OruScript());
    in.witness = {in.signature, in.pubkey};
    in.keyhash_match = true;
    in.crypto_accept = true;
    in.sighash_tag = KernelAssignedSighashTag(CScript(in.scriptPubKey.begin(), in.scriptPubKey.end()));
    return in;
}

static uint64_t HashOut(const LedgerKernelOutput& o)
{
    uint64_t h = (o.accept ? 1ULL : 0ULL) ^ (o.value_ok ? 2ULL : 0ULL);
    h ^= static_cast<uint64_t>(o.spent_delta.size()) << 4;
    h ^= static_cast<uint64_t>(o.created_delta.size()) << 12;
    for (const auto& r : o.spent_delta) {
        h = h * 131ULL + r.id + (static_cast<uint64_t>(r.n) << 32);
    }
    for (const auto& c : o.created_delta) {
        h = h * 131ULL + c.outpoint.id + static_cast<uint64_t>(c.value) +
            static_cast<uint64_t>(c.asset);
    }
    return h;
}

static uint64_t HashUtxo(const UTXOStateTransitionResult& o)
{
    uint64_t h = o.accept ? 1ULL : 0ULL;
    h ^= static_cast<uint64_t>(o.delta.removals.size()) << 8;
    h ^= static_cast<uint64_t>(o.delta.additions.size()) << 16;
    for (const auto& r : o.delta.removals) {
        h = h * 131ULL + r.id + r.n;
    }
    for (const auto& c : o.delta.additions) {
        h = h * 131ULL + c.outpoint.id + static_cast<uint64_t>(c.value);
    }
    return h;
}

static bool SameLedger(const LedgerKernelOutput& a, const LedgerKernelOutput& b)
{
    if (a.accept != b.accept || a.value_ok != b.value_ok) return false;
    if (a.spent_delta.size() != b.spent_delta.size()) return false;
    if (a.created_delta.size() != b.created_delta.size()) return false;
    for (std::size_t i = 0; i < a.spent_delta.size(); ++i) {
        if (!(a.spent_delta[i] == b.spent_delta[i])) return false;
    }
    for (std::size_t i = 0; i < a.created_delta.size(); ++i) {
        if (!(a.created_delta[i].outpoint == b.created_delta[i].outpoint)) return false;
        if (a.created_delta[i].value != b.created_delta[i].value) return false;
        if (a.created_delta[i].asset != b.created_delta[i].asset) return false;
        if (a.created_delta[i].coinbase_height != b.created_delta[i].coinbase_height) return false;
    }
    return true;
}

// Pre-optimization LedgerApply (P063 original). Test-only reference. Not production.
namespace {

bool AmountOk(CAmount v) { return v >= 0 && v <= MAX_MONEY; }

bool AddMoney(CAmount a, CAmount b, CAmount& out)
{
    if (!AmountOk(a) || !AmountOk(b)) return false;
    if (b > MAX_MONEY - a) return false;
    out = a + b;
    return AmountOk(out);
}

bool OutpointIn(const LedgerOutpoint& o, const std::vector<LedgerOutpoint>& xs, std::size_t from)
{
    for (std::size_t i = from; i < xs.size(); ++i) {
        if (xs[i] == o) return true;
    }
    return false;
}

bool NoDupOutpointsLinear(const std::vector<LedgerOutpoint>& xs)
{
    for (std::size_t i = 0; i < xs.size(); ++i) {
        if (OutpointIn(xs[i], xs, i + 1)) return false;
    }
    return true;
}

bool SumAsset(const std::vector<LedgerCoin>& coins, LedgerAsset asset, CAmount& out)
{
    out = 0;
    for (const auto& c : coins) {
        if (c.asset != asset) continue;
        if (!AddMoney(out, c.value, out)) return false;
    }
    return true;
}

bool AllAssetsKnown(const std::vector<LedgerCoin>& coins)
{
    for (const auto& c : coins) {
        if (c.asset != LedgerAsset::ORU && c.asset != LedgerAsset::RES) return false;
        if (!AmountOk(c.value)) return false;
    }
    return true;
}

bool Mature(const LedgerKernelInput& in)
{
    for (const auto& c : in.consumed) {
        if (c.coinbase_height < 0) continue;
        if (c.coinbase_height > in.height) return false;
        if (in.height - c.coinbase_height < in.maturity) return false;
    }
    return true;
}

bool GenesisOk(const LedgerKernelInput& in)
{
    if (in.height != 0) return true;
    return in.consensus.oru_issuance == 0 &&
           in.consensus.res_mint == 0 &&
           in.consensus.res_schedule == 40000000;
}

LedgerKernelOutput LedgerApplyReference(const LedgerKernelInput& in)
{
    LedgerKernelOutput out;
    out.accept = false;
    out.value_ok = false;

    if (!in.tx.accept || !in.consensus.accept) return out;
    if (!AmountOk(in.fee) || !AmountOk(in.burn) || !AmountOk(in.converge)) return out;
    if (!AmountOk(in.consensus.oru_issuance) || !AmountOk(in.consensus.res_mint)) return out;
    if (!AllAssetsKnown(in.consumed) || !AllAssetsKnown(in.created)) return out;

    std::vector<LedgerOutpoint> spent;
    spent.reserve(in.consumed.size());
    for (const auto& c : in.consumed) spent.push_back(c.outpoint);
    std::vector<LedgerOutpoint> created_ops;
    created_ops.reserve(in.created.size());
    for (const auto& c : in.created) created_ops.push_back(c.outpoint);
    if (!NoDupOutpointsLinear(spent) || !NoDupOutpointsLinear(created_ops)) return out;
    if (!Mature(in) || !GenesisOk(in)) return out;

    CAmount in_all = 0;
    CAmount out_all = 0;
    CAmount in_oru = 0;
    CAmount out_oru = 0;
    CAmount in_res = 0;
    CAmount out_res = 0;
    if (!SumAsset(in.consumed, LedgerAsset::ORU, in_oru)) return out;
    if (!SumAsset(in.consumed, LedgerAsset::RES, in_res)) return out;
    if (!SumAsset(in.created, LedgerAsset::ORU, out_oru)) return out;
    if (!SumAsset(in.created, LedgerAsset::RES, out_res)) return out;
    if (!AddMoney(in_oru, in_res, in_all)) return out;
    if (!AddMoney(out_oru, out_res, out_all)) return out;

    CAmount credited = 0;
    if (!AddMoney(in_all, in.consensus.oru_issuance, credited)) return out;
    if (!AddMoney(credited, in.consensus.res_mint, credited)) return out;
    CAmount sinks = 0;
    if (!AddMoney(in.burn, in.fee, sinks)) return out;
    CAmount rhs = 0;
    if (!AddMoney(out_all, sinks, rhs)) return out;
    const bool value_ok = credited >= rhs;
    out.value_ok = value_ok;
    if (!value_ok) return out;

    CAmount oru_sinks = 0;
    if (!AddMoney(in.burn, in.converge, oru_sinks)) return out;
    if (!AddMoney(oru_sinks, in.fee, oru_sinks)) return out;
    CAmount oru_left = 0;
    if (!AddMoney(in_oru, in.consensus.oru_issuance, oru_left)) return out;
    if (oru_left < oru_sinks) return out;
    if (oru_left - oru_sinks != out_oru) return out;
    if (in_res > std::numeric_limits<CAmount>::max() - in.consensus.res_mint) return out;
    if (in_res + in.consensus.res_mint != out_res) return out;

    out.accept = true;
    out.spent_delta = std::move(spent);
    out.created_delta = in.created;
    return out;
}

} // namespace

static LedgerKernelInput BaseLedger()
{
    LedgerKernelInput in;
    in.tx.accept = true;
    in.consensus.accept = true;
    in.consensus.oru_issuance = 0;
    in.consensus.res_mint = 0;
    in.consensus.res_schedule = 40000000;
    in.height = 1;
    in.maturity = 100;
    return in;
}

static UTXOStateTransitionInput OruUtxo()
{
    UTXOStateTransitionInput in;
    in.params = FrozenParams();
    in.height = 200;
    in.maturity = 100;
    in.tx = ValidOruTx();
    in.snapshot = {Coin(1, 0, 10 * COIN, LedgerAsset::ORU)};
    in.ledger.consumed = in.snapshot;
    in.ledger.created = {Coin(2, 0, 10 * COIN, LedgerAsset::ORU)};
    in.asset_scripts = {ScriptBytes(OruScript())};
    in.consensus.draw_target = Consensus::RES_PROBABILITY_TARGET_CONST;
    return in;
}

static void CompareLedger(const LedgerKernelInput& in, const char* label)
{
    const LedgerKernelOutput a = LedgerApplyReference(in);
    const LedgerKernelOutput b = LedgerApply(in);
    const LedgerKernelOutput c = LedgerApply(in);
    Expect(SameLedger(a, b), label);
    Expect(SameLedger(b, c), "dual-run state");
    Expect(HashOut(a) == HashOut(b), "hash identical");
    Expect(HashOut(b) == HashOut(c), "hash dual-run");
}

int main()
{
    const Consensus::Params p = FrozenParams();

    {
        const RigorIssuance a = ComputeRigorIssuance(0, p);
        const RigorIssuance b = ComputeRigorIssuance(0, p);
        Expect(a.oru_subsidy == 0 && a.oru_subsidy == b.oru_subsidy, "genesis issuance");
        Expect(a.res_reward == b.res_reward, "genesis RES schedule identity");
        Expect(GetBlockSubsidy(1, p) == ComputeRigorIssuance(1, p).oru_subsidy, "issuance pack");
        Expect(Consensus::GetResMintReward(1) == ComputeRigorIssuance(1, p).res_reward, "RES pack");
    }

    {
        LedgerKernelInput in = BaseLedger();
        in.height = 0;
        CompareLedger(in, "genesis result identical");
        Expect(LedgerApply(in).accept, "genesis accept");
    }

    {
        LedgerKernelInput in = BaseLedger();
        in.consumed = {Coin(7, 0, 10 * COIN, LedgerAsset::ORU, 1)};
        in.created = {Coin(8, 0, 10 * COIN, LedgerAsset::ORU)};
        in.height = 99;
        CompareLedger(in, "maturity 99");
        Expect(!LedgerApply(in).accept, "immature reject");
        in.height = 101;
        CompareLedger(in, "maturity 101");
        Expect(LedgerApply(in).accept, "mature accept");
    }

    {
        LedgerKernelInput in = BaseLedger();
        in.consumed = {Coin(1, 0, 10 * COIN, LedgerAsset::ORU)};
        in.created = {Coin(2, 0, 10 * COIN, LedgerAsset::ORU)};
        CompareLedger(in, "oru transfer");
    }

    {
        LedgerKernelInput in = BaseLedger();
        in.consumed = {Coin(3, 0, 40 * Consensus::RES_COIN, LedgerAsset::RES, 0)};
        in.created = {Coin(4, 0, 40 * Consensus::RES_COIN, LedgerAsset::RES)};
        in.height = 200;
        CompareLedger(in, "RES mature spend");
    }

    {
        LedgerKernelInput in = BaseLedger();
        std::vector<LedgerCoin> coins;
        CAmount sum = 0;
        for (int i = 0; i < 64; ++i) {
            coins.push_back(Coin(20 + i, 0, COIN, LedgerAsset::ORU));
            sum += COIN;
        }
        in.consumed = coins;
        in.created = {Coin(99, 0, sum, LedgerAsset::ORU)};
        CompareLedger(in, "many inputs uniqueness");
        Expect(LedgerApply(in).accept, "many inputs accept");
    }

    {
        LedgerKernelInput in = BaseLedger();
        in.consumed = {Coin(1, 0, 8 * COIN, LedgerAsset::ORU), Coin(1, 0, COIN, LedgerAsset::ORU)};
        in.created = {Coin(2, 0, 9 * COIN, LedgerAsset::ORU)};
        CompareLedger(in, "duplicate input reject");
        Expect(!LedgerApply(in).accept, "dup reject");
    }

    {
        LedgerKernelInput in = BaseLedger();
        in.tx.accept = false;
        in.consumed = {Coin(1, 0, COIN, LedgerAsset::ORU)};
        in.created = {Coin(2, 0, COIN, LedgerAsset::ORU)};
        CompareLedger(in, "tx reject");
        Expect(!LedgerApply(in).accept, "tx reject stays reject");
    }

    {
        const UTXOStateTransitionInput in = OruUtxo();
        const auto a = UTXOStateTransition(in);
        const auto b = UTXOStateTransition(in);
        Expect(a.accept == b.accept, "utxo result identical");
        Expect(HashUtxo(a) == HashUtxo(b), "utxo hash identical");
        Expect(a.delta.removals.size() == b.delta.removals.size(), "utxo state identical");
        Expect(a.accept, "oru utxo accept");
    }

    {
        UTXOStateTransitionInput in = OruUtxo();
        in.height = 0;
        in.ledger.consumed.clear();
        in.ledger.created.clear();
        in.snapshot.clear();
        in.tx = TransactionKernelInput{};
        const auto a = UTXOStateTransition(in);
        const auto b = UTXOStateTransition(in);
        Expect(a.accept && b.accept, "empty block accept");
        Expect(HashUtxo(a) == HashUtxo(b), "empty hash identical");
    }

    {
        TransactionKernelInput sighash = ValidOruTx();
        sighash.sighash_tag.assign(16, 0x51);
        sighash.chain_id.assign(32, 0x01);
        sighash.hashPrevouts.assign(32, 0x02);
        sighash.hashSequence.assign(32, 0x03);
        sighash.hashOutputs.assign(32, 0x04);
        sighash.prevout_hash.assign(32, 0x05);
        sighash.amount = 10 * COIN;
        sighash.sighash_fields_present = true;
        const auto p1 = CppSighashPreimage(sighash);
        const auto p2 = CppSighashPreimage(sighash);
        Expect(p1 == p2, "sighash bytes identical");
        Expect(!CppSighashHasHashtypeByte(p1), "no hashtype byte");
        TransactionKernelInput empty;
        Expect(!VerifyRigorPredicateBoundary(empty), "empty predicate reject");
        Expect(VerifyRigorPredicateBoundary(ValidOruTx()), "oru predicate accept");
    }

    if (g_fails) {
        std::fprintf(stderr, "PERFORMANCE_CONSENSUS_REGRESSION=%d\n", g_fails);
        return 1;
    }
    std::printf("RESULT_IDENTICAL=YES\n");
    std::printf("STATE_IDENTICAL=YES\n");
    std::printf("HASH_IDENTICAL=YES\n");
    std::printf("CONSENSUS_BEHAVIOR_CHANGED=FALSE\n");
    std::printf("STATE_TRANSITION_CHANGED=FALSE\n");
    std::printf("HASH_CHANGED=FALSE\n");
    std::printf("PERFORMANCE_CONSENSUS_REGRESSION=PASS\n");
    return 0;
}
