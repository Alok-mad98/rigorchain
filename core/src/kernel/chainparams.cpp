// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <kernel/chainparams.h>

#include <consensus/amount.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/merkle.h>
#include <consensus/params.h>
#include <hash.h>
#include <chainparamsbase.h>
#include <logging.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <uint256.h>
#include <util/strencodings.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

namespace {
static_assert(Consensus::ORU_INITIAL_BLOCK_SUBSIDY == 1000 * COIN);
static_assert(Consensus::ORU_SUBSIDY_MAX_SHIFTS == 3);
} // namespace

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/** Mainnet genesis PoW / header fields — fixed; do not read from runtime configuration. */
namespace GenesisLockMainnet {
constexpr uint32_t TIMESTAMP = 1778224715;
constexpr uint32_t NONCE = 0;
constexpr uint32_t NBITS = 0x207fffff;
constexpr int32_t VERSION = 1;
static const char* const TIMESTAMP_MSG =
    "Selection not computation. Convergence defines state. Naming is consensus.";
} // namespace GenesisLockMainnet

/**
 * Main network on which people trade goods and services.
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = CBaseChainParams::MAIN;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
        consensus.nBlockSubsidyBase = Consensus::ORU_INITIAL_BLOCK_SUBSIDY;
        consensus.nBlockSubsidyMaxHalvingShifts = Consensus::ORU_SUBSIDY_MAX_SHIFTS;
        consensus.nCoinbaseMaturity = 100;
        // Rigor mainnet uses a fresh genesis (genesis-lockdown.md). Do not copy Bitcoin's historical
        // script_flag_exceptions or buried-fork heights — those Bitcoin block hashes never occur here.
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 0; // witness rules from genesis (matches regtest/devtest)
        consensus.MinBIP9WarningHeight = 0;
        // RFC-014 v1.1 (Conv-Decouple-01): RES convergence eligibility constant
        // = floor(2^240 / MIN_CONVERGENCE_AMOUNT). RFC-014 v1.0 specified 2^240, which was
        // derived at f = 1 while the implemented weight is in satoshis; see SECURITY-001-A
        // finding A-6 and the derivation in consensus/params.h.
        // Identical across all networks; per RFC-014 §2.4 no per-network divergence.
        consensus.RES_PROBABILITY_TARGET = Consensus::RES_PROBABILITY_TARGET_CONST;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetSpacing = 10 * 60; // 10 minutes
        consensus.nPowTargetTimespan = 288 * consensus.nPowTargetSpacing; // 288 blocks
        consensus.nPowASERTHalfLife = Consensus::MAINNET_POW_ASERT_HALF_LIFE; // 2 days
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 259; // 90% of 288
        consensus.nMinerConfirmationWindow = 288; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // REMOVED: DEPLOYMENT_TAPROOT configuration
        // violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 complexity)

        /**
         * nMinimumChainWork — header-sync gate (not a substitute for economic majority / 51% defense).
         *
         * Release policy: keep this as a conservative floor beneath canonical tip chainwork.
         * Refresh before each mainnet release from a trusted fully-synced node:
         *   1) `rigor-cli getblockchaininfo` and copy "chainwork".
         *   2) Subtract safety margin (for example, 2k-10k blocks) and pin that value here.
         *   3) Never set above your own honest tip chainwork.
         *
         * Current bootstrap floor: 0x33540 (210240 decimal) ~= one nominal year of minimum
         * genesis-level work (GetBlockProof(genesis)==2, 52560 blocks/year).
         */
        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000000000000033540");

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xce;
        pchMessageStart[1] = 0xf1;
        pchMessageStart[2] = 0xdb;
        pchMessageStart[3] = 0xfa;
        nDefaultPort = 8760;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 540;
        m_assumed_chain_state_size = 7;

        // Mainnet manifesto — scriptPubKey hex is fixed; PoW fields use GenesisLockMainnet::* only.
        const std::vector<unsigned char> mainnet_genesis_spk = ParseHex("00141111111111111111111111111111111111111111");
        const CScript genesis_output_script(mainnet_genesis_spk.begin(), mainnet_genesis_spk.end());
        genesis = CreateGenesisBlock(GenesisLockMainnet::TIMESTAMP_MSG, genesis_output_script,
            GenesisLockMainnet::TIMESTAMP, GenesisLockMainnet::NONCE, GenesisLockMainnet::NBITS,
            GenesisLockMainnet::VERSION, Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
        consensus.hashGenesisBlock = genesis.GetHash();
        // Mainnet genesis timestamp/payload is customized for this chain.
        // Validate consistency against the computed merkle root instead of Bitcoin's legacy constant.
        assert(genesis.hashMerkleRoot == BlockMerkleRoot(genesis));
        assert(genesis.GetHash() == consensus.hashGenesisBlock);
        assert(consensus.hashGenesisBlock == uint256S("0x5dd90c30e88b90a1a6ce0b371d4abf8d7283d8a1275f70f9d51a8821048221de"));
        // Genesis values are derived from the locked timestamp payload above.

        /**
         * defaultAssumeValid — optional IBD optimization (see ConnectBlock in validation.cpp).
         * Pinned to genesis until a deep canonical tip exists; avoids stale hashes from prior
         * genesis revisions. Refresh to a buried block hash before releases once the chain is live:
         *   rigor-cli getblockhash <height_H>
         */
        consensus.defaultAssumeValid = consensus.hashGenesisBlock;

        // Checkpoints: map height → block hash on the canonical chain only.
        // When the chain has a stable deep tip, add rows after {0,...}, e.g.
        //   {150'000, uint256S("0x<output of: rigor-cli getblockhash 150000>")},
        // Keep heights monotonic; hashes must match your synced honest node.
        checkpointData = {
            {
                {0, consensus.hashGenesisBlock},
            }
        };

        // Mainnet DNS seeds (Rigor network).
        vSeeds.clear();
        vSeeds.emplace_back("seed1.rigor.network.");
        vSeeds.emplace_back("seed2.rigor.network.");
        vSeeds.emplace_back("seed3.rigor.network.");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,0);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "rg";

        // Keep fixed seeds empty until dedicated ORU seed snapshots are generated.
        vFixedSeeds.clear();

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        m_is_test_chain = false;
        m_is_mockable_chain = false;

        m_assumeutxo_data = MapAssumeutxo{
         // TODO to be specified in a future patch.
        };

        chainTxData = ChainTxData{0, 0, 0};
    }
};

