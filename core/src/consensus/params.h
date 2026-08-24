// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include <consensus/amount.h>
#include <uint256.h>

#include <chrono>
#include <limits>
#include <map>
#include <vector>

namespace Consensus {

/**
 * RFC-014 §2.1 corrected — SECURITY-001-A finding A-6.
 *
 * RFC-014 fixes the per-candidate convergence probability at `p = 2^-16` and its
 * §5.4 sensitivity analysis (100 mints/yr at N=125, 7.3% saturation at N=5000,
 * 228-day cold start at N=2) reproduces exactly under that figure. But §2.1 then
 * writes the constant as `p = T / 2^256`, i.e. it evaluates the consensus rule
 *
 *     H_i < T * f_i
 *
 * at `f_i = 1`. The implemented weight `f_i` is the convergence amount **in
 * satoshis**, floored at `MIN_CONVERGENCE_AMOUNT = 1 ORU = 1e8`, so shipping
 * `T = 2^240` gave `T * f_min ≈ 2^266.6 > 2^256` and the inequality was
 * satisfied by every candidate unconditionally — the eligibility filter was a
 * constant `true` and each block's RES draw degenerated into "whoever the miner
 * chose to include". The 512-bit arithmetic in rigor_math.h was never at fault.
 *
 * The corrected constant restores exactly the economics RFC-014 already
 * adjudicated, by dividing out the unit error:
 *
 *     RES_PROBABILITY_TARGET := floor(2^E / MIN_CONVERGENCE_AMOUNT)
 *
 * which yields, for a candidate converging n ORU,
 *
 *     p(n) = n / 2^(256-E)      (saturating at 1 for n >= 2^(256-E) ORU)
 *
 * At the E = 240 in force when A-6 was fixed that is p(1 ORU) = 2^-16 exactly, as RFC-014 §2.1
 * intended. That was a correction of a unit error, not a monetary decision: no figure in
 * RFC-014 §5.4 changed. RFC-017 subsequently moves E to 246, which IS a monetary decision and
 * is argued separately below.
 *
 * ---------------------------------------------------------------------------
 * SECURITY-001-A9-C: divided by the draw window W.
 *
 * The candidate set F now spans CONVERGENCE_DRAW_WINDOW = 32 blocks instead of one, so every
 * convergence transaction is a candidate in W consecutive draws rather than a single draw.
 * Left alone, that would multiply RES issuance by W. Dividing the target by the same W
 * restores neutrality exactly:
 *
 *     RES_PROBABILITY_TARGET := floor(2^E / MIN_CONVERGENCE_AMOUNT) >> CONVERGENCE_DRAW_WINDOW_LOG2
 *
 * per-draw probability   p_draw(n) = n / (C · W) = n / 32768     (at E = 246, C = 1024)
 * expected wins per tx   W · p_draw(n) = n / C = n / 1024   ← unchanged from the pre-A9C model
 *
 * So aggregate issuance and the burn-proportional fairness property are both preserved, and
 * no figure in RFC-014 §5.4 changes at the aggregate level.
 *
 * ONE ECONOMIC PROPERTY DOES CHANGE, deliberately. Eligibility no longer saturates at C ORU.
 * Per draw, saturation now needs C·W = 32 768 ORU; a candidate burning C = 1024 ORU is
 * eligible in at least one draw of the window with probability
 * 1 - (1 - p/W)^W ≈ 1 - e^-1 ≈ 63%, not 100%. The hard saturation cliff becomes a smooth
 * curve. This is intended: SECURITY-001-A8-DESIGN-REVIEW finding F-2 identified "burn to
 * saturation, then censor" as the cheapest path to GUARANTEED RES acquisition, and removing
 * the guarantee is the point. Total expected wins per unit burned is unchanged.
 *
 * ---------------------------------------------------------------------------
 * RFC-017: the exponent moves from 240 to 246.
 *
 * Inverting the relation above, a per-block burn of b base units yields b / (1e8 · 2^(256−E))
 * expected eligible candidates, i.e. one mint per C ORU burned with C = 2^(256−E). At E = 240
 * that is C = 65 536 ORU per mint, against an ORU tail of 125 ORU/block: the whole network
 * produces ~100 mints per year even if every newly emitted ORU is converged. A draw that fires
 * twice a week globally at its physical maximum has no price discovery and no observable
 * issuance. E = 246 gives C = 1024 ORU, ~6 400 mints/year at the same physical maximum.
 *
 * E is bounded above as well. The one-mint-per-block rule truncates the expected count, and at
 * E >= 247 epoch 0 saturates: every block mints regardless of burn, so `H_i < T · f_i` stops
 * discriminating and the eligibility filter — the thing A-6 was fixed to preserve — goes inert
 * during bootstrap. E = 246 reaches saturation only under a 100%-burn assumption.
 *
 * Cap utilisation is deliberately NOT a criterion. Cumulative ORU emission is fixed, so
 * (RES issued) × (average ORU cost per RES) is bounded by a constant; raising E buys supply by
 * halving average production cost, which is the inverse of what a store of value needs. See
 * RFC-017 §5.2 and §5.4.
 *
 * INVARIANTS (guarded by res_proof_math_tests):
 *   1. T * MIN_CONVERGENCE_AMOUNT < 2^256 — if this fails the eligibility filter is vacuous
 *      again and the draw degenerates to "whoever the miner chose to include" (the A-6 bug).
 *      At E = 246 the margin is 2^15; the boundary is at E = 261.
 *   2. T == floor(2^246 / MIN_CONVERGENCE_AMOUNT) / CONVERGENCE_DRAW_WINDOW, floored. The
 *      pre-windowing target is not a multiple of W (its residue mod 32 is 27), so the division
 *      discards a remainder and neutrality is exact only to a relative 27/T ≈ 2.4e-65 — far
 *      below the smallest quantity this protocol can express. The floor relation is what is
 *      pinned by test, not a lossless round trip.
 */
inline const uint256 RES_PROBABILITY_TARGET_CONST{
    uint256S("000000000055e63b88c230e77e7ee106959b5d3e1e9ea69ebac0b0b4be383479")};

/** Pre-A9C value, retained so tests can assert the shift relation rather than a bare literal. */
inline const uint256 RES_PROBABILITY_TARGET_UNWINDOWED{
    uint256S("000000000abcc77118461cefcfdc20d2b36ba7c3d3d4d3d758161697c7068f3b")};

/**
 * A buried deployment is one where the height of the activation has been hardcoded into
 * the client implementation long after the consensus change has activated. See BIP 90.
 */
enum BuriedDeployment : int16_t {
    // buried deployments get negative values to avoid overlap with DeploymentPos
    DEPLOYMENT_HEIGHTINCB = std::numeric_limits<int16_t>::min(),
    DEPLOYMENT_CLTV,
    DEPLOYMENT_DERSIG,
    DEPLOYMENT_CSV,
    DEPLOYMENT_SEGWIT,
};
constexpr bool ValidDeployment(BuriedDeployment dep) { return dep <= DEPLOYMENT_SEGWIT; }

enum DeploymentPos : uint16_t {
    DEPLOYMENT_TESTDUMMY,
    // REMOVED: DEPLOYMENT_TAPROOT
    // violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 complexity)
    // NOTE: Also add new deployments to VersionBitsDeploymentInfo in deploymentinfo.cpp
    MAX_VERSION_BITS_DEPLOYMENTS
};
constexpr bool ValidDeployment(DeploymentPos dep) { return dep < MAX_VERSION_BITS_DEPLOYMENTS; }

/**
 * Struct for each individual consensus rule change using BIP9.
 */
struct BIP9Deployment {
    /** Bit position to select the particular bit in nVersion. */
    int bit{28};
    /** Start MedianTime for version bits miner confirmation. Can be a date in the past */
    int64_t nStartTime{NEVER_ACTIVE};
    /** Timeout/expiry MedianTime for the deployment attempt. */
    int64_t nTimeout{NEVER_ACTIVE};
    /** If lock in occurs, delay activation until at least this block
     *  height.  Note that activation will only occur on a retarget
     *  boundary.
     */
    int min_activation_height{0};

