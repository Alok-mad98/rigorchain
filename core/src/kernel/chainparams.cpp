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
#include <netaddress.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <streams.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <version.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace {
static_assert(Consensus::ORU_FLAT_BLOCK_SUBSIDY == 128 * COIN);
static_assert(Consensus::ORU_GENESIS_BLOCK_SUBSIDY == 0);

// Mainnet operator pubkey: mechanism is compiled in; the operator key
// ceremony is pending. This value is not an operational signing key until
// that ceremony publishes one. Secret material is not in this repository.
constexpr const char* SEED_MANIFEST_PUBKEY_MAIN =
    "d6520b135d07e7e3c83769cbb6aab02a778d887b0d487f8d5c9b884b71bb89f5";
// Test-only. Publicly derivable from SHA256("RIGOR_SEED_MANIFEST_TEST") as
// the Ed25519 seed. Do not treat this as a secret.
constexpr const char* SEED_MANIFEST_PUBKEY_TEST =
    "52ca3ecbacb34a495df3b2a47e30b758450efb0dbd99218a932826a2f98c8072";

void SetSeedManifestFields(std::array<unsigned char, 32>& pubkey,
                           std::vector<std::string>& epochs,
                           bool mainnet)
{
    const auto hex = ParseHex(mainnet ? SEED_MANIFEST_PUBKEY_MAIN : SEED_MANIFEST_PUBKEY_TEST);
    assert(hex.size() == pubkey.size());
    std::copy(hex.begin(), hex.end(), pubkey.begin());
    epochs = {"2026Q3", "2026Q4"};
}

// ADDRv2 encoding consumed by net.cpp ConvertSeeds. Not consensus.
std::vector<uint8_t> EncodeIPv4FixedSeed(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port)
{
    struct in_addr ipv4{};
    const uint8_t bytes[4] = {a, b, c, d};
    std::memcpy(&ipv4.s_addr, bytes, 4);
    CService endpoint{ipv4, port};
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION | ADDRV2_FORMAT);
    ss << endpoint;
    std::vector<uint8_t> out(ss.size());
    if (!out.empty()) {
        std::memcpy(out.data(), ss.data(), out.size());
    }
    return out;
}
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