/**
 * Testnet: Rigor public proving ground — fast blocks, permissive min-difficulty gate,
 * ASERT retargeting every block, and buried deployments aligned to Rigor (not Bitcoin testnet3).
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = CBaseChainParams::TESTNET;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
        consensus.nBlockSubsidyBase = Consensus::ORU_INITIAL_BLOCK_SUBSIDY;
        consensus.nBlockSubsidyMaxHalvingShifts = Consensus::ORU_SUBSIDY_MAX_SHIFTS;
        consensus.nCoinbaseMaturity = 100;
        // No script_flag_exceptions: Bitcoin-era exception hashes never occur on Rigor testnet.
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 0; // witness rules from genesis (matches mainnet/devtest)
        consensus.MinBIP9WarningHeight = 0;
        // RFC-014 v1.1 (Conv-Decouple-01): RES convergence eligibility constant
        // = floor(2^240 / MIN_CONVERGENCE_AMOUNT). RFC-014 v1.0 specified 2^240, which was
        // derived at f = 1 while the implemented weight is in satoshis; see SECURITY-001-A
        // finding A-6 and the derivation in consensus/params.h.
        // Identical across all networks; per RFC-014 §2.4 no per-network divergence.
        consensus.RES_PROBABILITY_TARGET = Consensus::RES_PROBABILITY_TARGET_CONST;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetSpacing = 60; // 1 minute nominal inter-block interval
        consensus.nPowTargetTimespan = 30 * consensus.nPowTargetSpacing; // 30-block BIP9 window
        // Short ASERT half-life so difficulty tracks hashrate quickly (per-block evaluation).
        consensus.nPowASERTHalfLife = 300; // 5 minutes
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 27; // 90% of 30
        consensus.nMinerConfirmationWindow = 30;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // REMOVED: DEPLOYMENT_TAPROOT configuration
        // violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 complexity)

        // Minimum work for the locked testnet genesis (nBits 0x207fffff, same order of magnitude as mainnet genesis).
        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000000000000000002");
        consensus.defaultAssumeValid = uint256{};

        // Distinct wire magic from mainnet / regtest / legacy Rigor testnet builds (avoid accidental peer cross-talk).
        pchMessageStart[0] = 0x0c;
        pchMessageStart[1] = 0x12;
        pchMessageStart[2] = 0x0a;
        pchMessageStart[3] = 0x08;
        nDefaultPort = 18760;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 42;
        m_assumed_chain_state_size = 3;

        // Distinct coinbase witness from mainnet; easy target (0x207fffff, nonce 0) so the payload can be changed without remining.
        // Keep coinbase scriptSig <=100 bytes (see Consensus::CheckTransaction "bad-cb-length"); testnet must stay shorter than the old 90-char draft.
        const std::string testnet_genesis_timestamp = "Selection not computation. Convergence defines state. Naming is consensus.";
        const std::vector<unsigned char> testnet_genesis_spk = ParseHex("00142222222222222222222222222222222222222222");
        const CScript testnet_genesis_output_script(testnet_genesis_spk.begin(), testnet_genesis_spk.end());
        genesis = CreateGenesisBlock(testnet_genesis_timestamp.c_str(), testnet_genesis_output_script, 1778224715, 0, 0x207fffff, 1, Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(genesis.hashMerkleRoot == BlockMerkleRoot(genesis));
        assert(genesis.GetHash() == consensus.hashGenesisBlock);
        assert(consensus.hashGenesisBlock == uint256S("0xdc14491196188457f5e2d9f5951e38b788a1c714dd606225b7490dafa9ef8bca"));
        // Genesis values are derived from the locked timestamp payload above.

        checkpointData = {
            {
                {0, consensus.hashGenesisBlock},
            }
        };

        // Rigor public testnet: DNS seeds only (no Bitcoin bootstrap peers).
        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.emplace_back("seed1.testnet.rigor.network.");
        vSeeds.emplace_back("seed2.testnet.rigor.network.");
        vSeeds.emplace_back("seed3.testnet.rigor.network.");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "trg";

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        m_is_test_chain = true;
        m_is_mockable_chain = false;

        m_assumeutxo_data = MapAssumeutxo{
            // TODO to be specified in a future patch.
        };

        chainTxData = ChainTxData{0, 0, 0};
    }
};

/**
 * Devtest: fast public-style test chain for feature validation.
 */