    /** Constant for nTimeout very far in the future. */
    static constexpr int64_t NO_TIMEOUT = std::numeric_limits<int64_t>::max();

    /** Special value for nStartTime indicating that the deployment is always active.
     *  This is useful for testing, as it means tests don't need to deal with the activation
     *  process (which takes at least 3 BIP9 intervals). Only tests that specifically test the
     *  behaviour during activation cannot use this. */
    static constexpr int64_t ALWAYS_ACTIVE = -1;

    /** Special value for nStartTime indicating that the deployment is never active.
     *  This is useful for integrating the code changes for a new feature
     *  prior to deploying it on some or all networks. */
    static constexpr int64_t NEVER_ACTIVE = -2;
};

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;
    int nSubsidyHalvingInterval;
    /**
     * Per-block ORU subsidy base before right-shift halving (when non-zero). Zero selects legacy
     * 50 * COIN base (Bitcoin-style halving to zero after 64 periods).
     * Rigor main/testnet/signet/regtest/devtest: `Consensus::ORU_INITIAL_BLOCK_SUBSIDY` (see consensus/rigor_consensus_lock.h) with nBlockSubsidyMaxHalvingShifts caps further halving (see GetBlockSubsidy).
     */
    CAmount nBlockSubsidyBase{0};
    /**
     * When >= 0, subsidy = nBlockSubsidyBase >> min(nHeight / nSubsidyHalvingInterval, nBlockSubsidyMaxHalvingShifts)
     * (no further halving after that floor; ORU emission continues indefinitely at that per-block amount).
     * Example: base 1000 ORU, interval 210000 (~4y at 10min blocks): 1000 → 500 → 250 → 125 ORU then fixed at 125.
     * -1 means uncapped Bitcoin-style halving until period 64 then subsidy 0 (only used when nBlockSubsidyBase == 0).
     */
    int nBlockSubsidyMaxHalvingShifts{-1};
    /** Fixed draw target bits: T = ResMintTargetArith(this); see Consensus::ResDrawTargetForBlock. */
    uint8_t res_mint_target_bits{14};
    /** Confirmations required before spending an ORU coinbase or a body RES mint UTXO. */
    int nCoinbaseMaturity{100};
    /**
     * Hashes of blocks that
     * - are known to be consensus valid, and
     * - buried in the chain, and
     * - fail if the default script verify flags are applied.
     */
    std::map<uint256, uint32_t> script_flag_exceptions;
    /** Block height and hash at which BIP34 becomes active */
    int BIP34Height;
    uint256 BIP34Hash;
    /** Block height at which BIP65 becomes active */
    int BIP65Height;
    /** Block height at which BIP66 becomes active */
    int BIP66Height;
    /** Block height at which CSV (BIP68, BIP112 and BIP113) becomes active */
    int CSVHeight;
    /** Block height at which Segwit (BIP141, BIP143 and BIP147) becomes active.
     * Note that segwit v0 script rules are enforced on all blocks except the
     * BIP 16 exception blocks. */
    int SegwitHeight;
    /** Don't warn about unknown BIP 9 activations below this height.
     * This prevents us from warning about the CSV and segwit activations. */
    int MinBIP9WarningHeight;
    /**
     * RFC-014 (Conv-Decouple-01), as corrected by SECURITY-001-A finding A-6:
     * RES convergence eligibility constant. Identical across all networks
     * (RFC-014 §2.4). It is the RHS of `H_i < RES_PROBABILITY_TARGET * f_i`
     * (RFC-008 §6.1 / §14) and is NOT derived at runtime from `nBits`, ASERT
     * state, or any header field. `target_prev` remains in `bind_commit` for
     * Sigma-CR-01 replay-domain separation only (RFC-014 §3.2).
     *
     * Always assign `Consensus::RES_PROBABILITY_TARGET_CONST`; see the note
     * there for why the literal `2^240` printed in RFC-014 §2.1 is wrong, and for
     * why RFC-017 sets the exponent to 246.
     */
    uint256 RES_PROBABILITY_TARGET;
    /**
     * Minimum blocks including miner confirmation of the total of 2016 blocks in a retargeting period,
     * (nPowTargetTimespan / nPowTargetSpacing) which is also used for BIP9 deployments.
     * Examples: 1916 for 95%, 1512 for testchains.
     */
    uint32_t nRuleChangeActivationThreshold;
    uint32_t nMinerConfirmationWindow;
    BIP9Deployment vDeployments[MAX_VERSION_BITS_DEPLOYMENTS];
    /** Proof of work parameters */
    uint256 powLimit;
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    int64_t nPowTargetSpacing;
    int64_t nPowTargetTimespan;
    /** ASERT (aserti3-2d) half-life τ in seconds. Mainnet 43200 (12h); parent time + height-1 anchor. */
    int64_t nPowASERTHalfLife{0};
    std::chrono::seconds PowTargetSpacing() const
    {
        return std::chrono::seconds{nPowTargetSpacing};
    }
    int64_t DifficultyAdjustmentInterval() const { return nPowTargetTimespan / nPowTargetSpacing; }
    /** The best chain should have at least this much work */
    uint256 nMinimumChainWork;
    /** By default assume that the signatures in ancestors of this block are valid */
    uint256 defaultAssumeValid;

    /**
     * If true, witness commitments contain a payload equal to a Bitcoin Script solution
     * to the signet challenge. See BIP325.
     */
    bool signet_blocks{false};
    std::vector<uint8_t> signet_challenge;

    /**
     * FENCE-W11: extra per-coinbase RES on mockable test chains only.
     * Default 0. Set at CChainParams construction from -reswalletbootstrap
     * (CreateChainParams / RegTestOptions / MockableChainOptions). Never read
     * gArgs from ConnectBlock or the interpreter. Mainnet / testnet / signet
     * remain 0. ConnectBlock `bad-cb-res-amount` uses this field.
     */
    CAmount res_wallet_bootstrap_coinbase{0};

    int DeploymentHeight(BuriedDeployment dep) const
    {
        switch (dep) {
        case DEPLOYMENT_HEIGHTINCB:
            return BIP34Height;
        case DEPLOYMENT_CLTV:
            return BIP65Height;
        case DEPLOYMENT_DERSIG:
            return BIP66Height;
        case DEPLOYMENT_CSV:
            return CSVHeight;
        case DEPLOYMENT_SEGWIT:
            return SegwitHeight;
        } // no default case, so the compiler can warn about missing cases
        return std::numeric_limits<int>::max();
    }
};

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