/** Mainnet genesis header fields — RFC-016 Alpha1 freeze. Do not read from runtime configuration. */
namespace GenesisLockMainnet {
// RFC-016 §2: Alpha1 is a new chain (v0.1.1-alpha), not a hard fork of v0.1.0-alpha.
// ASERT anchors at height 1 (first mined block), so a stale genesis wall-clock no
// longer eases the chain to powLimit. Still keep TIMESTAMP near launch so MTP/locktime
// semantics match wall-clock; RFC-016 §6.3 historically forbade a refresh — pre-mainnet
// launch MUST still bump TIMESTAMP in a coordinated genesis freeze.
// Must be <= wall-clock UTC at launch. RFC-016 §6.3 forbids further timestamp refresh.
// Mainnet launch freeze: do not start the network before this instant has passed.
constexpr uint32_t TIMESTAMP = 1792908000; // 2026-10-25 06:00:00 UTC
constexpr uint32_t NONCE = 0;
// SPEC-008 §8.2. Target 2^223, so ~8.59e9 expected SHA3-256 trials. That is ~10 min
// on a typical 8-thread PC linking an optimized Keccak (~16 MH/s), which is the same
// design intent as the OpuHash-era 0x1e010000 (~1.68e7 trials at ~28 kH/s), rescaled
// by the measured cost ratio R = 2^9. Genesis is PoW-exempt and NONCE stays 0: this
// is the seed difficulty for block 1, not a mined proof.
constexpr uint32_t NBITS = 0x1d008000;
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
        // RFC-020: GetBlockSubsidy ignores Params. Observational calendar is RES_REWARD_EPOCH.
        consensus.nCoinbaseMaturity = 100;
        // Rigor mainnet uses a fresh genesis (genesis-lockdown.md). Buried softforks
        // are genesis-active (BIP34/65/66/CSV @1, Segwit @0); script flags are constant.
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 0;
        consensus.MinBIP9WarningHeight = 0;
        // 2^231 − 1 (SPEC-008 §8.2): genesis compact 0x1d008000 (2^223) remains legal;
        // the genesis/powLimit separation stays at 2^8, so ASERT keeps 8 half-lives of
        // easing headroom and the startup-time audit's "~47 steps to powLimit" is
        // unchanged. Floor ≈ 2^25 hashes so powLimit is not a free header — that is
        // the OpuHash-era 2^16 scaled by the measured cost ratio R = 2^9.
        consensus.powLimit = uint256S("0000007fffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetSpacing = 10 * 60; // 10 minutes
        consensus.nPowTargetTimespan = 288 * consensus.nPowTargetSpacing; // 288 blocks
        consensus.nPowASERTHalfLife = Consensus::MAINNET_POW_ASERT_HALF_LIFE; // 12 hours
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
         * Launch value: the chainwork of the genesis block alone, 2^33 - 1 = GetBlockProof
         * of 0x1d008000 (SPEC-008 §8.3.1). It must not exceed genesis work while the
         * chain is empty: a node whose tip is below this value stays in IBD, answers
         * getheaders with an empty headers message, routes peers' headers into PRESYNC
         * and drops their compact blocks, so blocks 1..k below the gate cannot propagate
         * and the first miner to reach the gate alone wins. At genesis work every
         * header above genesis carries work, block 1 relays normally, and the
         * 144-block tip-relative anti-DoS window takes over as the chain grows.
         *
         * What the gate buys: a peer stays in HeadersSyncState::PRESYNC — one bit
         * of memory per header — until its chain carries this much work. A header
         * counts only for the work it carries, and producing a header that passes
         * the PoW check at all costs exactly that work. Header count is not a
         * meaningful unit here; only work is. Do not reintroduce a header-count
         * invariant.
         *
         * Raise toward canonical tip chainwork in the first release after launch.
         */
        consensus.nMinimumChainWork = uint256S("0x00000000000000000000000000000000000000000000000000000001ffffffff");

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
        // UI disk-space hint only (not consensus). Alpha mainnet is far smaller than Bitcoin's ~540 GB.
        m_assumed_blockchain_size = 5;
        m_assumed_chain_state_size = 1;

        // Mainnet manifesto — scriptPubKey hex is fixed; PoW fields use GenesisLockMainnet::* only.
        const std::vector<unsigned char> mainnet_genesis_spk = ParseHex("00141111111111111111111111111111111111111111");
        const CScript genesis_output_script(mainnet_genesis_spk.begin(), mainnet_genesis_spk.end());
        genesis = CreateGenesisBlock(GenesisLockMainnet::TIMESTAMP_MSG, genesis_output_script,
            GenesisLockMainnet::TIMESTAMP, GenesisLockMainnet::NONCE, GenesisLockMainnet::NBITS,
            GenesisLockMainnet::VERSION, Consensus::ORU_GENESIS_BLOCK_SUBSIDY);
        consensus.hashGenesisBlock = genesis.GetHash();
        // Mainnet genesis timestamp/payload is customized for this chain.
        // Validate consistency against the computed merkle root instead of Bitcoin's legacy constant.
        assert(genesis.hashMerkleRoot == BlockMerkleRoot(genesis));
        assert(genesis.GetHash() == consensus.hashGenesisBlock);
        // P0-038 — Genesis ORU reward 0. Pre-P0-038 hash 1c2d6436…d758 is foreign; wipe datadir.
        assert(genesis.hashMerkleRoot == uint256S("0x9d3fbd207af927fc8e2c57e975d8d5845913bf059d956592e8a0bcf38b3515dd"));
        assert(consensus.hashGenesisBlock == uint256S("0xe2ce92cfef740f92e8cc84a8541d8e760c776c668afd9b61740b3a4315241468"));
        assert(genesis.vtx[0]->vout[0].nValue == Consensus::ORU_GENESIS_BLOCK_SUBSIDY);
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

        // DNS discovery only (not consensus). The only current Mainnet seed
        // hostname is seed.rigorchain.com. Node addresses come from DNS, not
        // from compiled vFixedSeeds, so a server can be replaced without a
        // wallet release. Lookups use nDefaultPort.
        vSeeds.clear();
        vSeeds.emplace_back("seed.rigorchain.com.");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,0);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "rg";

        // Keep compiled-in pnSeed empty until a signed epoch snapshot is published.
        vFixedSeeds.clear();
        SetSeedManifestFields(m_seed_manifest_pubkey, m_seed_epochs, /*mainnet=*/true);

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
 * Testnet: Rigor public proving ground — 1-minute blocks, buried deployments aligned
 * to Rigor (not Bitcoin testnet3). ASERT matches mainnet: same half-life, same
 * anchor, no min-difficulty reset. Only powLimit is easier.
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = CBaseChainParams::TESTNET;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        // RFC-020: GetBlockSubsidy ignores Params. Observational calendar is RES_REWARD_EPOCH.
        consensus.nCoinbaseMaturity = 100;
        // Policy: all buried softforks are genesis-active (same as mainnet).
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 0;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetSpacing = 60; // 1 minute nominal inter-block interval
        consensus.nPowTargetTimespan = 30 * consensus.nPowTargetSpacing; // 30-block BIP9 window
        // Same ASERT as mainnet. powLimit above is the only easier knob.
        consensus.nPowASERTHalfLife = Consensus::MAINNET_POW_ASERT_HALF_LIFE; // 12 hours
        consensus.fPowAllowMinDifficultyBlocks = false;
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
        genesis = CreateGenesisBlock(testnet_genesis_timestamp.c_str(), testnet_genesis_output_script, 1778224715, 0, 0x207fffff, 1, Consensus::ORU_GENESIS_BLOCK_SUBSIDY);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(genesis.hashMerkleRoot == BlockMerkleRoot(genesis));
        assert(genesis.GetHash() == consensus.hashGenesisBlock);
        assert(genesis.hashMerkleRoot == uint256S("0x54ace6b8a887ef617d5d583c7fb326a88805b7f7afba190de470898ea2885343"));
        assert(consensus.hashGenesisBlock == uint256S("0x0aca439758fba7fa5190b7e1cf37f9447516ef4cade07a49e7d484ea5b67b9ce"));
        assert(genesis.vtx[0]->vout[0].nValue == Consensus::ORU_GENESIS_BLOCK_SUBSIDY);
        // Genesis values are derived from the locked timestamp payload above.