class CDevTestParams : public CChainParams {
public:
    CDevTestParams() {
        strNetworkID = CBaseChainParams::DEVTEST;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
        consensus.nBlockSubsidyBase = Consensus::ORU_INITIAL_BLOCK_SUBSIDY;
        consensus.nBlockSubsidyMaxHalvingShifts = Consensus::ORU_SUBSIDY_MAX_SHIFTS;
        consensus.res_mint_target_bits = 1;
        // Align with mainnet/signet: ORU and RES coinbase outputs mature after 100 confirmations.
        consensus.nCoinbaseMaturity = 100;
        consensus.script_flag_exceptions.emplace( // BIP16 exception
            uint256S("0x00000000dd30457c001f4095d208cc1296b0eed002427aa599874af7a432b105"), SCRIPT_VERIFY_NONE);
        // Active from block 1, unlike the testnet3 heights this chain was originally cloned from.
        //
        // On Rigor BIP34 is not just the height push: the same deployment gates the coinbase
        // ConvergenceRoot commitment (`bad-cb-convergence-root`), which is what pins C — a σ input
        // for the next block's draw. Left at 21111 the rule slept through every devtest run, so no
        // functional test ever checked that a block commits the C consensus derives, and
        // SECURITY-001-A9-C made that gap load-bearing: C now spans the whole draw window, so a
        // producer that derives it from its own body alone is wrong in a way nothing on devtest
        // would have caught.
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 581885;
        consensus.BIP66Height = 330776;
        consensus.CSVHeight = 770112;
        // Must be active from genesis: devtest uses bech32/P2WPKH wallets and mines witness-bearing txs long
        // before mainnet-style height 834624; leaving Segwit inactive causes ContextualCheckBlock
        // "unexpected-witness" when the mempool includes segwit spends.
        consensus.SegwitHeight = 0;
        consensus.MinBIP9WarningHeight = 0;
        // RFC-014 v1.1 (Conv-Decouple-01): RES convergence eligibility constant
        // = floor(2^240 / MIN_CONVERGENCE_AMOUNT). RFC-014 v1.0 specified 2^240, which was
        // derived at f = 1 while the implemented weight is in satoshis; see SECURITY-001-A
        // finding A-6 and the derivation in consensus/params.h.
        // Identical across all networks; per RFC-014 §2.4 no per-network divergence.
        consensus.RES_PROBABILITY_TARGET = Consensus::RES_PROBABILITY_TARGET_CONST;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetSpacing = 60; // 1 minute
        consensus.nPowTargetTimespan = 30 * consensus.nPowTargetSpacing; // 30 blocks
        consensus.nPowASERTHalfLife = 1800; // 30 minutes (match regtest-style tuning)
        consensus.fPowAllowMinDifficultyBlocks = true;
        // Devtest enables ASERT retargeting (30-block window) to exercise nBits transitions under load changes.
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 27; // 90% of 30
        consensus.nMinerConfirmationWindow = 30;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0;
        // REMOVED: DEPLOYMENT_TAPROOT configuration
        // violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 complexity)

        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000000000000000002");
        consensus.defaultAssumeValid = uint256{};

        pchMessageStart[0] = 0x1d;
        pchMessageStart[1] = 0x2a;
        pchMessageStart[2] = 0x3b;
        pchMessageStart[3] = 0x4c;
        nDefaultPort = 48760;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        const std::string devtest_genesis_timestamp = "[Rigor Devtest] In V0 we trust, in V1 we execute — fast validation chain.";
        const std::vector<unsigned char> devtest_genesis_spk = ParseHex("00143333333333333333333333333333333333333333");
        const CScript devtest_genesis_output_script(devtest_genesis_spk.begin(), devtest_genesis_spk.end());
        genesis = CreateGenesisBlock(devtest_genesis_timestamp.c_str(), devtest_genesis_output_script, 1774915600, 2, 0x207fffff, 1, Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(genesis.hashMerkleRoot == BlockMerkleRoot(genesis));
        assert(genesis.GetHash() == consensus.hashGenesisBlock);
        // Hash recomputed after c43babf39f rebranded pszTimestamp to RIGOR wording (TASK-H5).
        assert(consensus.hashGenesisBlock == uint256S("0x407381b8179c92027826909806920e3633d5487cac816e8fff419223d30641c7"));

        checkpointData = {
            {
                {0, consensus.hashGenesisBlock},
            }
        };

        vFixedSeeds.clear();
        vSeeds.clear();

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "dvt";

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        m_is_test_chain = true;
        m_is_mockable_chain = true;

        m_assumeutxo_data = MapAssumeutxo{};
        chainTxData = ChainTxData{0, 0, 0};
    }
};

/**
 * Signet: test network with an additional consensus parameter (see BIP325).
 */
class SigNetParams : public CChainParams {
public:
    explicit SigNetParams(const SigNetOptions& options)
    {
        std::vector<uint8_t> bin;
        vSeeds.clear();

        if (!options.challenge) {
            bin = ParseHex("512103ad5e0edad18cb1f0fc0d28a3d4f1f3e445640337489abb10404f2d1e086be430210359ef5021964fe22d6f8e05b2463c9540ce96883fe3b278760f048f5189f2e6c452ae");
            vSeeds.emplace_back("seed.signet.bitcoin.sprovoost.nl.");

            // Hardcoded nodes can be removed once there are more DNS seeds
            vSeeds.emplace_back("178.128.221.177");
            vSeeds.emplace_back("v7ajjeirttkbnt32wpy3c6w3emwnfr3fkla7hpxcfokr3ysd3kqtzmqd.onion:38333");

            consensus.nMinimumChainWork = uint256{};
            consensus.defaultAssumeValid = uint256{};
            m_assumed_blockchain_size = 1;
            m_assumed_chain_state_size = 0;
            chainTxData = ChainTxData{0, 0, 0};
        } else {
            bin = *options.challenge;
            consensus.nMinimumChainWork = uint256{};
            consensus.defaultAssumeValid = uint256{};
            m_assumed_blockchain_size = 0;
            m_assumed_chain_state_size = 0;
            chainTxData = ChainTxData{
                0,
                0,
                0,
            };
            LogPrintf("Signet with challenge %s\n", HexStr(bin));
        }

        if (options.seeds) {
            vSeeds = *options.seeds;
        }

        strNetworkID = CBaseChainParams::SIGNET;
        consensus.signet_blocks = true;
        consensus.signet_challenge.assign(bin.begin(), bin.end());
        consensus.nSubsidyHalvingInterval = Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
        consensus.nBlockSubsidyBase = Consensus::ORU_INITIAL_BLOCK_SUBSIDY;
        consensus.nBlockSubsidyMaxHalvingShifts = Consensus::ORU_SUBSIDY_MAX_SHIFTS;
        consensus.nCoinbaseMaturity = 100;
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.nPowTargetSpacing = 10 * 60; // 10 minutes
        consensus.nPowTargetTimespan = 288 * consensus.nPowTargetSpacing; // 288 blocks
        consensus.nPowASERTHalfLife = Consensus::MAINNET_POW_ASERT_HALF_LIFE; // 2 days (same as mainnet)
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 259; // 90% of 288
        consensus.nMinerConfirmationWindow = 288; // nPowTargetTimespan / nPowTargetSpacing
        consensus.MinBIP9WarningHeight = 0;
        // RFC-014 v1.1 (Conv-Decouple-01): RES convergence eligibility constant
        // = floor(2^240 / MIN_CONVERGENCE_AMOUNT). RFC-014 v1.0 specified 2^240, which was
        // derived at f = 1 while the implemented weight is in satoshis; see SECURITY-001-A
        // finding A-6 and the derivation in consensus/params.h.
        // Identical across all networks; per RFC-014 §2.4 no per-network divergence.
        consensus.RES_PROBABILITY_TARGET = Consensus::RES_PROBABILITY_TARGET_CONST;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // REMOVED: DEPLOYMENT_TAPROOT configuration (signet)
        // violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 complexity)

        // message start is defined as the first 4 bytes of the sha256d of the block script
        HashWriter h{};
        h << consensus.signet_challenge;
        uint256 hash = h.GetHash();
        memcpy(pchMessageStart, hash.begin(), 4);

        nDefaultPort = 38760;
        nPruneAfterHeight = 1000;

        const std::string signet_genesis_timestamp = "Selection not computation. Convergence defines state. Naming is consensus.";
        const std::vector<unsigned char> signet_genesis_spk = ParseHex("00144444444444444444444444444444444444444444");
        const CScript signet_genesis_output_script(signet_genesis_spk.begin(), signet_genesis_spk.end());
        genesis = CreateGenesisBlock(signet_genesis_timestamp.c_str(), signet_genesis_output_script, 1778224715, 0, 0x207fffff, 1, Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(genesis.hashMerkleRoot == BlockMerkleRoot(genesis));
        assert(genesis.GetHash() == consensus.hashGenesisBlock);
        assert(consensus.hashGenesisBlock == uint256S("0x85b91b276d162f9fe04fec1f24ecf84c46bd027d75e4be1943412d221ff8f9a7"));
        // Genesis values are derived from the locked timestamp payload above.

        vFixedSeeds.clear();

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "srg";

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        m_is_test_chain = true;
        m_is_mockable_chain = false;
    }
};

/**
 * Regression test: intended for private networks only. Has minimal difficulty to ensure that
 * blocks can be found instantly.
 */
class CRegTestParams : public CChainParams
{
public:
    explicit CRegTestParams(const RegTestOptions& opts)
    {
        strNetworkID =  CBaseChainParams::REGTEST;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 150;
        consensus.nBlockSubsidyBase = Consensus::ORU_INITIAL_BLOCK_SUBSIDY;
        consensus.nBlockSubsidyMaxHalvingShifts = Consensus::ORU_SUBSIDY_MAX_SHIFTS;
        consensus.nCoinbaseMaturity = 100;
        consensus.BIP34Height = 1; // Always active unless overridden
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1;  // Always active unless overridden
        consensus.BIP66Height = 1;  // Always active unless overridden
        consensus.CSVHeight = 1;    // Always active unless overridden
        consensus.SegwitHeight = 0; // Always active unless overridden
        consensus.MinBIP9WarningHeight = 0;
        // RFC-014 v1.1 (Conv-Decouple-01): RES convergence eligibility constant
        // = floor(2^240 / MIN_CONVERGENCE_AMOUNT). RFC-014 v1.0 specified 2^240, which was
        // derived at f = 1 while the implemented weight is in satoshis; see SECURITY-001-A
        // finding A-6 and the derivation in consensus/params.h.
        // Identical across all networks; per RFC-014 §2.4 no per-network divergence.
        consensus.RES_PROBABILITY_TARGET = Consensus::RES_PROBABILITY_TARGET_CONST;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetSpacing = 10 * 60; // 10 minutes
        consensus.nPowTargetTimespan = 288 * consensus.nPowTargetSpacing; // 288 blocks
        consensus.nPowASERTHalfLife = 1800; // 30 minutes (ASERT regtest)
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 216; // 75% of 288
        consensus.nMinerConfirmationWindow = 288; // nPowTargetTimespan / nPowTargetSpacing

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // REMOVED: DEPLOYMENT_TAPROOT configuration (Rigor does not use Taproot)

        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000000000000000002");
        consensus.defaultAssumeValid = uint256{};

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 28760;
        nPruneAfterHeight = opts.fastprune ? 100 : 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        for (const auto& [dep, height] : opts.activation_heights) {
            switch (dep) {
            case Consensus::BuriedDeployment::DEPLOYMENT_SEGWIT:
                consensus.SegwitHeight = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_HEIGHTINCB:
                consensus.BIP34Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_DERSIG:
                consensus.BIP66Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_CLTV:
                consensus.BIP65Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_CSV:
                consensus.CSVHeight = int{height};
                break;
            }
        }

        for (const auto& [deployment_pos, version_bits_params] : opts.version_bits_parameters) {
            consensus.vDeployments[deployment_pos].nStartTime = version_bits_params.start_time;
            consensus.vDeployments[deployment_pos].nTimeout = version_bits_params.timeout;
            consensus.vDeployments[deployment_pos].min_activation_height = version_bits_params.min_activation_height;
        }

        const std::string regtest_genesis_timestamp = "Selection not computation. Convergence defines state. Naming is consensus.";
        const std::vector<unsigned char> regtest_genesis_spk = ParseHex("00145555555555555555555555555555555555555555");
        const CScript regtest_genesis_output_script(regtest_genesis_spk.begin(), regtest_genesis_spk.end());
        genesis = CreateGenesisBlock(regtest_genesis_timestamp.c_str(), regtest_genesis_output_script, 1778224715, 0, 0x207fffff, 1, Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(genesis.hashMerkleRoot == BlockMerkleRoot(genesis));
        assert(genesis.GetHash() == consensus.hashGenesisBlock);
        assert(consensus.hashGenesisBlock == uint256S("0xcc31d596fd01d27134f1ad83e50dc93699c984683acc420477ad986ffa5f348e"));
        // Genesis values are derived from the locked timestamp payload above.

        checkpointData = {
            {
                {0, consensus.hashGenesisBlock},
            }
        };

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();
        vSeeds.emplace_back("dummySeed.invalid.");

        fDefaultConsistencyChecks = true;
        fRequireStandard = true;
        m_is_test_chain = true;
        m_is_mockable_chain = true;

        // Serialized UTXO hash + nChainTx from TestChain100Setup + regtest genesis (see
        // validation_chainstate_tests.cpp `rigor_assumeutxo_param_gen` with RIGOR_GEN_ASSUMEUTXO=1).
        m_assumeutxo_data = MapAssumeutxo{
            {110,
             {AssumeutxoHash{uint256S("0x342bd84c45c2ec96fd0b7a65c0d48dc2fb10ec40663314d1cc2b5cf94393b841")},
             111}},
            {200,
             {AssumeutxoHash{uint256S("0x0d687545598f759039ed94edab89b6044472bcd9dd42497d80509e3701f6aef2")},
             201}},
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "rgrt";
    }
};

std::unique_ptr<const CChainParams> CChainParams::SigNet(const SigNetOptions& options)
{
    return std::make_unique<const SigNetParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::RegTest(const RegTestOptions& options)
{
    return std::make_unique<const CRegTestParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::Main()
{
    return std::make_unique<const CMainParams>();
}

std::unique_ptr<const CChainParams> CChainParams::TestNet()
{
    return std::make_unique<const CTestNetParams>();
}

std::unique_ptr<const CChainParams> CChainParams::DevTest()
{
    return std::make_unique<const CDevTestParams>();
}

/**
 * Slowtest: Low-speed test chain for realistic block time validation.
 */
class CSlowTestParams : public CChainParams {
public:
    CSlowTestParams() {
        strNetworkID = CBaseChainParams::SLOWTEST;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
        consensus.nBlockSubsidyBase = Consensus::ORU_INITIAL_BLOCK_SUBSIDY;
        consensus.nBlockSubsidyMaxHalvingShifts = Consensus::ORU_SUBSIDY_MAX_SHIFTS;
        consensus.nCoinbaseMaturity = 1; // Reduced for testing
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 0;
        consensus.MinBIP9WarningHeight = 0;
        // RFC-014 v1.1 (Conv-Decouple-01): RES convergence eligibility constant
        // = floor(2^240 / MIN_CONVERGENCE_AMOUNT). RFC-014 v1.0 specified 2^240, which was
        // derived at f = 1 while the implemented weight is in satoshis; see SECURITY-001-A
        // finding A-6 and the derivation in consensus/params.h.
        // Identical across all networks; per RFC-014 §2.4 no per-network divergence.
        consensus.RES_PROBABILITY_TARGET = Consensus::RES_PROBABILITY_TARGET_CONST;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetSpacing = 30; // 30 seconds target
        consensus.nPowTargetTimespan = 30 * consensus.nPowTargetSpacing; // 30 blocks
        consensus.nPowASERTHalfLife = 1800; // 30 minutes
        consensus.fPowAllowMinDifficultyBlocks = false; // Disable min-difficulty for real ASERT testing
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 27; // 90% of 30
        consensus.nMinerConfirmationWindow = 30;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0;
        // REMOVED: DEPLOYMENT_TAPROOT configuration (Rigor does not use Taproot)

        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000000000000000002");
        consensus.defaultAssumeValid = uint256{};

        pchMessageStart[0] = 0x2e;
        pchMessageStart[1] = 0x3c;
        pchMessageStart[2] = 0x4d;
        pchMessageStart[3] = 0x5e;
        nDefaultPort = 49760;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        const std::string slowtest_genesis_timestamp = "[Rigor Slowtest] In V0 we trust, in V1 we execute — low-speed validation chain.";
        const std::vector<unsigned char> slowtest_genesis_spk = ParseHex("00146666666666666666666666666666666666666666");
        const CScript slowtest_genesis_output_script(slowtest_genesis_spk.begin(), slowtest_genesis_spk.end());
        genesis = CreateGenesisBlock(slowtest_genesis_timestamp.c_str(), slowtest_genesis_output_script, 1777177700, 0, 0x1e0fffff, 1, Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(genesis.hashMerkleRoot == BlockMerkleRoot(genesis));
        assert(genesis.GetHash() == consensus.hashGenesisBlock);

        checkpointData = {
            {
                {0, consensus.hashGenesisBlock},
            }
        };

        vFixedSeeds.clear();
        vSeeds.clear();

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "slt";

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        m_is_test_chain = true;
        m_is_mockable_chain = true;

        m_assumeutxo_data = MapAssumeutxo{};
        chainTxData = ChainTxData{0, 0, 0};
    }
};

std::unique_ptr<const CChainParams> CChainParams::SlowTest()
{
    return std::make_unique<const CSlowTestParams>();
}