        checkpointData = {
            {
                {0, consensus.hashGenesisBlock},
            }
        };

        // Private-testnet bootstrap. Mainnet still clears vFixedSeeds.
        // EXISTING endpoint: 127.0.0.1:18760 = node-001 when
        // tools/testnet/start-bootstrap.sh is running. No public operator IP
        // is compiled here (none exists). DNS names below are historical
        // strings; this tree does not claim they resolve — configs set dnsseed=0.
        // addrman skips IsLocal, so joiners also use -seednode/-addnode in
        // testnet/bootstrap/*.conf.
        vFixedSeeds = EncodeIPv4FixedSeed(127, 0, 0, 1, nDefaultPort);
        SetSeedManifestFields(m_seed_manifest_pubkey, m_seed_epochs, /*mainnet=*/false);
        // Testnet names only. Do not add the mainnet seed.rigorchain.com.
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
    explicit CDevTestParams(const MockableChainOptions& options)
    {
        strNetworkID = CBaseChainParams::DEVTEST;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        // RFC-020: GetBlockSubsidy ignores Params. Observational calendar is RES_REWARD_EPOCH.
        // Align with mainnet/signet: ORU and RES coinbase outputs mature after 100 confirmations.
        consensus.nCoinbaseMaturity = 100;
        // All buried softforks active from genesis / block 1 — no Bitcoin-era delayed heights.
        // BIP34 @1 gates the coinbase height push.
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 0;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetSpacing = 60; // 1 minute
        consensus.nPowTargetTimespan = 30 * consensus.nPowTargetSpacing; // 30 blocks
        consensus.nPowASERTHalfLife = 28800; // 8 hours (bounded next-block ease vs MAX_FUTURE_BLOCK_TIME)
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
        genesis = CreateGenesisBlock(devtest_genesis_timestamp.c_str(), devtest_genesis_output_script, 1774915600, 2, 0x207fffff, 1, Consensus::ORU_GENESIS_BLOCK_SUBSIDY);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(genesis.hashMerkleRoot == BlockMerkleRoot(genesis));
        assert(genesis.GetHash() == consensus.hashGenesisBlock);
        assert(genesis.hashMerkleRoot == uint256S("0xada94f709a0bb27dfb0d1862eb4c0962c8a7c000c1c6e21dc5a7841f869c0a93"));
        assert(consensus.hashGenesisBlock == uint256S("0x9762f59ecfe1050ca45e404dc95e32ae5a93b9a046e116f2f2f562e44f21dce0"));
        assert(genesis.vtx[0]->vout[0].nValue == Consensus::ORU_GENESIS_BLOCK_SUBSIDY);

        checkpointData = {
            {
                {0, consensus.hashGenesisBlock},
            }
        };

        vFixedSeeds.clear();
        SetSeedManifestFields(m_seed_manifest_pubkey, m_seed_epochs, /*mainnet=*/false);
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
        consensus.res_wallet_bootstrap_coinbase = options.res_wallet_bootstrap_coinbase;

        m_assumeutxo_data = MapAssumeutxo{};
        chainTxData = ChainTxData{0, 0, 0};
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
        // RFC-020: GetBlockSubsidy ignores Params. Observational calendar is RES_REWARD_EPOCH.
        consensus.nCoinbaseMaturity = 100;
        // Same genesis-active policy as public nets. BIP34/65/66/CSV @1 exempts
        // the genesis coinbase from BIP34 height-encoding; Segwit @0 is from genesis.
        // These cannot be overridden: -testactivationheight is not a registered argument.
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 0;
        consensus.MinBIP9WarningHeight = 0;
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

        const std::string regtest_genesis_timestamp = "Selection not computation. Convergence defines state. Naming is consensus.";
        const std::vector<unsigned char> regtest_genesis_spk = ParseHex("00145555555555555555555555555555555555555555");
        const CScript regtest_genesis_output_script(regtest_genesis_spk.begin(), regtest_genesis_spk.end());
        genesis = CreateGenesisBlock(regtest_genesis_timestamp.c_str(), regtest_genesis_output_script, 1778224715, 0, 0x207fffff, 1, Consensus::ORU_GENESIS_BLOCK_SUBSIDY);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(genesis.hashMerkleRoot == BlockMerkleRoot(genesis));
        assert(genesis.GetHash() == consensus.hashGenesisBlock);
        assert(genesis.hashMerkleRoot == uint256S("0xd3da525deee10cd11754b3269e919f8388184ec1f01a1222ad5419d00f0b43e3"));
        assert(consensus.hashGenesisBlock == uint256S("0x3070f47e2c1b153a3288382f3258396bb48b016eab76eae9f235d006dcc2af63"));
        assert(genesis.vtx[0]->vout[0].nValue == Consensus::ORU_GENESIS_BLOCK_SUBSIDY);
        // Genesis values are derived from the locked timestamp payload above.

        checkpointData = {
            {
                {0, consensus.hashGenesisBlock},
            }
        };

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        SetSeedManifestFields(m_seed_manifest_pubkey, m_seed_epochs, /*mainnet=*/false);
        vSeeds.clear();
        vSeeds.emplace_back("dummySeed.invalid.");

        fDefaultConsistencyChecks = true;
        fRequireStandard = true;
        m_is_test_chain = true;
        m_is_mockable_chain = true;
        consensus.res_wallet_bootstrap_coinbase = opts.res_wallet_bootstrap_coinbase;

        // Serialized UTXO hash + nChainTx from TestChain100Setup + regtest genesis (see
        // validation_chainstate_tests.cpp `rigor_assumeutxo_param_gen` with RIGOR_GEN_ASSUMEUTXO=1).
        m_assumeutxo_data = MapAssumeutxo{
            {110,
             {AssumeutxoHash{uint256S("0x70a2b06fdb2cac946a7818131150891d356116c4ea8d4ff48f665b3b542f2e49")},
             111}},
            {200,
             {AssumeutxoHash{uint256S("0x887ab6c056b5f1944bb574b5de03ed9e585992f3ce51fc3f571ef6e3d7abd72a")},
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

std::unique_ptr<const CChainParams> CChainParams::DevTest(const MockableChainOptions& options)
{
    return std::make_unique<const CDevTestParams>(options);
}

/**
 * Slowtest: Low-speed test chain for realistic block time validation.
 */
class CSlowTestParams : public CChainParams {
public:
    explicit CSlowTestParams(const MockableChainOptions& options)
    {
        strNetworkID = CBaseChainParams::SLOWTEST;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        // RFC-020: GetBlockSubsidy ignores Params. Observational calendar is RES_REWARD_EPOCH.
        consensus.nCoinbaseMaturity = 1; // Reduced for testing
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 0;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetSpacing = 30; // 30 seconds target
        consensus.nPowTargetTimespan = 30 * consensus.nPowTargetSpacing; // 30 blocks
        consensus.nPowASERTHalfLife = Consensus::MAINNET_POW_ASERT_HALF_LIFE; // 12 hours, no min-difficulty
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
        genesis = CreateGenesisBlock(slowtest_genesis_timestamp.c_str(), slowtest_genesis_output_script, 1777177700, 0, 0x1e0fffff, 1, Consensus::ORU_GENESIS_BLOCK_SUBSIDY);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(genesis.hashMerkleRoot == BlockMerkleRoot(genesis));
        assert(genesis.GetHash() == consensus.hashGenesisBlock);
        assert(genesis.hashMerkleRoot == uint256S("0x46ee1198cb3aeee9160bf0f88b1c99d167024fedbbf9e926d81a9f2c49d32c85"));
        assert(consensus.hashGenesisBlock == uint256S("0x18ddf722cf24b35ad9e3b548ddba22a2e54535fd5932d4d699fa9602f6d3efb8"));
        assert(genesis.vtx[0]->vout[0].nValue == Consensus::ORU_GENESIS_BLOCK_SUBSIDY);

        checkpointData = {
            {
                {0, consensus.hashGenesisBlock},
            }
        };

        vFixedSeeds.clear();
        SetSeedManifestFields(m_seed_manifest_pubkey, m_seed_epochs, /*mainnet=*/false);
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
        consensus.res_wallet_bootstrap_coinbase = options.res_wallet_bootstrap_coinbase;

        m_assumeutxo_data = MapAssumeutxo{};
        chainTxData = ChainTxData{0, 0, 0};
    }
};

std::unique_ptr<const CChainParams> CChainParams::SlowTest(const MockableChainOptions& options)
{
    return std::make_unique<const CSlowTestParams>(options);
}
