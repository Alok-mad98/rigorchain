// Copyright (c) 2010 The RigorChain developers
// Copyright (c) 2009-2022 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/blockchain.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <key_io.h>
#include <chainparams.h>
#include <clientversion.h>
#include <version.h>
#include <coins.h>
#include <consensus/amount.h>
#include <blockfilter.h>
#include <chain.h>
#include <consensus/convergenceres.h>
#include <consensus/res_issuance.h>
#include <consensus/params.h>
#include <consensus/replay_snapshot.h>
#include <node/supply_monitor/runtime_supply_export.h>
#include <node/supply_monitor/runtime_supply_invariant_checker.h>
#include <node/supply_monitor/runtime_supply_witness_chain.h>
#include <node/supply_monitor/runtime_supply_witness_coq_adapter.h>
#include <node/supply_monitor/runtime_supply_coq_chas_adapter.h>
#include <node/supply_monitor/runtime_supply_witness_equivalence_candidate.h>
#include <node/supply_monitor/runtime_supply_global_candidate_equivalence.h>
#include <node/supply_monitor/runtime_supply_equivalence_closure.h>
#include <node/supply_monitor/runtime_supply_phase5_summary.h>
#include <proof/runtime_vector_harness.h>
#include <consensus/state_diff.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <deploymentinfo.h>
#include <deploymentstatus.h>
#include <logging.h>
#include <script/conv_sign_trace.h>
#include <index/blockfilterindex.h>
#include <index/coinstatsindex.h>
#include <kernel/coinstats.h>
#include <logging/timer.h>
#include <net.h>
#include <net_processing.h>
#include <node/blockstorage.h>
#include <node/context.h>
#include <node/transaction.h>
#include <node/utxo_snapshot.h>
#include <rigor_kernel/pow_digest.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <rpc/server.h>
#include <rpc/server_util.h>
#include <rpc/util.h>
#include <script/standard.h>
#include <script/descriptor.h>
#include <streams.h>
#include <sync.h>
#include <txdb.h>
#include <txmempool.h>
#include <undo.h>
#include <univalue.h>
#include <util/moneystr.h>
#include <util/strencodings.h>
#include <util/fs.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <util/translation.h>
#include <validation.h>
#include <validationinterface.h>
#include <versionbits.h>
#include <warnings.h>

#include <stdint.h>

#include <condition_variable>
#include <algorithm>
#include <memory>
#include <mutex>

using kernel::CCoinsStats;
using kernel::CoinStatsHashType;

using node::BlockManager;
using node::NodeContext;
using node::ReadBlockFromDisk;
using node::SnapshotMetadata;
using node::UndoReadFromDisk;

struct CUpdatedBlock
{
    uint256 hash;
    int height;
};

static GlobalMutex cs_blockchange;
static std::condition_variable cond_blockchange;
static CUpdatedBlock latestblock GUARDED_BY(cs_blockchange);

/** Sum of `m_block_res_mint_sum` along the active tip chain; cached by tip hash (RPC display only). */
static Mutex g_cached_tip_res_mint_total_mutex;
static uint256 g_cached_tip_res_mint_total_block_hash;
static CAmount g_cached_tip_res_mint_total_value{0};
static bool g_cached_tip_res_mint_total_valid{false};

static CAmount CachedActiveTipCumulativeResMinted(const CBlockIndex* tip) EXCLUSIVE_LOCKS_REQUIRED(::cs_main)
{
    if (!tip) return 0;
    const uint256 h = tip->GetBlockHash();
    LOCK(g_cached_tip_res_mint_total_mutex);
    if (g_cached_tip_res_mint_total_valid && h == g_cached_tip_res_mint_total_block_hash) {
        return g_cached_tip_res_mint_total_value;
    }
    CAmount sum = 0;
    for (const CBlockIndex* p = tip; p; p = p->pprev) {
        sum += p->m_block_res_mint_sum;
    }
    g_cached_tip_res_mint_total_block_hash = h;
    g_cached_tip_res_mint_total_value = sum;
    g_cached_tip_res_mint_total_valid = true;
    return sum;
}

/* Calculate the difficulty for a given block index.
 */
double GetDifficulty(const CBlockIndex* blockindex)
{
    CHECK_NONFATAL(blockindex);

    int nShift = (blockindex->nBits >> 24) & 0xff;
    double dDiff =
        (double)0x0000ffff / (double)(blockindex->nBits & 0x00ffffff);

    while (nShift < 29)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

static int ComputeNextBlockAndDepth(const CBlockIndex* tip, const CBlockIndex* blockindex, const CBlockIndex*& next)
{
    next = tip->GetAncestor(blockindex->nHeight + 1);
    if (next && next->pprev == blockindex) {
        return tip->nHeight - blockindex->nHeight + 1;
    }
    next = nullptr;
    return blockindex == tip ? 1 : -1;
}

static const CBlockIndex* ParseHashOrHeight(const UniValue& param, ChainstateManager& chainman)
{
    LOCK(::cs_main);
    CChain& active_chain = chainman.ActiveChain();

    if (param.isNum()) {
        const int height{param.getInt<int>()};
        if (height < 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Target block height %d is negative", height));
        }
        const int current_tip{active_chain.Height()};
        if (height > current_tip) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Target block height %d after current tip %d", height, current_tip));
        }

        return active_chain[height];
    } else {
        const uint256 hash{ParseHashV(param, "hash_or_height")};
        const CBlockIndex* pindex = chainman.m_blockman.LookupBlockIndex(hash);

        if (!pindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }

        return pindex;
    }
}

UniValue blockheaderToJSON(const CBlockIndex* tip, const CBlockIndex* blockindex)
{
    // Serialize passed information without accessing chain state of the active chain!
    AssertLockNotHeld(cs_main); // For performance reasons

    UniValue result(UniValue::VOBJ);
    result.pushKV("hash", blockindex->GetBlockHash().GetHex());
    result.pushKV("pow_hash", rigor_kernel::GetRigorPowHash(blockindex->GetBlockHeader()).GetHex());
    const CBlockIndex* pnext;
    int confirmations = ComputeNextBlockAndDepth(tip, blockindex, pnext);
    result.pushKV("confirmations", confirmations);
    result.pushKV("height", blockindex->nHeight);
    result.pushKV("version", blockindex->nVersion);
    result.pushKV("versionHex", strprintf("%08x", blockindex->nVersion));
    result.pushKV("merkleroot", blockindex->hashMerkleRoot.GetHex());
    result.pushKV("time", (int64_t)blockindex->nTime);
    result.pushKV("mediantime", (int64_t)blockindex->GetMedianTimePast());
    result.pushKV("nonce", (uint64_t)blockindex->nNonce);
    result.pushKV("cumulative_convergence", ValueFromAmount(blockindex->Observational().GetCumulativeConvergence()));
    result.pushKV("rigor_res_minted", static_cast<int>(blockindex->has_res));
    result.pushKV("bits", strprintf("%08x", blockindex->nBits));
    result.pushKV("difficulty", GetDifficulty(blockindex));
    result.pushKV("chainwork", blockindex->nChainWork.GetHex());
    result.pushKV("nTx", (uint64_t)blockindex->nTx);

    if (blockindex->pprev)
        result.pushKV("previousblockhash", blockindex->pprev->GetBlockHash().GetHex());
    if (pnext)
        result.pushKV("nextblockhash", pnext->GetBlockHash().GetHex());
    return result;
}

UniValue blockToJSON(BlockManager& blockman, const CBlock& block, const CBlockIndex* tip, const CBlockIndex* blockindex, ChainstateManager& chainman, const Consensus::Params& consensus_params, TxVerbosity verbosity)
{
    UniValue result = blockheaderToJSON(tip, blockindex);

    result.pushKV("strippedsize", (int)::GetSerializeSize(block, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS));
    result.pushKV("size", (int)::GetSerializeSize(block, PROTOCOL_VERSION));
    result.pushKV("weight", (int)::GetBlockWeight(block));
    UniValue txs(UniValue::VARR);

    switch (verbosity) {
        case TxVerbosity::SHOW_TXID:
            for (const CTransactionRef& tx : block.vtx) {
                txs.push_back(tx->GetHash().GetHex());
            }
            break;

        case TxVerbosity::SHOW_DETAILS:
        case TxVerbosity::SHOW_DETAILS_AND_PREVOUT:
            CBlockUndo blockUndo;
            const bool is_not_pruned{WITH_LOCK(::cs_main, return !blockman.IsBlockPruned(blockindex))};
            const bool have_undo{is_not_pruned && UndoReadFromDisk(blockUndo, blockindex)};

            for (size_t i = 0; i < block.vtx.size(); ++i) {
                const CTransactionRef& tx = block.vtx.at(i);
                // coinbase transaction (i.e. i == 0) doesn't have undo data
                const CTxUndo* txundo = (have_undo && i > 0) ? &blockUndo.vtxundo.at(i - 1) : nullptr;
                UniValue objTx(UniValue::VOBJ);
                TxToUniv(*tx, /*block_hash=*/uint256(), /*entry=*/objTx, /*include_hex=*/true, RPCSerializationFlags(), txundo, verbosity);
                txs.push_back(objTx);
            }
            break;
    }

    result.pushKV("tx", txs);

    return result;
}

static RPCHelpMan getblockcount()
{
    return RPCHelpMan{"getblockcount",
                "\nReturns the height of the most-work fully-validated chain.\n"
                "The genesis block has height 0.\n",
                {},
                RPCResult{
                    RPCResult::Type::NUM, "", "The current block count"},
                RPCExamples{
                    HelpExampleCli("getblockcount", "")
            + HelpExampleRpc("getblockcount", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    LOCK(cs_main);
    return chainman.ActiveChain().Height();
},
    };
}

static RPCHelpMan getbestblockhash()
{
    return RPCHelpMan{"getbestblockhash",
                "\nReturns the hash of the best (tip) block in the most-work fully-validated chain.\n",
                {},
                RPCResult{
                    RPCResult::Type::STR_HEX, "", "the block hash, hex-encoded"},
                RPCExamples{
                    HelpExampleCli("getbestblockhash", "")
            + HelpExampleRpc("getbestblockhash", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    LOCK(cs_main);
    return chainman.ActiveChain().Tip()->GetBlockHash().GetHex();
},
    };
}

void RPCNotifyBlockChange(const CBlockIndex* pindex)
{
    if(pindex) {
        LOCK(cs_blockchange);
        latestblock.hash = pindex->GetBlockHash();
        latestblock.height = pindex->nHeight;
    }
    cond_blockchange.notify_all();
}

static RPCHelpMan waitfornewblock()
{
    return RPCHelpMan{"waitfornewblock",
                "\nWaits for a specific new block and returns useful info about it.\n"
                "\nReturns the current block on timeout or exit.\n",
                {
                    {"timeout", RPCArg::Type::NUM, RPCArg::Default{0}, "Time in milliseconds to wait for a response. 0 indicates no timeout."},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR_HEX, "hash", "The blockhash"},
                        {RPCResult::Type::NUM, "height", "Block height"},
                    }},
                RPCExamples{
                    HelpExampleCli("waitfornewblock", "1000")
            + HelpExampleRpc("waitfornewblock", "1000")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    int timeout = 0;
    if (!request.params[0].isNull())
        timeout = request.params[0].getInt<int>();

    CUpdatedBlock block;
    {
        WAIT_LOCK(cs_blockchange, lock);
        block = latestblock;
        if(timeout)
            cond_blockchange.wait_for(lock, std::chrono::milliseconds(timeout), [&block]() EXCLUSIVE_LOCKS_REQUIRED(cs_blockchange) {return latestblock.height != block.height || latestblock.hash != block.hash || !IsRPCRunning(); });
        else
            cond_blockchange.wait(lock, [&block]() EXCLUSIVE_LOCKS_REQUIRED(cs_blockchange) {return latestblock.height != block.height || latestblock.hash != block.hash || !IsRPCRunning(); });
        block = latestblock;
    }
    UniValue ret(UniValue::VOBJ);
    ret.pushKV("hash", block.hash.GetHex());
    ret.pushKV("height", block.height);
    return ret;
},
    };
}

static RPCHelpMan waitforblock()
{
    return RPCHelpMan{"waitforblock",
                "\nWaits for a specific new block and returns useful info about it.\n"
                "\nReturns the current block on timeout or exit.\n",
                {
                    {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "Block hash to wait for."},
                    {"timeout", RPCArg::Type::NUM, RPCArg::Default{0}, "Time in milliseconds to wait for a response. 0 indicates no timeout."},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR_HEX, "hash", "The blockhash"},
                        {RPCResult::Type::NUM, "height", "Block height"},
                    }},
                RPCExamples{
                    HelpExampleCli("waitforblock", "\"0000000000079f8ef3d2c688c244eb7a4570b24c9ed7b4a8c619eb02596f8862\" 1000")
            + HelpExampleRpc("waitforblock", "\"0000000000079f8ef3d2c688c244eb7a4570b24c9ed7b4a8c619eb02596f8862\", 1000")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    int timeout = 0;

    uint256 hash(ParseHashV(request.params[0], "blockhash"));

    if (!request.params[1].isNull())
        timeout = request.params[1].getInt<int>();

    CUpdatedBlock block;
    {
        WAIT_LOCK(cs_blockchange, lock);
        if(timeout)
            cond_blockchange.wait_for(lock, std::chrono::milliseconds(timeout), [&hash]() EXCLUSIVE_LOCKS_REQUIRED(cs_blockchange) {return latestblock.hash == hash || !IsRPCRunning();});
        else
            cond_blockchange.wait(lock, [&hash]() EXCLUSIVE_LOCKS_REQUIRED(cs_blockchange) {return latestblock.hash == hash || !IsRPCRunning(); });
        block = latestblock;
    }

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("hash", block.hash.GetHex());
    ret.pushKV("height", block.height);
    return ret;
},
    };
}

static RPCHelpMan waitforblockheight()
{
    return RPCHelpMan{"waitforblockheight",
                "\nWaits for (at least) block height and returns the height and hash\n"
                "of the current tip.\n"
                "\nReturns the current block on timeout or exit.\n",
                {
                    {"height", RPCArg::Type::NUM, RPCArg::Optional::NO, "Block height to wait for."},
                    {"timeout", RPCArg::Type::NUM, RPCArg::Default{0}, "Time in milliseconds to wait for a response. 0 indicates no timeout."},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR_HEX, "hash", "The blockhash"},
                        {RPCResult::Type::NUM, "height", "Block height"},
                    }},
                RPCExamples{
                    HelpExampleCli("waitforblockheight", "100 1000")
            + HelpExampleRpc("waitforblockheight", "100, 1000")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    int timeout = 0;

    int height = request.params[0].getInt<int>();

    if (!request.params[1].isNull())
        timeout = request.params[1].getInt<int>();

    CUpdatedBlock block;
    {
        WAIT_LOCK(cs_blockchange, lock);
        if(timeout)
            cond_blockchange.wait_for(lock, std::chrono::milliseconds(timeout), [&height]() EXCLUSIVE_LOCKS_REQUIRED(cs_blockchange) {return latestblock.height >= height || !IsRPCRunning();});
        else
            cond_blockchange.wait(lock, [&height]() EXCLUSIVE_LOCKS_REQUIRED(cs_blockchange) {return latestblock.height >= height || !IsRPCRunning(); });
        block = latestblock;
    }
    UniValue ret(UniValue::VOBJ);
    ret.pushKV("hash", block.hash.GetHex());
    ret.pushKV("height", block.height);
    return ret;
},
    };
}

static RPCHelpMan syncwithvalidationinterfacequeue()
{
    return RPCHelpMan{"syncwithvalidationinterfacequeue",
                "\nWaits for the validation interface queue to catch up on everything that was there when we entered this function.\n",
                {},
                RPCResult{RPCResult::Type::NONE, "", ""},
                RPCExamples{
                    HelpExampleCli("syncwithvalidationinterfacequeue","")
            + HelpExampleRpc("syncwithvalidationinterfacequeue","")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    SyncWithValidationInterfaceQueue();
    return UniValue::VNULL;
},
    };
}

static RPCHelpMan getdifficulty()
{
    return RPCHelpMan{"getdifficulty",
                "\nReturns the proof-of-work difficulty as a multiple of the minimum difficulty.\n",
                {},
                RPCResult{
                    RPCResult::Type::NUM, "", "the proof-of-work difficulty as a multiple of the minimum difficulty."},
                RPCExamples{
                    HelpExampleCli("getdifficulty", "")
            + HelpExampleRpc("getdifficulty", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    LOCK(cs_main);
    return GetDifficulty(chainman.ActiveChain().Tip());
},
    };
}

static RPCHelpMan getblockfrompeer()
{
    return RPCHelpMan{
        "getblockfrompeer",
        "Attempt to fetch block from a given peer.\n\n"
        "We must have the header for this block, e.g. using submitheader.\n"
        "Subsequent calls for the same block may cause the response from the previous peer to be ignored.\n"
        "Peers generally ignore requests for a stale block that they never fully verified, or one that is more than a month old.\n"
        "When a peer does not respond with a block, we will disconnect.\n"
        "Note: The block could be re-pruned as soon as it is received.\n\n"
        "Returns an empty JSON object if the request was successfully scheduled.",
        {
            {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The block hash to try to fetch"},
            {"peer_id", RPCArg::Type::NUM, RPCArg::Optional::NO, "The peer to fetch it from (see getpeerinfo for peer IDs)"},
        },
        RPCResult{RPCResult::Type::OBJ, "", /*optional=*/false, "", {}},
        RPCExamples{
            HelpExampleCli("getblockfrompeer", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\" 0")
            + HelpExampleRpc("getblockfrompeer", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\" 0")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    const NodeContext& node = EnsureAnyNodeContext(request.context);
    ChainstateManager& chainman = EnsureChainman(node);
    PeerManager& peerman = EnsurePeerman(node);

    const uint256& block_hash{ParseHashV(request.params[0], "blockhash")};
    const NodeId peer_id{request.params[1].getInt<int64_t>()};

    const CBlockIndex* const index = WITH_LOCK(cs_main, return chainman.m_blockman.LookupBlockIndex(block_hash););

    if (!index) {
        throw JSONRPCError(RPC_MISC_ERROR, "Block header missing");
    }

    // Fetching blocks before the node has syncing past their height can prevent block files from
    // being pruned, so we avoid it if the node is in prune mode.
    if (chainman.m_blockman.IsPruneMode() && index->nHeight > WITH_LOCK(chainman.GetMutex(), return chainman.ActiveTip()->nHeight)) {
        throw JSONRPCError(RPC_MISC_ERROR, "In prune mode, only blocks that the node has already synced previously can be fetched from a peer");
    }

    const bool block_has_data = WITH_LOCK(::cs_main, return index->nStatus & BLOCK_HAVE_DATA);
    if (block_has_data) {
        throw JSONRPCError(RPC_MISC_ERROR, "Block already downloaded");
    }

    if (const auto err{peerman.FetchBlock(peer_id, *index)}) {
        throw JSONRPCError(RPC_MISC_ERROR, err.value());
    }
    return UniValue::VOBJ;
},
    };
}

static RPCHelpMan getblockhash()
{
    return RPCHelpMan{"getblockhash",
                "\nReturns hash of block in best-block-chain at height provided.\n",
                {
                    {"height", RPCArg::Type::NUM, RPCArg::Optional::NO, "The height index"},
                },
                RPCResult{
                    RPCResult::Type::STR_HEX, "", "The block hash"},
                RPCExamples{
                    HelpExampleCli("getblockhash", "1000")
            + HelpExampleRpc("getblockhash", "1000")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    LOCK(cs_main);
    const CChain& active_chain = chainman.ActiveChain();

    int nHeight = request.params[0].getInt<int>();
    if (nHeight < 0 || nHeight > active_chain.Height())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");

    const CBlockIndex* pblockindex = active_chain[nHeight];
    return pblockindex->GetBlockHash().GetHex();
},
    };
}

static RPCHelpMan getblockheader()
{
    return RPCHelpMan{"getblockheader",
                "\nIf verbose is false, returns a string that is serialized, hex-encoded data for blockheader 'hash'.\n"
                "If verbose is true, returns an Object with information about blockheader <hash>.\n",
                {
                    {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The block hash"},
                    {"verbose", RPCArg::Type::BOOL, RPCArg::Default{true}, "true for a json object, false for the hex-encoded data"},
                },
                {
                    RPCResult{"for verbose = true",
                        RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::STR_HEX, "hash", "the block hash (same as provided)"},
                            {RPCResult::Type::STR_HEX, "pow_hash", "SHA3-256 proof-of-work digest of the 80-byte header (SPEC-008), compared against the target"},
                            {RPCResult::Type::NUM, "confirmations", "The number of confirmations, or -1 if the block is not on the main chain"},
                            {RPCResult::Type::NUM, "height", "The block height or index"},
                            {RPCResult::Type::NUM, "version", "The block version"},
                            {RPCResult::Type::STR_HEX, "versionHex", "The block version formatted in hexadecimal"},
                            {RPCResult::Type::STR_HEX, "merkleroot", "The merkle root"},
                            {RPCResult::Type::NUM_TIME, "time", "The block time expressed in " + UNIX_EPOCH_TIME},
                            {RPCResult::Type::NUM_TIME, "mediantime", "The median block time expressed in " + UNIX_EPOCH_TIME},
                            {RPCResult::Type::NUM, "nonce", "The nonce"},
                            {RPCResult::Type::STR_AMOUNT, "cumulative_convergence", "Cumulative convergence in commitment (or 0 if absent)"},
                            {RPCResult::Type::NUM, "rigor_res_minted", "1 if this block included a body RES mint (convergence cycle), else 0"},
                            {RPCResult::Type::STR_HEX, "bits", "The bits"},
                            {RPCResult::Type::NUM, "difficulty", "The difficulty"},
                            {RPCResult::Type::STR_HEX, "chainwork", "Expected number of hashes required to produce the current chain"},
                            {RPCResult::Type::NUM, "nTx", "The number of transactions in the block"},
                            {RPCResult::Type::STR_HEX, "previousblockhash", /*optional=*/true, "The hash of the previous block (if available)"},
                            {RPCResult::Type::STR_HEX, "nextblockhash", /*optional=*/true, "The hash of the next block (if available)"},
                        }},
                    RPCResult{"for verbose=false",
                        RPCResult::Type::STR_HEX, "", "A string that is serialized, hex-encoded data for block 'hash'"},
                },
                RPCExamples{
                    HelpExampleCli("getblockheader", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
            + HelpExampleRpc("getblockheader", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    uint256 hash(ParseHashV(request.params[0], "hash"));

    bool fVerbose = true;
    if (!request.params[1].isNull())
        fVerbose = request.params[1].get_bool();

    const CBlockIndex* pblockindex;
    const CBlockIndex* tip;
    {
        ChainstateManager& chainman = EnsureAnyChainman(request.context);
        LOCK(cs_main);
        pblockindex = chainman.m_blockman.LookupBlockIndex(hash);
        tip = chainman.ActiveChain().Tip();
    }

    if (!pblockindex) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
    }

    if (!fVerbose)
    {
        DataStream ssBlock{};
        ssBlock << pblockindex->GetBlockHeader();
        std::string strHex = HexStr(ssBlock);
        return strHex;
    }

    return blockheaderToJSON(tip, pblockindex);
},
    };
}

static CBlock GetBlockChecked(BlockManager& blockman, const CBlockIndex* pblockindex)
{
    CBlock block;
    {
        LOCK(cs_main);
        if (blockman.IsBlockPruned(pblockindex)) {
            throw JSONRPCError(RPC_MISC_ERROR, "Block not available (pruned data)");
        }
    }

    if (!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus())) {
        // Block not found on disk. This could be because we have the block
        // header in our index but not yet have the block or did not accept the
        // block. Or if the block was pruned right after we released the lock above.
        throw JSONRPCError(RPC_MISC_ERROR, "Block not found on disk");
    }

    return block;
}

static CBlockUndo GetUndoChecked(BlockManager& blockman, const CBlockIndex* pblockindex)
{
    CBlockUndo blockUndo;

    // The Genesis block does not have undo data
    if (pblockindex->nHeight == 0) return blockUndo;

    {
        LOCK(cs_main);
        if (blockman.IsBlockPruned(pblockindex)) {
            throw JSONRPCError(RPC_MISC_ERROR, "Undo data not available (pruned data)");
        }
    }

    if (!UndoReadFromDisk(blockUndo, pblockindex)) {
        throw JSONRPCError(RPC_MISC_ERROR, "Can't read undo data from disk");
    }

    return blockUndo;
}

const RPCResult getblock_vin{
    RPCResult::Type::ARR, "vin", "",
    {
        {RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::ELISION, "", "The same output as verbosity = 2"},
            {RPCResult::Type::OBJ, "prevout", "(Only if undo information is available)",
            {
                {RPCResult::Type::BOOL, "generated", "Coinbase or not"},
                {RPCResult::Type::NUM, "height", "The height of the prevout"},
                {RPCResult::Type::STR_AMOUNT, "value", "The value in " + CURRENCY_UNIT},
                {RPCResult::Type::OBJ, "scriptPubKey", "",
                {
                    {RPCResult::Type::STR, "asm", "Disassembly of the public key script"},
                    {RPCResult::Type::STR, "desc", "Inferred descriptor for the output"},
                    {RPCResult::Type::STR_HEX, "hex", "The raw public key script bytes, hex-encoded"},
                    {RPCResult::Type::STR, "address", /*optional=*/true, "The Rigor address (only if a well-defined address exists)"},
                    {RPCResult::Type::STR, "type", "The type (one of: " + GetAllOutputTypes() + ")"},
                }},
            }},
        }},
    }
};

static RPCHelpMan getblock()
{
    return RPCHelpMan{"getblock",
                "\nIf verbosity is 0, returns a string that is serialized, hex-encoded data for block 'hash'.\n"
                "If verbosity is 1, returns an Object with information about block <hash>.\n"
                "If verbosity is 2, returns an Object with information about block <hash> and information about each transaction.\n"
                "If verbosity is 3, returns an Object with information about block <hash> and information about each transaction, including prevout information for inputs (only for unpruned blocks in the current best chain).\n",
                {
                    {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The block hash"},
                    {"verbosity|verbose", RPCArg::Type::NUM, RPCArg::Default{1}, "0 for hex-encoded data, 1 for a JSON object, 2 for JSON object with transaction data, and 3 for JSON object with transaction data including prevout information for inputs",
                     RPCArgOptions{.skip_type_check = true}},
                },
                {
                    RPCResult{"for verbosity = 0",
                RPCResult::Type::STR_HEX, "", "A string that is serialized, hex-encoded data for block 'hash'"},
                    RPCResult{"for verbosity = 1",
                RPCResult::Type::OBJ, "", "",
                {
                    {RPCResult::Type::STR_HEX, "hash", "the block hash (same as provided)"},
                    {RPCResult::Type::STR_HEX, "pow_hash", "SHA3-256 proof-of-work digest of the 80-byte header (SPEC-008), compared against the target"},
                    {RPCResult::Type::NUM, "confirmations", "The number of confirmations, or -1 if the block is not on the main chain"},
                    {RPCResult::Type::NUM, "size", "The block size"},
                    {RPCResult::Type::NUM, "strippedsize", "The block size excluding witness data"},
                    {RPCResult::Type::NUM, "weight", "The block weight as defined in BIP 141"},
                    {RPCResult::Type::NUM, "height", "The block height or index"},
                    {RPCResult::Type::NUM, "version", "The block version"},
                    {RPCResult::Type::STR_HEX, "versionHex", "The block version formatted in hexadecimal"},
                    {RPCResult::Type::STR_HEX, "merkleroot", "The merkle root"},
                    {RPCResult::Type::ARR, "tx", "The transaction ids",
                        {{RPCResult::Type::STR_HEX, "", "The transaction id"}}},
                    {RPCResult::Type::NUM_TIME, "time",       "The block time expressed in " + UNIX_EPOCH_TIME},
                    {RPCResult::Type::NUM_TIME, "mediantime", "The median block time expressed in " + UNIX_EPOCH_TIME},
                    {RPCResult::Type::NUM, "nonce", "The nonce"},
                    {RPCResult::Type::STR_AMOUNT, "cumulative_convergence", "Cumulative convergence in commitment (or 0 if absent)"},
                    {RPCResult::Type::NUM, "rigor_res_minted", "1 if this block included a body RES mint (convergence cycle), else 0"},
                    {RPCResult::Type::STR_HEX, "bits", "The bits"},
                    {RPCResult::Type::NUM, "difficulty", "The difficulty"},
                    {RPCResult::Type::STR_HEX, "chainwork", "Expected number of hashes required to produce the chain up to this block (in hex)"},
                    {RPCResult::Type::NUM, "nTx", "The number of transactions in the block"},
                    {RPCResult::Type::STR_HEX, "previousblockhash", /*optional=*/true, "The hash of the previous block (if available)"},
                    {RPCResult::Type::STR_HEX, "nextblockhash", /*optional=*/true, "The hash of the next block (if available)"},
                }},
                    RPCResult{"for verbosity = 2",
                RPCResult::Type::OBJ, "", "",
                {
                    {RPCResult::Type::ELISION, "", "Same output as verbosity = 1"},
                    {RPCResult::Type::ARR, "tx", "",
                    {
                        {RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::ELISION, "", "The transactions in the format of the getrawtransaction RPC. Different from verbosity = 1 \"tx\" result"},
                            {RPCResult::Type::NUM, "fee", "The transaction fee in " + CURRENCY_UNIT + ", omitted if block undo data is not available"},
                        }},
                    }},
                }},
                    RPCResult{"for verbosity = 3",
                RPCResult::Type::OBJ, "", "",
                {
                    {RPCResult::Type::ELISION, "", "Same output as verbosity = 2"},
                    {RPCResult::Type::ARR, "tx", "",
                    {
                        {RPCResult::Type::OBJ, "", "",
                        {
                            getblock_vin,
                        }},
                    }},
                }},
        },
                RPCExamples{
                    HelpExampleCli("getblock", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
            + HelpExampleRpc("getblock", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    uint256 hash(ParseHashV(request.params[0], "blockhash"));

    int verbosity = 1;
    if (!request.params[1].isNull()) {
        if (request.params[1].isBool()) {
            verbosity = request.params[1].get_bool() ? 1 : 0;
        } else {
            verbosity = request.params[1].getInt<int>();
        }
    }

    const CBlockIndex* pblockindex;
    const CBlockIndex* tip;
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    {
        LOCK(cs_main);
        pblockindex = chainman.m_blockman.LookupBlockIndex(hash);
        tip = chainman.ActiveChain().Tip();

        if (!pblockindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }
    }

    const CBlock block{GetBlockChecked(chainman.m_blockman, pblockindex)};

    if (verbosity <= 0)
    {
        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION | RPCSerializationFlags());
        ssBlock << block;
        std::string strHex = HexStr(ssBlock);
        return strHex;
    }

    TxVerbosity tx_verbosity;
    if (verbosity == 1) {
        tx_verbosity = TxVerbosity::SHOW_TXID;
    } else if (verbosity == 2) {
        tx_verbosity = TxVerbosity::SHOW_DETAILS;
    } else {
        tx_verbosity = TxVerbosity::SHOW_DETAILS_AND_PREVOUT;
    }

    return blockToJSON(chainman.m_blockman, block, tip, pblockindex, chainman, chainman.GetParams().GetConsensus(), tx_verbosity);
},
    };
}

static RPCHelpMan pruneblockchain()
{
    return RPCHelpMan{"pruneblockchain", "",
                {
                    {"height", RPCArg::Type::NUM, RPCArg::Optional::NO, "The block height to prune up to. May be set to a discrete height, or to a " + UNIX_EPOCH_TIME + "\n"
            "                  to prune blocks whose block time is at least 2 hours older than the provided timestamp."},
                },
                RPCResult{
                    RPCResult::Type::NUM, "", "Height of the last block pruned"},
                RPCExamples{
                    HelpExampleCli("pruneblockchain", "1000")
            + HelpExampleRpc("pruneblockchain", "1000")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    if (!chainman.m_blockman.IsPruneMode()) {
        throw JSONRPCError(RPC_MISC_ERROR, "Cannot prune blocks because node is not in prune mode.");
    }

    LOCK(cs_main);
    Chainstate& active_chainstate = chainman.ActiveChainstate();
    CChain& active_chain = active_chainstate.m_chain;

    int heightParam = request.params[0].getInt<int>();
    if (heightParam < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative block height.");
    }

    // Height value more than a billion is too high to be a block height, and
    // too low to be a block time (corresponds to timestamp from Sep 2001).
    if (heightParam > 1000000000) {
        // Add a 2 hour buffer to include blocks which might have had old timestamps
        const CBlockIndex* pindex = active_chain.FindEarliestAtLeast(heightParam - TIMESTAMP_WINDOW, 0);
        if (!pindex) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Could not find block with at least the specified timestamp.");
        }
        heightParam = pindex->nHeight;
    }

    unsigned int height = (unsigned int) heightParam;
    unsigned int chainHeight = (unsigned int) active_chain.Height();
    if (chainHeight < chainman.GetParams().PruneAfterHeight()) {
        throw JSONRPCError(RPC_MISC_ERROR, "Blockchain is too short for pruning.");
    } else if (height > chainHeight) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Blockchain is shorter than the attempted prune height.");
    } else if (height > chainHeight - MIN_BLOCKS_TO_KEEP) {
        LogPrint(BCLog::RPC, "Attempt to prune blocks close to the tip.  Retaining the minimum number of blocks.\n");
        height = chainHeight - MIN_BLOCKS_TO_KEEP;
    }

    PruneBlockFilesManual(active_chainstate, height);
    const CBlockIndex& block{*CHECK_NONFATAL(active_chain.Tip())};
    const CBlockIndex* last_block{active_chainstate.m_blockman.GetFirstStoredBlock(block)};

    return static_cast<int64_t>(last_block->nHeight - 1);
},
    };
}

CoinStatsHashType ParseHashType(const std::string& hash_type_input)
{
    if (hash_type_input == "hash_serialized_2") {
        return CoinStatsHashType::HASH_SERIALIZED;
    } else if (hash_type_input == "muhash") {
        return CoinStatsHashType::MUHASH;
    } else if (hash_type_input == "none") {
        return CoinStatsHashType::NONE;
    } else {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("'%s' is not a valid hash_type", hash_type_input));
    }
}

/**
 * Calculate statistics about the unspent transaction output set
 *
 * @param[in] index_requested Signals if the coinstatsindex should be used (when available).
 */
static std::optional<kernel::CCoinsStats> GetUTXOStats(CCoinsView* view, node::BlockManager& blockman,
                                                       kernel::CoinStatsHashType hash_type,
                                                       const std::function<void()>& interruption_point = {},
                                                       const CBlockIndex* pindex = nullptr,
                                                       bool index_requested = true)
{
    // Use CoinStatsIndex if it is requested and available and a hash_type of Muhash or None was requested
    if ((hash_type == kernel::CoinStatsHashType::MUHASH || hash_type == kernel::CoinStatsHashType::NONE) && g_coin_stats_index && index_requested) {
        if (pindex) {
            return g_coin_stats_index->LookUpStats(*pindex);
        } else {
            CBlockIndex& block_index = *CHECK_NONFATAL(WITH_LOCK(::cs_main, return blockman.LookupBlockIndex(view->GetBestBlock())));
            return g_coin_stats_index->LookUpStats(block_index);
        }
    }

    // If the coinstats index isn't requested or is otherwise not usable, the
    // pindex should either be null or equal to the view's best block. This is
    // because without the coinstats index we can only get coinstats about the
    // best block.
    CHECK_NONFATAL(!pindex || pindex->GetBlockHash() == view->GetBestBlock());

    return kernel::ComputeUTXOStats(hash_type, view, blockman, interruption_point);
}

static RPCHelpMan gettxoutsetinfo()
{
    return RPCHelpMan{"gettxoutsetinfo",
                "\nReturns statistics about the unspent transaction output set.\n"
                "Note this call may take some time if you are not using coinstatsindex.\n",
                {
                    {"hash_type", RPCArg::Type::STR, RPCArg::Default{"hash_serialized_2"}, "Which UTXO set hash should be calculated. Options: 'hash_serialized_2' (the legacy algorithm), 'muhash', 'none'."},
                    {"hash_or_height", RPCArg::Type::NUM, RPCArg::DefaultHint{"the current best block"}, "The block hash or height of the target height (only available with coinstatsindex).",
                     RPCArgOptions{
                         .skip_type_check = true,
                         .type_str = {"", "string or numeric"},
                     }},
                    {"use_index", RPCArg::Type::BOOL, RPCArg::Default{true}, "Use coinstatsindex, if available."},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM, "height", "The block height (index) of the returned statistics"},
                        {RPCResult::Type::STR_HEX, "bestblock", "The hash of the block at which these statistics are calculated"},
                        {RPCResult::Type::NUM, "txouts", "The number of unspent transaction outputs"},
                        {RPCResult::Type::NUM, "bogosize", "Database-independent, meaningless metric indicating the UTXO set size"},
                        {RPCResult::Type::STR_HEX, "hash_serialized_2", /*optional=*/true, "The serialized hash (only present if 'hash_serialized_2' hash_type is chosen)"},
                        {RPCResult::Type::STR_HEX, "muhash", /*optional=*/true, "The serialized hash (only present if 'muhash' hash_type is chosen)"},
                        {RPCResult::Type::NUM, "transactions", /*optional=*/true, "The number of transactions with unspent outputs (not available when coinstatsindex is used)"},
                        {RPCResult::Type::NUM, "disk_size", /*optional=*/true, "The estimated size of the chainstate on disk (not available when coinstatsindex is used)"},
                        {RPCResult::Type::STR_AMOUNT, "total_amount", "Sum of every UTXO nValue with no ORU/RES split (both 1e8). Not circulating ORU supply. CVG2 is absent from the UTXO set."},
                        {RPCResult::Type::STR_AMOUNT, "total_unspendable_amount", /*optional=*/true, "The total amount of coins permanently excluded from the UTXO set (only available if coinstatsindex is used)"},
                        {RPCResult::Type::OBJ, "block_info", /*optional=*/true, "Info on amounts in the block at this block height (only available if coinstatsindex is used)",
                        {
                            {RPCResult::Type::STR_AMOUNT, "prevout_spent", "Total amount of all prevouts spent in this block"},
                            {RPCResult::Type::STR_AMOUNT, "coinbase", "Coinbase subsidy amount of this block"},
                            {RPCResult::Type::STR_AMOUNT, "new_outputs_ex_coinbase", "Total amount of new outputs created by this block"},
                            {RPCResult::Type::STR_AMOUNT, "unspendable", "Total amount of unspendable outputs created in this block"},
                            {RPCResult::Type::OBJ, "unspendables", "Detailed view of the unspendable categories",
                            {
                                {RPCResult::Type::STR_AMOUNT, "genesis_block", "The unspendable amount of the Genesis block subsidy"},
                                {RPCResult::Type::STR_AMOUNT, "bip30", "Transactions overridden by duplicates (no longer possible with BIP30)"},
                                {RPCResult::Type::STR_AMOUNT, "scripts", "Amounts sent to scripts that are unspendable (for example RIGOR_MARKER_RETURN outputs)"},
                                {RPCResult::Type::STR_AMOUNT, "unclaimed_rewards", "Fee rewards that miners did not claim in their coinbase transaction"},
                            }}
                        }},
                    }},
                RPCExamples{
                    HelpExampleCli("gettxoutsetinfo", "") +
                    HelpExampleCli("gettxoutsetinfo", R"("none")") +
                    HelpExampleCli("gettxoutsetinfo", R"("none" 1000)") +
                    HelpExampleCli("gettxoutsetinfo", R"("none" '"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09"')") +
                    HelpExampleCli("-named gettxoutsetinfo", R"(hash_type='muhash' use_index='false')") +
                    HelpExampleRpc("gettxoutsetinfo", "") +
                    HelpExampleRpc("gettxoutsetinfo", R"("none")") +
                    HelpExampleRpc("gettxoutsetinfo", R"("none", 1000)") +
                    HelpExampleRpc("gettxoutsetinfo", R"("none", "00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09")")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    UniValue ret(UniValue::VOBJ);

    const CBlockIndex* pindex{nullptr};
    const CoinStatsHashType hash_type{request.params[0].isNull() ? CoinStatsHashType::HASH_SERIALIZED : ParseHashType(request.params[0].get_str())};
    bool index_requested = request.params[2].isNull() || request.params[2].get_bool();

    NodeContext& node = EnsureAnyNodeContext(request.context);
    ChainstateManager& chainman = EnsureChainman(node);
    Chainstate& active_chainstate = chainman.ActiveChainstate();
    active_chainstate.ForceFlushStateToDisk();

    CCoinsView* coins_view;
    BlockManager* blockman;
    {
        LOCK(::cs_main);
        coins_view = &active_chainstate.CoinsDB();
        blockman = &active_chainstate.m_blockman;
        pindex = blockman->LookupBlockIndex(coins_view->GetBestBlock());
    }

    if (!request.params[1].isNull()) {
        if (!g_coin_stats_index) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Querying specific block heights requires coinstatsindex");
        }

        if (hash_type == CoinStatsHashType::HASH_SERIALIZED) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "hash_serialized_2 hash type cannot be queried for a specific block");
        }

        if (!index_requested) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot set use_index to false when querying for a specific block");
        }
        pindex = ParseHashOrHeight(request.params[1], chainman);
    }

    if (index_requested && g_coin_stats_index) {
        if (!g_coin_stats_index->BlockUntilSyncedToCurrentChain()) {
            const IndexSummary summary{g_coin_stats_index->GetSummary()};

            // If a specific block was requested and the index has already synced past that height, we can return the
            // data already even though the index is not fully synced yet.
            if (pindex->nHeight > summary.best_block_height) {
                throw JSONRPCError(RPC_INTERNAL_ERROR, strprintf("Unable to get data because coinstatsindex is still syncing. Current height: %d", summary.best_block_height));
            }
        }
    }

    const std::optional<CCoinsStats> maybe_stats = GetUTXOStats(coins_view, *blockman, hash_type, node.rpc_interruption_point, pindex, index_requested);
    if (maybe_stats.has_value()) {
        const CCoinsStats& stats = maybe_stats.value();
        ret.pushKV("height", (int64_t)stats.nHeight);
        ret.pushKV("bestblock", stats.hashBlock.GetHex());
        ret.pushKV("txouts", (int64_t)stats.nTransactionOutputs);
        ret.pushKV("bogosize", (int64_t)stats.nBogoSize);
        if (hash_type == CoinStatsHashType::HASH_SERIALIZED) {
            ret.pushKV("hash_serialized_2", stats.hashSerialized.GetHex());
        }
        if (hash_type == CoinStatsHashType::MUHASH) {
            ret.pushKV("muhash", stats.hashSerialized.GetHex());
        }
        CHECK_NONFATAL(stats.total_amount.has_value());
        ret.pushKV("total_amount", ValueFromAmount(stats.total_amount.value()));
        if (!stats.index_used) {
            ret.pushKV("transactions", static_cast<int64_t>(stats.nTransactions));
            ret.pushKV("disk_size", stats.nDiskSize);
        } else {
            ret.pushKV("total_unspendable_amount", ValueFromAmount(stats.total_unspendable_amount));

            CCoinsStats prev_stats{};
            if (pindex->nHeight > 0) {
                const std::optional<CCoinsStats> maybe_prev_stats = GetUTXOStats(coins_view, *blockman, hash_type, node.rpc_interruption_point, pindex->pprev, index_requested);
                if (!maybe_prev_stats) {
                    throw JSONRPCError(RPC_INTERNAL_ERROR, "Unable to read UTXO set");
                }
                prev_stats = maybe_prev_stats.value();
            }

            UniValue block_info(UniValue::VOBJ);
            block_info.pushKV("prevout_spent", ValueFromAmount(stats.total_prevout_spent_amount - prev_stats.total_prevout_spent_amount));
            block_info.pushKV("coinbase", ValueFromAmount(stats.total_coinbase_amount - prev_stats.total_coinbase_amount));
            block_info.pushKV("new_outputs_ex_coinbase", ValueFromAmount(stats.total_new_outputs_ex_coinbase_amount - prev_stats.total_new_outputs_ex_coinbase_amount));
            block_info.pushKV("unspendable", ValueFromAmount(stats.total_unspendable_amount - prev_stats.total_unspendable_amount));

            UniValue unspendables(UniValue::VOBJ);
            unspendables.pushKV("genesis_block", ValueFromAmount(stats.total_unspendables_genesis_block - prev_stats.total_unspendables_genesis_block));
            unspendables.pushKV("bip30", ValueFromAmount(stats.total_unspendables_bip30 - prev_stats.total_unspendables_bip30));
            unspendables.pushKV("scripts", ValueFromAmount(stats.total_unspendables_scripts - prev_stats.total_unspendables_scripts));
            unspendables.pushKV("unclaimed_rewards", ValueFromAmount(stats.total_unspendables_unclaimed_rewards - prev_stats.total_unspendables_unclaimed_rewards));
            block_info.pushKV("unspendables", unspendables);

            ret.pushKV("block_info", block_info);
        }
    } else {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Unable to read UTXO set");
    }
    return ret;
},
    };
}

static RPCHelpMan gettxout()
{
    return RPCHelpMan{"gettxout",
        "\nReturns details about an unspent transaction output.\n",
        {
            {"txid", RPCArg::Type::STR, RPCArg::Optional::NO, "The transaction id"},
            {"n", RPCArg::Type::NUM, RPCArg::Optional::NO, "vout number"},
            {"include_mempool", RPCArg::Type::BOOL, RPCArg::Default{true}, "Whether to include the mempool. Note that an unspent output that is spent in the mempool won't appear."},
        },
        {
            RPCResult{"If the UTXO was not found", RPCResult::Type::NONE, "", ""},
            RPCResult{"Otherwise", RPCResult::Type::OBJ, "", "", {
                {RPCResult::Type::STR_HEX, "bestblock", "The hash of the block at the tip of the chain"},
                {RPCResult::Type::NUM, "confirmations", "The number of confirmations"},
                {RPCResult::Type::STR_AMOUNT, "value", "The transaction value in " + CURRENCY_UNIT},
                {RPCResult::Type::OBJ, "scriptPubKey", "", {
                    {RPCResult::Type::STR, "asm", "Disassembly of the public key script"},
                    {RPCResult::Type::STR, "desc", "Inferred descriptor for the output"},
                    {RPCResult::Type::STR_HEX, "hex", "The raw public key script bytes, hex-encoded"},
                    {RPCResult::Type::STR, "type", "The type, eg pubkeyhash"},
                    {RPCResult::Type::STR, "address", /*optional=*/true, "The Rigor address (only if a well-defined address exists)"},
                }},
                {RPCResult::Type::BOOL, "coinbase", "Coinbase or not"},
                {RPCResult::Type::BOOL, "res_mint", "True if this UTXO is a body RES mint output (immature until nCoinbaseMaturity)"},
            }},
        },
        RPCExamples{
            "\nGet unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nView the details\n"
            + HelpExampleCli("gettxout", "\"txid\" 1") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("gettxout", "\"txid\", 1")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    NodeContext& node = EnsureAnyNodeContext(request.context);
    ChainstateManager& chainman = EnsureChainman(node);
    LOCK(cs_main);

    UniValue ret(UniValue::VOBJ);

    uint256 hash(ParseHashV(request.params[0], "txid"));
    COutPoint out{hash, request.params[1].getInt<uint32_t>()};
    bool fMempool = true;
    if (!request.params[2].isNull())
        fMempool = request.params[2].get_bool();

    Coin coin;
    Chainstate& active_chainstate = chainman.ActiveChainstate();
    CCoinsViewCache* coins_view = &active_chainstate.CoinsTip();

    if (fMempool) {
        const CTxMemPool& mempool = EnsureMemPool(node);
        LOCK(mempool.cs);
        CCoinsViewMemPool view(coins_view, mempool);
        if (!view.GetCoin(out, coin) || mempool.isSpent(out)) {
            return UniValue::VNULL;
        }
    } else {
        if (!coins_view->GetCoin(out, coin)) {
            return UniValue::VNULL;
        }
    }

    const CBlockIndex* pindex = active_chainstate.m_blockman.LookupBlockIndex(coins_view->GetBestBlock());
    ret.pushKV("bestblock", pindex->GetBlockHash().GetHex());
    if (coin.nHeight == MEMPOOL_HEIGHT) {
        ret.pushKV("confirmations", 0);
    } else {
        ret.pushKV("confirmations", (int64_t)(pindex->nHeight - coin.nHeight + 1));
    }
    ret.pushKV("value", ValueFromAmount(coin.out.nValue));
    UniValue o(UniValue::VOBJ);
    ScriptToUniv(coin.out.scriptPubKey, /*out=*/o, /*include_hex=*/true, /*include_address=*/true);
    ret.pushKV("scriptPubKey", o);
    ret.pushKV("coinbase", (bool)coin.fCoinBase);
    ret.pushKV("res_mint", IsResMintScript(coin.out.scriptPubKey));

    if (ConvSignTraceActive()) {
        int wver = 0;
        std::vector<unsigned char> wprog;
        if (coin.out.scriptPubKey.IsWitnessProgram(wver, wprog) && wver == 0 && wprog.size() == 20) {
            LogPrint(BCLog::CONVERGESIGN,
                     "convtrace gettxout CHAIN_VIEW %s:%u nValue=%s coinbase=%i height=%u scriptPubKey=%s\n",
                     hash.ToString().c_str(),
                     static_cast<unsigned>(out.n),
                     FormatMoney(coin.out.nValue),
                     coin.fCoinBase ? 1 : 0,
                     static_cast<unsigned>(coin.nHeight),
                     HexStr(coin.out.scriptPubKey));
        }
    }

    return ret;
},
    };
}

static RPCHelpMan verifychain()
{
    return RPCHelpMan{"verifychain",
                "\nVerifies blockchain database.\n",
                {
                    {"checklevel", RPCArg::Type::NUM, RPCArg::DefaultHint{strprintf("%d, range=0-4", DEFAULT_CHECKLEVEL)},
                        strprintf("How thorough the block verification is:\n%s", MakeUnorderedList(CHECKLEVEL_DOC))},
                    {"nblocks", RPCArg::Type::NUM, RPCArg::DefaultHint{strprintf("%d, 0=all", DEFAULT_CHECKBLOCKS)}, "The number of blocks to check."},
                },
                RPCResult{
                    RPCResult::Type::BOOL, "", "Verification finished successfully. If false, check debug.log for reason."},
                RPCExamples{
                    HelpExampleCli("verifychain", "")
            + HelpExampleRpc("verifychain", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    const int check_level{request.params[0].isNull() ? DEFAULT_CHECKLEVEL : request.params[0].getInt<int>()};
    const int check_depth{request.params[1].isNull() ? DEFAULT_CHECKBLOCKS : request.params[1].getInt<int>()};

    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    LOCK(cs_main);

    Chainstate& active_chainstate = chainman.ActiveChainstate();
    return CVerifyDB().VerifyDB(
               active_chainstate, chainman.GetParams().GetConsensus(), active_chainstate.CoinsTip(), check_level, check_depth) == VerifyDBResult::SUCCESS;
},
    };
}

static void SoftForkDescPushBack(const CBlockIndex* blockindex, UniValue& softforks, const ChainstateManager& chainman, Consensus::BuriedDeployment dep)
{
    // For buried deployments.

    if (!DeploymentEnabled(chainman, dep)) return;

    UniValue rv(UniValue::VOBJ);
    rv.pushKV("type", "buried");
    // getdeploymentinfo reports the softfork as active from when the chain height is
    // one below the activation height
    rv.pushKV("active", DeploymentActiveAfter(blockindex, chainman, dep));
    rv.pushKV("height", chainman.GetConsensus().DeploymentHeight(dep));
    softforks.pushKV(DeploymentName(dep), rv);
}

static void SoftForkDescPushBack(const CBlockIndex* blockindex, UniValue& softforks, const ChainstateManager& chainman, Consensus::DeploymentPos id)
{
    // For BIP9 deployments.

    if (!DeploymentEnabled(chainman, id)) return;
    if (blockindex == nullptr) return;

    auto get_state_name = [](const ThresholdState state) -> std::string {
        switch (state) {
        case ThresholdState::DEFINED: return "defined";
        case ThresholdState::STARTED: return "started";
        case ThresholdState::LOCKED_IN: return "locked_in";
        case ThresholdState::ACTIVE: return "active";
        case ThresholdState::FAILED: return "failed";
        }
        return "invalid";
    };

    UniValue bip9(UniValue::VOBJ);

    const ThresholdState next_state = chainman.m_versionbitscache.State(blockindex, chainman.GetConsensus(), id);
    const ThresholdState current_state = chainman.m_versionbitscache.State(blockindex->pprev, chainman.GetConsensus(), id);

    const bool has_signal = (ThresholdState::STARTED == current_state || ThresholdState::LOCKED_IN == current_state);

    // BIP9 parameters
    if (has_signal) {
        bip9.pushKV("bit", chainman.GetConsensus().vDeployments[id].bit);
    }
    bip9.pushKV("start_time", chainman.GetConsensus().vDeployments[id].nStartTime);
    bip9.pushKV("timeout", chainman.GetConsensus().vDeployments[id].nTimeout);
    bip9.pushKV("min_activation_height", chainman.GetConsensus().vDeployments[id].min_activation_height);

    // BIP9 status
    bip9.pushKV("status", get_state_name(current_state));
    bip9.pushKV("since", chainman.m_versionbitscache.StateSinceHeight(blockindex->pprev, chainman.GetConsensus(), id));
    bip9.pushKV("status_next", get_state_name(next_state));

    // BIP9 signalling status, if applicable
    if (has_signal) {
        UniValue statsUV(UniValue::VOBJ);
        std::vector<bool> signals;
        BIP9Stats statsStruct = chainman.m_versionbitscache.Statistics(blockindex, chainman.GetConsensus(), id, &signals);
        statsUV.pushKV("period", statsStruct.period);
        statsUV.pushKV("elapsed", statsStruct.elapsed);
        statsUV.pushKV("count", statsStruct.count);
        if (ThresholdState::LOCKED_IN != current_state) {
            statsUV.pushKV("threshold", statsStruct.threshold);
            statsUV.pushKV("possible", statsStruct.possible);
        }
        bip9.pushKV("statistics", statsUV);

        std::string sig;
        sig.reserve(signals.size());
        for (const bool s : signals) {
            sig.push_back(s ? '#' : '-');
        }
        bip9.pushKV("signalling", sig);
    }

    UniValue rv(UniValue::VOBJ);
    rv.pushKV("type", "bip9");
    if (ThresholdState::ACTIVE == next_state) {
        rv.pushKV("height", chainman.m_versionbitscache.StateSinceHeight(blockindex, chainman.GetConsensus(), id));
    }
    rv.pushKV("active", ThresholdState::ACTIVE == next_state);
    rv.pushKV("bip9", bip9);

    softforks.pushKV(DeploymentName(id), rv);
}

// Diagnostic-only RPC. Returns a Consensus::ReplaySnapshot (RPL2 wire format,
// see consensus/replay_snapshot.h). Strictly read-only; never mutates chainstate.
// All derived counter fields are projections of the UTXO set and are NOT folded
// into state_hash (see ReplaySnapshot doc-comment for the RIGOR RULE-3 rationale).
static RPCHelpMan getreplaysnapshot()
{
    return RPCHelpMan{"getreplaysnapshot",
                "\nCaptures a diagnostic replay snapshot for divergence triage.\n"
                "Returns the deterministic state_hash plus derived UTXO-set projections.\n"
                "STRICTLY DIAGNOSTIC: the authoritative replay artifact is `state_hash`;\n"
                "all counter fields are non-consensus projections (see RFC notes).\n",
                {},
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM, "height", "Current block height"},
                        {RPCResult::Type::STR_HEX, "block_hash", "Hash of the current tip block"},
                        {RPCResult::Type::STR_HEX, "state_hash", "Deterministic state hash (authoritative)"},
                        {RPCResult::Type::STR_HEX, "best_block_hash", "Hash of the best block (equals block_hash)"},
                        {RPCResult::Type::NUM, "utxo_count", "Number of UTXOs in the chainstate"},
                        {RPCResult::Type::NUM, "mint_count", "UTXOs matching IsResScript() (RES outputs)"},
                        {RPCResult::Type::NUM, "lock_count", "UTXOs with witness-V1 lock-program shape (informational)"},
                        {RPCResult::Type::NUM, "converge_count", "UTXOs matching IsConvergenceScript() (invariant: must be 0)"},
                        {RPCResult::Type::STR_AMOUNT, "res_balance", "Sum of nValue over RES UTXOs"},
                        {RPCResult::Type::STR_AMOUNT, "other_balance", "Sum of nValue over non-RES UTXOs"},
                        {RPCResult::Type::STR_HEX, "serialized_snapshot", "Canonical RPL2 serialized snapshot (hex)"},
                    }
                },
                RPCExamples{
                    HelpExampleCli("getreplaysnapshot", "") + HelpExampleRpc("getreplaysnapshot", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            const NodeContext& node = EnsureAnyNodeContext(request.context);
            const ChainstateManager& chainman = EnsureChainman(node);
            LOCK(cs_main);
            const Chainstate& chainstate = chainman.ActiveChainstate();

            uint256 state_hash = const_cast<Chainstate&>(chainstate).ComputeDeterministicStateHash();
            Consensus::ReplaySnapshot snapshot = Consensus::CaptureReplaySnapshot(
                const_cast<Chainstate&>(chainstate).CoinsTip(),
                chainstate.m_chain.Tip(),
                state_hash);

            UniValue result(UniValue::VOBJ);
            result.pushKV("height", snapshot.height);
            result.pushKV("block_hash", snapshot.block_hash.GetHex());
            result.pushKV("state_hash", snapshot.state_hash.GetHex());
            result.pushKV("best_block_hash", snapshot.best_block_hash.GetHex());
            result.pushKV("utxo_count", static_cast<uint64_t>(snapshot.utxo_count));
            result.pushKV("mint_count", static_cast<uint64_t>(snapshot.mint_count));
            result.pushKV("lock_count", static_cast<uint64_t>(snapshot.lock_count));
            result.pushKV("converge_count", static_cast<uint64_t>(snapshot.converge_count));
            result.pushKV("res_balance", ValueFromAmountAsset(static_cast<CAmount>(snapshot.res_balance), ASSET_RES));
            result.pushKV("other_balance", ValueFromAmount(static_cast<CAmount>(snapshot.other_balance)));

            std::string serialized = snapshot.Serialize();
            result.pushKV("serialized_snapshot", HexStr(serialized));

            return result;
        },
    };
}

namespace {
UniValue GetRuntimeSupplyRpcResult(Chainstate& chainstate, ChainstateManager& chainman, const CBlockIndex* pindex)
{
    if (!pindex) {
        throw JSONRPCError(RPC_MISC_ERROR, "No chain tip");
    }
    const Consensus::RuntimeSupplyExport exp = Consensus::BuildRuntimeSupplyExport(
        pindex, chainstate.CoinsTip(), chainman.GetParams().GetConsensus());
    return Consensus::RuntimeSupplyExportToJson(exp, *pindex, chainman.GetParams().GetConsensus());
}

UniValue RuntimeInvariantResultToJson(const Consensus::RuntimeInvariantResult& result)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("passed", result.passed);
    obj.pushKV("checked_invariants", static_cast<int>(result.checked_invariants));
    UniValue violations(UniValue::VARR);
    for (const std::string& id : result.violations) {
        violations.push_back(id);
    }
    obj.pushKV("violations", std::move(violations));
    return obj;
}

UniValue RuntimeSupplyWitnessToJson(const Consensus::RuntimeSupplyWitness& witness)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("height", witness.height);
    obj.pushKV("block_hash", witness.block_hash.GetHex());
    obj.pushKV("parent_hash", witness.parent_hash.IsNull() ? "" : witness.parent_hash.GetHex());
    obj.pushKV("invariants_passed", witness.invariants_passed);
    obj.pushKV("checked_invariants", static_cast<int>(witness.checked_invariants));
    UniValue violations(UniValue::VARR);
    for (const std::string& id : witness.violations) {
        violations.push_back(id);
    }
    obj.pushKV("violations", std::move(violations));
    obj.pushKV("oru_supply_cumulative", ValueFromAmount(witness.oru_supply_cumulative));
    obj.pushKV("res_mint_cumulative", ValueFromAmountAsset(witness.res_mint_cumulative, ASSET_RES));
    return obj;
}
} // namespace

static RPCHelpMan getruntimesupply()
{
    return RPCHelpMan{
        "getruntimesupply",
        "\nReturns the observational ORU/RES supply ledger mirror (read-only).\n"
        "Includes Coq catalog fields, authoritative pindex caches, and 30 Phase-5 supply audit cells.\n"
        "Does not affect consensus, replay, or state_hash.\n"
        "\nSupply semantics (do not collapse these fields):\n"
        "  oru_subsidy_cumulative_derived = total issued ORU (Σ R).\n"
        "  oru_supply_cumulative          = RFC-002 ledger S_ORU = issued − converge; NOT circulating.\n"
        "  oru_converge_burn_block        = this block's Convergence destruction C(h); NOT RFC-018 fee burn.\n"
        "  utxo_non_res_balance           = circulating spendable ORU UTXO proxy (includes immature; excludes CVG2).\n"
        "  RFC-018 fee-burn cumulative is not persisted on the block index.\n",
        {
            {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED,
             "Block hash or height (default: active tip)"},
        },
        RPCResult{
            RPCResult::Type::OBJ,
            "",
            "",
            {
                {RPCResult::Type::NUM, "height", "Block height"},
                {RPCResult::Type::BOOL, "mirror_present", "True if post-ConnectBlock mirror stored on pindex"},
                {RPCResult::Type::STR, "mirror_source", "stored_mirror or captured_fresh"},
                {RPCResult::Type::STR_HEX, "block_hash", "Block hash"},
                {RPCResult::Type::STR_HEX, "parent_hash", "Parent block hash"},
                {RPCResult::Type::STR_AMOUNT, "oru_supply_cumulative", "RFC-002 ledger S_ORU(h) = issued − converge. NOT circulating ORU after RFC-018 fee burn"},
                {RPCResult::Type::STR_AMOUNT, "oru_subsidy_block", "R(h)"},
                {RPCResult::Type::STR_AMOUNT, "oru_converge_burn_block", "C(h) Convergence destroyed this block (not RFC-018 fee burn)"},
                {RPCResult::Type::STR_AMOUNT, "oru_supply_parent", "RFC-002 ledger S_ORU(h-1)"},
                {RPCResult::Type::STR_AMOUNT, "res_mint_cumulative", "S_RES(h) cumulative mint"},
                {RPCResult::Type::STR_AMOUNT, "res_mint_block", "RES mint in this block"},
                {RPCResult::Type::STR_AMOUNT, "res_mint_parent", "S_RES(h-1)"},
                {RPCResult::Type::NUM, "res_mint_present_block", "M(h) in {0,1}: 1 iff this block minted RES. An indicator, not a count -- RFC-020 puts no cap on mints per block; the amount is res_mint_block"},
                {RPCResult::Type::STR_AMOUNT, "res_max_supply", "RES_MAX_SUPPLY constant"},
                {RPCResult::Type::STR_AMOUNT, "utxo_res_balance", "UTXO RES aggregate"},
                {RPCResult::Type::STR_AMOUNT, "utxo_non_res_balance", "Circulating spendable ORU UTXO proxy (non-RES scripts; includes immature coinbase; CVG2 not in UTXO set)"},
                {RPCResult::Type::STR_AMOUNT, "oru_subsidy_cumulative_derived", "Total issued ORU: optional sum of R(i), i<=h"},
                {RPCResult::Type::ARR, "invariant_violations", "Empty if mirror satisfies runtime invariants",
                 {
                     {RPCResult::Type::STR, "violation", ""},
                 }},
                {RPCResult::Type::OBJ, "authoritative_caches", "CBlockIndex ledger caches"},
                {RPCResult::Type::OBJ, "coq_fields", "Phase-5 Coq catalog names"},
                {RPCResult::Type::ARR, "coq_hook_violations", "Empty when hook matches mirror and pindex",
                 {{RPCResult::Type::STR, "violation", ""}}},
                {RPCResult::Type::NUM, "phase5_supply_cell_count", "30 (15 observables x 2 sources)"},
                {RPCResult::Type::ARR, "phase5_supply_cells", "Phase-5 audit grid",
                 {{RPCResult::Type::OBJ, "cell", "Supply audit cell"}}},
                {RPCResult::Type::OBJ, "supply_semantics", "Canonical meaning of supply fields; equivalence_claimed remains false",
                 {
                     {RPCResult::Type::BOOL, "equivalence_claimed", "Always false"},
                     {RPCResult::Type::STR, "oru_supply_cumulative", "rfc002_ledger_issued_minus_converge_not_circulating"},
                     {RPCResult::Type::STR, "oru_subsidy_cumulative_derived", "total_issued_oru"},
                     {RPCResult::Type::STR, "oru_converge_burn_block", "this_block_convergence_destroyed_oru_not_rfc018_fee_burn"},
                     {RPCResult::Type::STR, "utxo_non_res_balance", "circulating_spendable_oru_utxo_includes_immature_excludes_cvg2"},
                     {RPCResult::Type::STR, "rfc018_fee_burn_cumulative", "not_persisted_on_block_index"},
                 }},
            }},
        RPCExamples{
            HelpExampleCli("getruntimesupply", "") +
            HelpExampleCli("getruntimesupply", "\"blockhash\"") +
            HelpExampleRpc("getruntimesupply", "")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            LOCK(cs_main);
            Chainstate& chainstate = chainman.ActiveChainstate();
            const CBlockIndex* pindex = chainstate.m_chain.Tip();
            if (!request.params[0].isNull()) {
                pindex = ParseHashOrHeight(request.params[0], chainman);
            }
            return GetRuntimeSupplyRpcResult(chainstate, chainman, pindex);
        },
    };
}

static RPCHelpMan getruntimesupplyinvariants()
{
    return RPCHelpMan{
        "getruntimesupplyinvariants",
        "\nVerify RuntimeSupplyState against formal ORU/RES invariants (read-only).\n"
        "Does not affect consensus, validation, or state_hash.\n",
        {
            {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED,
             "Block hash or height (default: active tip)"},
        },
        RPCResult{
            RPCResult::Type::OBJ,
            "",
            "",
            {
                {RPCResult::Type::BOOL, "passed", "True if all formal invariants hold"},
                {RPCResult::Type::NUM, "checked_invariants", "Always 9 (ORU-L1..L4 + RES-L1, L2, L4, L5, L6)"},
                {RPCResult::Type::ARR, "violations", "Stable ids e.g. ORU-L3, RES-L5",
                 {{RPCResult::Type::STR, "id", ""}}},
            }},
        RPCExamples{
            HelpExampleCli("getruntimesupplyinvariants", "") +
            HelpExampleRpc("getruntimesupplyinvariants", "")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            LOCK(cs_main);
            Chainstate& chainstate = chainman.ActiveChainstate();
            const CBlockIndex* pindex = chainstate.m_chain.Tip();
            if (!request.params[0].isNull()) {
                pindex = ParseHashOrHeight(request.params[0], chainman);
            }
            if (!pindex) {
                throw JSONRPCError(RPC_MISC_ERROR, "No chain tip");
            }
            const Consensus::RuntimeSupplyExport exp = Consensus::BuildRuntimeSupplyExport(
                pindex, chainstate.CoinsTip(), chainman.GetParams().GetConsensus());
            const Consensus::RuntimeInvariantResult result = Consensus::VerifyRuntimeSupplyInvariants(
                exp.mirror, chainman.GetParams().GetConsensus());
            return RuntimeInvariantResultToJson(result);
        },
    };
}

static RPCHelpMan getruntimesupplywitness()
{
    return RPCHelpMan{
        "getruntimesupplywitness",
        "\nReturn the runtime supply witness for the active tip (read-only).\n"
        "Historical formal verification record from ConnectBlock; does not affect consensus.\n"
        "FENCE-W7: resolved via the tip's block hash, never a height-keyed global map.\n",
        {},
        RPCResult{
            RPCResult::Type::OBJ,
            "",
            "",
            {
                {RPCResult::Type::NUM, "height", "Block height"},
                {RPCResult::Type::STR_HEX, "block_hash", "Block hash"},
                {RPCResult::Type::STR_HEX, "parent_hash", "Parent block hash"},
                {RPCResult::Type::BOOL, "invariants_passed", "Formal ORU/RES invariants at connect"},
                {RPCResult::Type::NUM, "checked_invariants", "Always 9"},
                {RPCResult::Type::ARR, "violations", "Stable ids e.g. ORU-L3",
                 {{RPCResult::Type::STR, "id", ""}}},
                {RPCResult::Type::STR_AMOUNT, "oru_supply_cumulative", "RFC-002 ledger S_ORU(h); not circulating"},
                {RPCResult::Type::STR_AMOUNT, "res_mint_cumulative", "S_RES(h)"},
            }},
        RPCExamples{
            HelpExampleCli("getruntimesupplywitness", "") +
            HelpExampleRpc("getruntimesupplywitness", "")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            (void)self;
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            LOCK(cs_main);
            const CBlockIndex* pindex = chainman.ActiveChainstate().m_chain.Tip();
            if (!pindex) {
                throw JSONRPCError(RPC_MISC_ERROR, "No chain tip");
            }
            const std::optional<Consensus::RuntimeSupplyWitness> witness =
                Consensus::GetRuntimeSupplyWitness(pindex);
            if (!witness.has_value()) {
                throw JSONRPCError(RPC_MISC_ERROR, "No runtime supply witness stored");
            }
            return RuntimeSupplyWitnessToJson(*witness);
        },
    };
}

static RPCHelpMan getruntimesupplywitnessbyheight()
{
    return RPCHelpMan{
        "getruntimesupplywitnessbyheight",
        "\nReturn runtime supply witness for a specific height (read-only).\n"
        "FENCE-W7: height is resolved on the active chain, then the witness is loaded by block hash.\n"
        "There is no height-keyed consensus cache.\n",
        {
            {"height", RPCArg::Type::NUM, RPCArg::Optional::NO, "Block height on the active chain"},
        },
        RPCResult{
            RPCResult::Type::OBJ,
            "",
            "",
            {
                {RPCResult::Type::NUM, "height", "Block height"},
                {RPCResult::Type::STR_HEX, "block_hash", "Block hash"},
                {RPCResult::Type::STR_HEX, "parent_hash", "Parent block hash"},
                {RPCResult::Type::BOOL, "invariants_passed", "Formal ORU/RES invariants at connect"},
                {RPCResult::Type::NUM, "checked_invariants", "Always 9"},
                {RPCResult::Type::ARR, "violations", "Stable ids e.g. RES-L5",
                 {{RPCResult::Type::STR, "id", ""}}},
                {RPCResult::Type::STR_AMOUNT, "oru_supply_cumulative", "RFC-002 ledger S_ORU(h); not circulating"},
                {RPCResult::Type::STR_AMOUNT, "res_mint_cumulative", "S_RES(h)"},
            }},
        RPCExamples{
            HelpExampleCli("getruntimesupplywitnessbyheight", "1000") +
            HelpExampleRpc("getruntimesupplywitnessbyheight", "1000")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            (void)self;
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            LOCK(cs_main);
            const CBlockIndex* pindex = ParseHashOrHeight(request.params[0], chainman);
            if (!pindex) {
                throw JSONRPCError(RPC_MISC_ERROR, "No block at height");
            }
            const std::optional<Consensus::RuntimeSupplyWitness> witness =
                Consensus::GetRuntimeSupplyWitness(pindex);
            if (!witness.has_value()) {
                throw JSONRPCError(RPC_MISC_ERROR, "No runtime supply witness at height");
            }
            return RuntimeSupplyWitnessToJson(*witness);
        },
    };
}

static RPCHelpMan getruntimesupplywitnesscatalog()
{
    return RPCHelpMan{
        "getruntimesupplywitnesscatalog",
        "\nPhase-5 Coq catalog row from historical runtime witness + stored mirror.\n"
        "Uses in-memory witness chain only; does not affect consensus or state_hash.\n",
        {
            {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED,
             "Block hash or height (default: active tip)"},
        },
        RPCResult{
            RPCResult::Type::OBJ,
            "",
            "",
            {
                {RPCResult::Type::NUM, "height", "Block height"},
                {RPCResult::Type::STR_HEX, "block_hash", "Block hash"},
                {RPCResult::Type::STR_HEX, "parent_hash", "Parent block hash"},
                {RPCResult::Type::BOOL, "invariants_passed", "FVR-E formal invariants at connect"},
                {RPCResult::Type::NUM, "checked_invariants", "Always 9"},
                {RPCResult::Type::ARR, "violations", "ORU-L* / RES-L* ids",
                 {{RPCResult::Type::STR, "id", ""}}},
                {RPCResult::Type::BOOL, "mirror_present", "Stored post-ConnectBlock mirror on pindex"},
                {RPCResult::Type::STR, "mirror_source", "stored_mirror or witness_only"},
                {RPCResult::Type::BOOL, "equivalence_claimed", "Always false in this phase"},
                {RPCResult::Type::STR, "coq_equiv_runtime", "NOT_ESTABLISHED"},
                {RPCResult::Type::OBJ, "coq_catalog", "Phase-5 rs_oru_supply / rs_res_supply vectors"},
                {RPCResult::Type::OBJ, "parent_delta", "Witness parent-child cumulative deltas"},
            }},
        RPCExamples{
            HelpExampleCli("getruntimesupplywitnesscatalog", "") +
            HelpExampleCli("getruntimesupplywitnesscatalog", "1000") +
            HelpExampleRpc("getruntimesupplywitnesscatalog", "")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            (void)self;
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            LOCK(cs_main);
            Chainstate& chainstate = chainman.ActiveChainstate();
            const CBlockIndex* pindex = chainstate.m_chain.Tip();
            if (!request.params[0].isNull()) {
                pindex = ParseHashOrHeight(request.params[0], chainman);
            }
            if (!pindex) {
                throw JSONRPCError(RPC_MISC_ERROR, "No chain tip");
            }
            const std::optional<Consensus::RuntimeSupplyWitnessCoqCatalogRow> row =
                Consensus::ResolveWitnessCoqCatalogRowAt(pindex);
            if (!row.has_value()) {
                throw JSONRPCError(RPC_MISC_ERROR, "No runtime supply witness catalog row at block");
            }
            return Consensus::WitnessCoqCatalogRowToJson(*row);
        },
    };
}

static RPCHelpMan getphase5runtimealignment()
{
    return RPCHelpMan{
        "getphase5runtimealignment",
        "\nPhase-5 CHAS alignment vectors from witness Coq catalog row.\n"
        "Read-only; uses in-memory witnesses + mirrors; does not assert Coq ≡ Runtime.\n",
        {
            {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED,
             "Block hash or height (default: active tip)"},
        },
        RPCResult{
            RPCResult::Type::OBJ,
            "",
            "",
            {
                {RPCResult::Type::NUM, "height", "Block height"},
                {RPCResult::Type::STR_HEX, "block_hash", "Block hash"},
                {RPCResult::Type::BOOL, "mirror_present", "Stored mirror on pindex"},
                {RPCResult::Type::STR, "mirror_source", "stored_mirror or witness_only"},
                {RPCResult::Type::BOOL, "invariants_passed", "FVR-E formal invariants"},
                {RPCResult::Type::BOOL, "equivalence_claimed", "Always false"},
                {RPCResult::Type::STR, "coq_equiv_runtime", "NOT_ESTABLISHED"},
                {RPCResult::Type::OBJ, "parent_delta", "Witness cumulative deltas + audit"},
                {RPCResult::Type::ARR, "chas_field_alignments", "chas_oru_state / chas_res_state vectors",
                 {{RPCResult::Type::OBJ, "field", "Per-field CHAS alignment"}}},
                {RPCResult::Type::OBJ, "chas_summary", "Per-field CHAS status + proof-debt flags"},
            }},
        RPCExamples{
            HelpExampleCli("getphase5runtimealignment", "") +
            HelpExampleCli("getphase5runtimealignment", "100") +
            HelpExampleRpc("getphase5runtimealignment", "")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            (void)self;
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            LOCK(cs_main);
            Chainstate& chainstate = chainman.ActiveChainstate();
            const CBlockIndex* pindex = chainstate.m_chain.Tip();
            if (!request.params[0].isNull()) {
                pindex = ParseHashOrHeight(request.params[0], chainman);
            }
            if (!pindex) {
                throw JSONRPCError(RPC_MISC_ERROR, "No chain tip");
            }
            const std::optional<Consensus::RuntimeSupplyChasAlignmentReport> report =
                Consensus::ResolveChasAlignmentAt(pindex);
            if (!report.has_value()) {
                throw JSONRPCError(RPC_MISC_ERROR, "No Phase-5 runtime alignment report at block");
            }
            return Consensus::ChasAlignmentReportToJson(*report);
        },
    };
}

static RPCHelpMan getruntimeequivalencecandidate()
{
    return RPCHelpMan{
        "getruntimeequivalencecandidate",
        "\nLatest runtime↔Coq witness equivalence candidate (read-only).\n"
        "Theoretical comparability only; equivalence_claimed is always false.\n",
        {},
        RPCResult{
            RPCResult::Type::OBJ,
            "",
            "",
            {
                {RPCResult::Type::NUM, "height", "Block height"},
                {RPCResult::Type::STR_HEX, "block_hash", "Block hash"},
                {RPCResult::Type::STR, "oru_supply_status", "CES_* for rs_oru_supply"},
                {RPCResult::Type::STR, "res_supply_status", "CES_* for rs_res_supply"},
                {RPCResult::Type::STR, "invariant_status", "CES_* for FVR-E invariants"},
                {RPCResult::Type::STR, "witness_chain_status", "CES_* for FVR-F linkage"},
                {RPCResult::Type::ARR, "blocking_reasons", "Audit ids incl. Draw + proof debt",
                 {{RPCResult::Type::STR, "reason", ""}}},
                {RPCResult::Type::BOOL, "equivalence_claimed", "Always false"},
                {RPCResult::Type::STR, "coq_equiv_runtime", "NOT_ESTABLISHED"},
            }},
        RPCExamples{
            HelpExampleCli("getruntimeequivalencecandidate", "") +
            HelpExampleRpc("getruntimeequivalencecandidate", "")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            (void)self;
            (void)request;
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            LOCK(cs_main);
            Chainstate& chainstate = chainman.ActiveChainstate();
            const CBlockIndex* pindex = chainstate.m_chain.Tip();
            if (!pindex) {
                throw JSONRPCError(RPC_MISC_ERROR, "No chain tip");
            }
            const std::optional<Consensus::WitnessEquivalenceCandidate> candidate =
                Consensus::ResolveWitnessEquivalenceCandidateAt(pindex);
            if (!candidate.has_value()) {
                throw JSONRPCError(RPC_MISC_ERROR, "No equivalence candidate at latest witness");
            }
            return Consensus::WitnessEquivalenceCandidateToJson(*candidate);
        },
    };
}

static RPCHelpMan getruntimeequivalencecandidatebyheight()
{
    return RPCHelpMan{
        "getruntimeequivalencecandidatebyheight",
        "\nRuntime↔Coq witness equivalence candidate at height (read-only).\n",
        {
            {"height", RPCArg::Type::NUM, RPCArg::Optional::NO, "Block height"},
        },
        RPCResult{
            RPCResult::Type::OBJ,
            "",
            "",
            {
                {RPCResult::Type::NUM, "height", "Block height"},
                {RPCResult::Type::STR_HEX, "block_hash", "Block hash"},
                {RPCResult::Type::STR, "oru_supply_status", "CES_* for rs_oru_supply"},
                {RPCResult::Type::STR, "res_supply_status", "CES_* for rs_res_supply"},
                {RPCResult::Type::STR, "invariant_status", "CES_* for FVR-E invariants"},
                {RPCResult::Type::STR, "witness_chain_status", "CES_* for FVR-F linkage"},
                {RPCResult::Type::ARR, "blocking_reasons", "Audit ids",
                 {{RPCResult::Type::STR, "reason", ""}}},
                {RPCResult::Type::BOOL, "equivalence_claimed", "Always false"},
            }},
        RPCExamples{
            HelpExampleCli("getruntimeequivalencecandidatebyheight", "100") +
            HelpExampleRpc("getruntimeequivalencecandidatebyheight", "100")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            (void)self;
            const int height = request.params[0].getInt<int>();
            if (height < 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Height must be non-negative");
            }
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            LOCK(cs_main);
            Chainstate& chainstate = chainman.ActiveChainstate();
            const CBlockIndex* pindex = chainstate.m_chain[height];
            if (!pindex) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");
            }
            const std::optional<Consensus::WitnessEquivalenceCandidate> candidate =
                Consensus::ResolveWitnessEquivalenceCandidateAt(pindex);
            if (!candidate.has_value()) {
                throw JSONRPCError(RPC_MISC_ERROR, "No equivalence candidate at height");
            }
            return Consensus::WitnessEquivalenceCandidateToJson(*candidate);
        },
    };
}

static RPCHelpMan exportruntimeequivalencecandidate()
{
    return RPCHelpMan{
        "exportruntimeequivalencecandidate",
        "\nCoq-readable equivalence candidate export (FVR-I + formal layer).\n"
        "Observation only; equivalence_claimed is always false.\n",
        {
            {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED,
             "Block hash or height (default: active tip)"},
        },
        RPCResult{
            RPCResult::Type::OBJ,
            "",
            "",
            {
                {RPCResult::Type::STR, "task", "FVR-J"},
                {RPCResult::Type::BOOL, "equivalence_claimed", "Always false"},
                {RPCResult::Type::OBJ, "runtime_candidate", "FVR-I candidate JSON"},
                {RPCResult::Type::OBJ, "coq_formal_layer",
                 "RuntimeWitness / CoqWitnessRow / CHASReport / CandidateEquivalenceObservation"},
            }},
        RPCExamples{
            HelpExampleCli("exportruntimeequivalencecandidate", "") +
            HelpExampleCli("exportruntimeequivalencecandidate", "100") +
            HelpExampleRpc("exportruntimeequivalencecandidate", "")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            (void)self;
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            LOCK(cs_main);
            Chainstate& chainstate = chainman.ActiveChainstate();
            const CBlockIndex* pindex = chainstate.m_chain.Tip();
            if (!request.params[0].isNull()) {
                pindex = ParseHashOrHeight(request.params[0], chainman);
            }
            if (!pindex) {
                throw JSONRPCError(RPC_MISC_ERROR, "No chain tip");
            }
            const std::optional<Consensus::CoqEquivalenceCandidateExport> exp =
                Consensus::ResolveCoqEquivalenceCandidateExportAt(pindex);
            if (!exp.has_value()) {
                throw JSONRPCError(RPC_MISC_ERROR, "No Coq equivalence candidate export at block");
            }
            return Consensus::CoqEquivalenceCandidateExportToJson(*exp);
        },
    };
}

static RPCHelpMan getphase5summary()
{
    return RPCHelpMan{
        "getphase5summary",
        "\nPhase-5 canonical summary (FVR-A..L freeze snapshot).\n"
        "equivalence_claimed always false.\n",
        {
            {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED,
             "Block hash or height (default: active tip)"},
        },
        RPCResult{
            RPCResult::Type::OBJ,
            "",
            "",
            {
                {RPCResult::Type::NUM, "height", "Resolved block height"},
                {RPCResult::Type::BOOL, "equivalence_claimed", "Always false"},
                {RPCResult::Type::STR, "closure_status", "FVR-L closure gate"},
                {RPCResult::Type::STR, "freeze_protocol_link", "Protocol freeze doc id"},
                {RPCResult::Type::ARR, "phase5_layer_catalog", "FVR layer entries",
                 {{RPCResult::Type::OBJ, "layer", "ple_task_id / ple_status"}}},
            }},
        RPCExamples{
            HelpExampleCli("getphase5summary", "") +
            HelpExampleCli("getphase5summary", "100") +
            HelpExampleRpc("getphase5summary", "")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            (void)self;
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            LOCK(cs_main);
            Chainstate& chainstate = chainman.ActiveChainstate();
            const CBlockIndex* pindex = chainstate.m_chain.Tip();
            if (!request.params[0].isNull()) {
                pindex = ParseHashOrHeight(request.params[0], chainman);
            }
            if (!pindex) {
                throw JSONRPCError(RPC_MISC_ERROR, "No chain tip");
            }
            const std::optional<Consensus::Phase5SummaryExport> exp =
                Consensus::ResolvePhase5SummaryAt(pindex);
            if (!exp.has_value()) {
                throw JSONRPCError(RPC_MISC_ERROR, "No Phase-5 summary at block");
            }
            return Consensus::Phase5SummaryExportToJson(*exp);
        },
    };
}

static RPCHelpMan getphase5summaryfinal()
{
    return RPCHelpMan{
        "getphase5summaryfinal",
        "\nPhase-5 final summary linking FVR-A..N, PROOF-023..026 gates,\n"
        "ECS partition, and MAINNET_PROTOCOL_FREEZE_DECLARATION.\n"
        "equivalence_claimed always false.\n",
        {
            {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED,
             "Block hash or height (default: active tip)"},
        },
        RPCResult{
            RPCResult::Type::OBJ,
            "",
            "",
            {
                {RPCResult::Type::NUM, "height", "Resolved block height"},
                {RPCResult::Type::BOOL, "equivalence_claimed", "Always false"},
                {RPCResult::Type::BOOL, "candidate_equivalence_runtime_ready", "Always false at baseline"},
                {RPCResult::Type::BOOL, "cpp01_candidate_equivalence_ready", "Always false at baseline"},
                {RPCResult::Type::STR, "pd_4f_ob_02", "READY"},
                {RPCResult::Type::STR, "pd_4f_cpp_01", "OPEN"},
                {RPCResult::Type::NUM, "golden_vector_total", "12"},
                {RPCResult::Type::NUM, "golden_vector_materialized", "9 at h<=501 partial batch"},
                {RPCResult::Type::STR, "freeze_protocol_link", "Protocol freeze doc id"},
            }},
        RPCExamples{
            HelpExampleCli("getphase5summaryfinal", "") +
            HelpExampleCli("getphase5summaryfinal", "501") +
            HelpExampleRpc("getphase5summaryfinal", "")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            (void)self;
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            LOCK(cs_main);
            Chainstate& chainstate = chainman.ActiveChainstate();
            const CBlockIndex* pindex = chainstate.m_chain.Tip();
            if (!request.params[0].isNull()) {
                pindex = ParseHashOrHeight(request.params[0], chainman);
            }
            if (!pindex) {
                throw JSONRPCError(RPC_MISC_ERROR, "No chain tip");
            }
            const std::optional<Consensus::Phase5SummaryFinalExport> exp =
                Consensus::ResolvePhase5SummaryFinalAt(pindex);
            if (!exp.has_value()) {
                throw JSONRPCError(RPC_MISC_ERROR, "No Phase-5 final summary at block");
            }
            return Consensus::Phase5SummaryFinalExportToJson(*exp);
        },
    };
}

static RPCHelpMan getruntimevectorstatus()
{
    return RPCHelpMan{
        "getruntimevectorstatus",
        "\nGolden vector runtime verification harness (read-only).\n"
        "Compares normative PROOF-001..004 vectors against FVR witness exports.\n"
        "Evidence only; equivalence_claimed is always false.\n",
        {
            {"target", RPCArg::Type::STR, RPCArg::Optional::OMITTED,
             "Golden vector id (e.g. GV-ORU-0001) or \"batch\" for all twelve vectors (default: batch)"},
            {"verify_fvr", RPCArg::Type::BOOL, RPCArg::Optional::OMITTED,
             "When true, require FVR-F/G witness rows (default: false)"},
        },
        RPCResult{
            RPCResult::Type::OBJ,
            "",
            "",
            {
                {RPCResult::Type::STR, "harness_version", "Harness schema version"},
                {RPCResult::Type::BOOL, "equivalence_claimed", "Always false"},
                {RPCResult::Type::STR, "coq_equiv_runtime", "NOT_ESTABLISHED"},
                {RPCResult::Type::STR, "draw_equivalence", "PROHIBITED"},
                {RPCResult::Type::STR, "vector_id", "Present for single-vector response"},
                {RPCResult::Type::STR, "verification_status",
                 "GVR_PASS | GVR_FAIL | GVR_PARTIAL | GVR_NOT_EXECUTED"},
                {RPCResult::Type::NUM, "mismatch_count", "Required field mismatches"},
                {RPCResult::Type::NUM, "checked_fields", "Compared field count"},
                {RPCResult::Type::BOOL, "runtime_witness_present", "FVR export obtained"},
                {RPCResult::Type::ARR, "failed_fields", "Required fields that mismatched",
                 {{RPCResult::Type::STR, "field", ""}}},
                {RPCResult::Type::ARR, "notes", "Harness notes",
                 {{RPCResult::Type::STR, "note", ""}}},
                {RPCResult::Type::ARR, "field_results", "Per-field comparison rows",
                 {{RPCResult::Type::OBJ, "field_result", ""}}},
                {RPCResult::Type::OBJ, "summary", "Batch counts (batch mode only)"},
                {RPCResult::Type::ARR, "vectors", "Per-vector reports (batch mode only)",
                 {{RPCResult::Type::OBJ, "vector_report", ""}}},
            }},
        RPCExamples{
            HelpExampleCli("getruntimevectorstatus", "") +
            HelpExampleCli("getruntimevectorstatus", "GV-ORU-0001") +
            HelpExampleCli("getruntimevectorstatus", "batch true") +
            HelpExampleRpc("getruntimevectorstatus", "\"batch\", true")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            (void)self;
            std::string target = "batch";
            if (!request.params[0].isNull()) {
                target = request.params[0].get_str();
            }
            const bool verify_fvr = !request.params[1].isNull() && request.params[1].get_bool();

            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            proof::SetGoldenVectorVerifyFvrForRpc(verify_fvr);
            LOCK(cs_main);
            Chainstate& chainstate = chainman.ActiveChainstate();
            const Consensus::Params& consensus = chainman.GetConsensus();
            CCoinsViewCache& coins_view = chainstate.CoinsTip();

            if (target == "batch") {
                std::vector<proof::VectorVerificationReport> reports;
                reports.reserve(proof::GOLDEN_VECTOR_COUNT);
                for (size_t i = 0; i < proof::GOLDEN_VECTOR_COUNT; ++i) {
                    const proof::GoldenVectorSpec& spec = proof::GetGoldenVectorSpec(i);
                    if (spec.height >= 0 && spec.height <= chainstate.m_chain.Height()) {
                        const CBlockIndex* pindex = chainstate.m_chain[spec.height];
                        reports.push_back(proof::EvaluateGoldenVectorOnChain(spec, pindex, coins_view, consensus));
                    } else {
                        proof::RuntimeWitnessSnapshot empty;
                        reports.push_back(proof::EvaluateGoldenVector(spec, empty));
                    }
                }
                return proof::GoldenVectorBatchReportToJson(reports);
            }

            const proof::GoldenVectorSpec* spec = proof::FindGoldenVectorSpec(target);
            if (spec == nullptr) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Unknown golden vector id");
            }

            const CBlockIndex* pindex = nullptr;
            if (spec->height >= 0 && spec->height <= chainstate.m_chain.Height()) {
                pindex = chainstate.m_chain[spec->height];
            }
            const proof::VectorVerificationReport report =
                pindex != nullptr
                    ? proof::EvaluateGoldenVectorOnChain(*spec, pindex, coins_view, consensus)
                    : proof::EvaluateGoldenVector(*spec, proof::RuntimeWitnessSnapshot{});

            return proof::VectorVerificationReportToJson(report);
        },
    };
}

static RPCHelpMan res_mint_unit_cost()
{
    return RPCHelpMan{
        "res_mint_unit_cost",
        "\nReturn the continuous height-based destruction cost destroy_cost(h) and the exact burns that mint at this height.\n"
        "destroy_cost is display-only and may sit one atom below the whole-RES construction burn.\n"
        "Fails if height < 0. Zero is never returned as a destroy cost for height >= 0.\n",
        {
            {"height", RPCArg::Type::NUM, RPCArg::Optional::OMITTED,
             "Block height (default: the next block, tip + 1)"},
        },
        RPCResult{RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::NUM, "height", "Height the destruction cost is quoted at"},
                {RPCResult::Type::NUM, "destroy_cost", "Current destruction cost: display destroy_cost(h) in ORU atoms per whole RES; do not feed this back as B"},
                {RPCResult::Type::NUM, "destroy_cost_next", "Destruction cost at height + 1 (next block, not an epoch step)"},
                {RPCResult::Type::NUM, "min_atom_burn", "Smallest ORU burn that mints 1 RES atom at this height"},
                {RPCResult::Type::NUM, "min_whole_res_burn", "Smallest ORU burn that mints 1 whole RES at this height"},
                {RPCResult::Type::BOOL, "authorized", "Always true on success"},
            }},
        RPCExamples{
            HelpExampleCli("res_mint_unit_cost", "") +
            HelpExampleCli("res_mint_unit_cost", "0") +
            HelpExampleRpc("res_mint_unit_cost", "210000")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            int height = 0;
            if (request.params[0].isNull()) {
                LOCK(cs_main);
                const CBlockIndex* tip = chainman.ActiveChain().Tip();
                CHECK_NONFATAL(tip);
                height = tip->nHeight + 1;
            } else {
                height = request.params[0].getInt<int>();
            }
            if (height < 0 || !Consensus::ResMintAuthorizedAtHeight(height)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                    "mint height must be non-negative");
            }
            const CAmount destroy_cost = Consensus::ResDestroyCostAtHeight(height);
            if (destroy_cost <= 0) {
                throw JSONRPCError(RPC_MISC_ERROR,
                    "mint is not authorized at this height");
            }
            UniValue result(UniValue::VOBJ);
            result.pushKV("height", height);
            result.pushKV("destroy_cost", destroy_cost);
            result.pushKV("destroy_cost_next", Consensus::ResDestroyCostAtHeight(height + 1));
            result.pushKV("min_atom_burn", Consensus::ResMinMintBurnAtoms(height));
            result.pushKV("min_whole_res_burn", Consensus::ResMinBurnForAtoms(Consensus::RES_COIN, height));
            result.pushKV("authorized", true);
            return result;
        },
    };
}

static RPCHelpMan getruntimeequivalenceclosure()
{
    return RPCHelpMan{
        "getruntimeequivalenceclosure",
        "\nEquivalence closure / proof-permission boundary (read-only).\n"
        "Does NOT prove Coq ≡ Runtime; reports when proof attempt is permitted.\n",
        {
            {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED,
             "Block hash or height (default: active tip)"},
        },
        RPCResult{
            RPCResult::Type::OBJ,
            "",
            "",
            {
                {RPCResult::Type::STR, "closure_status", "OPEN | PARTIAL | READY | PROHIBITED"},
                {RPCResult::Type::BOOL, "closure_allowed", "Proof permission gate"},
                {RPCResult::Type::BOOL, "equivalence_claimed", "Always false"},
                {RPCResult::Type::ARR, "missing_layers", "Absent FVR layers",
                 {{RPCResult::Type::STR, "layer", ""}}},
                {RPCResult::Type::ARR, "active_layers", "Present FVR layers",
                 {{RPCResult::Type::STR, "layer", ""}}},
            }},
        RPCExamples{
            HelpExampleCli("getruntimeequivalenceclosure", "") +
            HelpExampleCli("getruntimeequivalenceclosure", "100") +
            HelpExampleRpc("getruntimeequivalenceclosure", "")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            (void)self;
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            LOCK(cs_main);
            Chainstate& chainstate = chainman.ActiveChainstate();
            const CBlockIndex* pindex = chainstate.m_chain.Tip();
            if (!request.params[0].isNull()) {
                pindex = ParseHashOrHeight(request.params[0], chainman);
            }
            if (!pindex) {
                throw JSONRPCError(RPC_MISC_ERROR, "No chain tip");
            }
            const std::optional<Consensus::EquivalenceClosureResult> result =
                Consensus::ResolveEquivalenceClosureAt(pindex);
            if (!result.has_value()) {
                throw JSONRPCError(RPC_MISC_ERROR, "No equivalence closure state at block");
            }
            return Consensus::EquivalenceClosureResultToJson(*result);
        },
    };
}

static RPCHelpMan exportruntimeglobalcandidateequivalence()
{
    return RPCHelpMan{
        "exportruntimeglobalcandidateequivalence",
        "\nGlobal CHAS + candidate equivalence export (Coq-auditable).\n"
        "Aggregates FVR-H and FVR-J; equivalence_claimed is always false.\n",
        {
            {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED,
             "Block hash or height (default: active tip)"},
        },
        RPCResult{
            RPCResult::Type::OBJ,
            "",
            "",
            {
                {RPCResult::Type::STR, "task", "FVR-K"},
                {RPCResult::Type::BOOL, "equivalence_claimed", "Always false"},
                {RPCResult::Type::OBJ, "GlobalCandidateEquivalenceObservation",
                 "gceo_oru_global_status / gceo_res_global_status"},
                {RPCResult::Type::OBJ, "chas_report", "FVR-H alignment vectors"},
                {RPCResult::Type::OBJ, "runtime_candidate", "FVR-I candidate record"},
            }},
        RPCExamples{
            HelpExampleCli("exportruntimeglobalcandidateequivalence", "") +
            HelpExampleCli("exportruntimeglobalcandidateequivalence", "100") +
            HelpExampleRpc("exportruntimeglobalcandidateequivalence", "")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            (void)self;
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            LOCK(cs_main);
            Chainstate& chainstate = chainman.ActiveChainstate();
            const CBlockIndex* pindex = chainstate.m_chain.Tip();
            if (!request.params[0].isNull()) {
                pindex = ParseHashOrHeight(request.params[0], chainman);
            }
            if (!pindex) {
                throw JSONRPCError(RPC_MISC_ERROR, "No chain tip");
            }
            const std::optional<Consensus::GlobalCandidateEquivalenceExport> exp =
                Consensus::ResolveGlobalCandidateEquivalenceExportAt(pindex);
            if (!exp.has_value()) {
                throw JSONRPCError(RPC_MISC_ERROR, "No global candidate equivalence export at block");
            }
            return Consensus::GlobalCandidateEquivalenceExportToJson(*exp);
        },
    };
}

static RPCHelpMan exportruntimesupply()
{
    return RPCHelpMan{
        "exportruntimesupply",
        "\nAlias for `getruntimesupply` (full export for Coq / debug tooling).\n"
        "Read-only; identical payload.\n",
        {
            {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED,
             "Block hash or height (default: active tip)"},
        },
        RPCResult{
            RPCResult::Type::OBJ,
            "",
            std::string{"Same JSON as getruntimesupply"},
        },
        RPCExamples{
            HelpExampleCli("exportruntimesupply", "") +
            "# CLI wrapper: contrib/tools/runtimesupply-export.sh [blockhash]\n"},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            LOCK(cs_main);
            Chainstate& chainstate = chainman.ActiveChainstate();
            const CBlockIndex* pindex = chainstate.m_chain.Tip();
            if (!request.params[0].isNull()) {
                pindex = ParseHashOrHeight(request.params[0], chainman);
            }
            return GetRuntimeSupplyRpcResult(chainstate, chainman, pindex);
        },
    };
}

RPCHelpMan getdeterministicstatehash()
{
    return RPCHelpMan{"getdeterministicstatehash",
                "\nReturns the deterministic hash of the chainstate for replay verification.\n"
                "This hash is byte-identical across nodes for the same blockchain state,\n"
                "independent of unordered_map iteration order.\n",
                {},
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR_HEX, "state_hash", "Deterministic hash of the chainstate"},
                        {RPCResult::Type::NUM, "height", "Current block height"},
                        {RPCResult::Type::STR_HEX, "best_block_hash", "Hash of the best block"},
                    }
                },
                RPCExamples{
                    HelpExampleCli("getdeterministicstatehash", "") + HelpExampleRpc("getdeterministicstatehash", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            const NodeContext& node = EnsureAnyNodeContext(request.context);
            const ChainstateManager& chainman = EnsureChainman(node);
            LOCK(cs_main);
            const Chainstate& chainstate = chainman.ActiveChainstate();
            
            uint256 state_hash = const_cast<Chainstate&>(chainstate).ComputeDeterministicStateHash();
            int height = chainstate.m_chain.Height();
            uint256 best_block_hash = chainstate.m_chain.Tip() ? chainstate.m_chain.Tip()->GetBlockHash() : uint256();
            
            UniValue result(UniValue::VOBJ);
            result.pushKV("state_hash", state_hash.GetHex());
            result.pushKV("height", height);
            result.pushKV("best_block_hash", best_block_hash.GetHex());
            return result;
        },
    };
}

static RPCHelpMan getchainstatesummary()
{
    return RPCHelpMan{
        "getchainstatesummary",
        "\nReturns a fixed-size CSM1 diagnostic snapshot of the active chainstate.\n"
        "Read-only; does not alter consensus. The chainstate view is always the **active tip**;\n"
        "if `blockhash` is passed it must equal that tip's hash. Use `diffchainstatesummaries`\n"
        "to compare blobs from two nodes.\n",
        {
            {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED,
             "Must equal the active chain tip hash if provided (sanity check); default: tip"},
        },
        RPCResult{
            RPCResult::Type::OBJ,
            "",
            "",
            {
                {RPCResult::Type::NUM, "height", "Chain height at snapshot"},
                {RPCResult::Type::STR_HEX, "block_hash", "Tip block hash"},
                {RPCResult::Type::STR_HEX, "state_hash", "STEP-0101 deterministic state hash"},
                {RPCResult::Type::NUM, "utxo_count", "UTXO count"},
                {RPCResult::Type::STR_AMOUNT, "res_balance", "Sum of nValue over RES UTXOs"},
                {RPCResult::Type::STR_AMOUNT, "other_balance", "Sum of nValue over non-RES UTXOs"},
                {RPCResult::Type::NUM, "mint_count", "RES-shaped UTXO count"},
                {RPCResult::Type::NUM, "lock_count", "Witness-V1 lock-shape UTXO count"},
                {RPCResult::Type::NUM, "converge_count", "Convergence-shaped UTXO count (expect 0)"},
                {RPCResult::Type::STR_HEX, "chainstatesummary_hex", "Full CSM1 wire image (hex)"},
            }},
        RPCExamples{HelpExampleCli("getchainstatesummary", "") +
                    HelpExampleRpc("getchainstatesummary", "")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            LOCK(cs_main);
            Chainstate& chainstate = chainman.ActiveChainstate();
            const CBlockIndex* tip = chainstate.m_chain.Tip();
            if (!tip) {
                throw JSONRPCError(RPC_MISC_ERROR, "No chain tip");
            }
            if (!request.params[0].isNull()) {
                const uint256 req_hash = ParseHashV(request.params[0], "blockhash");
                if (req_hash != tip->GetBlockHash()) {
                    throw JSONRPCError(RPC_INVALID_PARAMETER,
                        "blockhash must equal the active chain tip hash (snapshot uses CoinsTip() at tip only)");
                }
            }
            const CBlockIndex* pindex = tip;
            // The CSM1 trailer hashes the memory-only W1 observational ledgers, which sit
            // at their -1 default until some path-fold fills them. Warm them here for the
            // same reason Chainstate::ComputeDeterministicStateHash does: otherwise a
            // freshly restarted node hashes -1 and reports a different state_hash than
            // getdeterministicstatehash, and than itself once any supply RPC has run.
            Consensus::WarmSupplyLedgerCaches(const_cast<CBlockIndex*>(pindex),
                                              chainman.GetParams().GetConsensus());
            const Consensus::ChainstateSummary summary =
                Consensus::CaptureChainstateSummary(chainstate.CoinsTip(), pindex, nullptr);

            UniValue result(UniValue::VOBJ);
            result.pushKV("height", summary.height);
            result.pushKV("block_hash", summary.block_hash.GetHex());
            result.pushKV("state_hash", summary.state_hash.GetHex());
            result.pushKV("utxo_count", static_cast<uint64_t>(summary.utxo_count));
            result.pushKV("res_balance", ValueFromAmountAsset(static_cast<CAmount>(summary.res_balance), ASSET_RES));
            result.pushKV("other_balance", ValueFromAmount(static_cast<CAmount>(summary.other_balance)));
            result.pushKV("mint_count", static_cast<uint64_t>(summary.mint_count));
            result.pushKV("lock_count", static_cast<uint64_t>(summary.lock_count));
            result.pushKV("converge_count", static_cast<uint64_t>(summary.converge_count));
            result.pushKV("chainstatesummary_hex", HexStr(summary.Serialize()));
            return result;
        },
    };
}

static RPCHelpMan diffchainstatesummaries()
{
    return RPCHelpMan{
        "diffchainstatesummaries",
        "\nDeterministic comparison of two CSM1 blobs from `getchainstatesummary` (hex).\n"
        "Returns the first logical field mismatch (canonical order) and the first raw\n"
        "byte mismatch on the wire. Diagnostic only.\n",
        {
            {"summary_a", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "First CSM1 summary (hex)"},
            {"summary_b", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "Second CSM1 summary (hex)"},
        },
        RPCResult{
            RPCResult::Type::OBJ,
            "",
            "",
            {
                {RPCResult::Type::OBJ,
                 "logical",
                 "First divergence in semantic field order (STEP-0102)",
                 {
                     {RPCResult::Type::BOOL, "identical", "True iff every compared field matches"},
                     {RPCResult::Type::STR, "field", "First mismatching field name (empty if identical)"},
                     {RPCResult::Type::STR, "value_a", "Side A value (decimal or hex)"},
                     {RPCResult::Type::STR, "value_b", "Side B value"},
                     {RPCResult::Type::NUM, "bucket_index", "UTXO bucket index if field is utxo_bucket_hashes"},
                     {RPCResult::Type::NUM, "serialized_byte_offset", "CSM1 byte offset of the field start, or -1"},
                     {RPCResult::Type::STR, "wire_segment", "CSM1 region label"},
                     {RPCResult::Type::STR, "message", "Human-readable summary line"},
                 }},
                {RPCResult::Type::OBJ,
                 "serialized",
                 "First divergence in raw wire lexicographic scan",
                 {
                     {RPCResult::Type::BOOL, "identical", "True iff wire images are byte-identical"},
                     {RPCResult::Type::STR, "field", "Usually `serialized_byte` or `wire_length`"},
                     {RPCResult::Type::STR, "value_a", "Hex byte or decimal length"},
                     {RPCResult::Type::STR, "value_b", "Hex byte or decimal length"},
                     {RPCResult::Type::NUM, "serialized_byte_offset", "First differing byte offset (or length split)"},
                     {RPCResult::Type::STR, "wire_segment", "CSM1 region containing the differing byte"},
                     {RPCResult::Type::STR, "message", "Human-readable wire diff line"},
                 }},
            }},
        RPCExamples{HelpExampleCli("diffchainstatesummaries", "\"<hex_a>\" \"<hex_b>\"") +
                    HelpExampleRpc("diffchainstatesummaries", "\"<hex_a>\", \"<hex_b>\"")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue {
            std::vector<unsigned char> da = ParseHex(request.params[0].get_str());
            std::vector<unsigned char> db = ParseHex(request.params[1].get_str());
            const std::string sa(reinterpret_cast<const char*>(da.data()), da.size());
            const std::string sb(reinterpret_cast<const char*>(db.data()), db.size());

            Consensus::ChainstateSummary a;
            Consensus::ChainstateSummary b;
            if (!a.Deserialize(sa) || !b.Deserialize(sb)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid CSM1 summary hex (wrong magic or length)");
            }

            const Consensus::DiffResult dr = Consensus::DiffChainstateSummaries(a, b);
            const Consensus::DiffResult wr = Consensus::DiffSerializedChainstateSummaries(sa, sb);

            UniValue logical(UniValue::VOBJ);
            logical.pushKV("identical", dr.identical);
            logical.pushKV("field", dr.field);
            logical.pushKV("value_a", dr.value_a);
            logical.pushKV("value_b", dr.value_b);
            logical.pushKV("bucket_index", dr.bucket_index);
            logical.pushKV("serialized_byte_offset", dr.serialized_byte_offset);
            logical.pushKV("wire_segment", dr.wire_segment);
            logical.pushKV("message", dr.ToString());

            UniValue ser(UniValue::VOBJ);
            ser.pushKV("identical", wr.identical);
            ser.pushKV("field", wr.field);
            ser.pushKV("value_a", wr.value_a);
            ser.pushKV("value_b", wr.value_b);
            ser.pushKV("bucket_index", wr.bucket_index);
            ser.pushKV("serialized_byte_offset", wr.serialized_byte_offset);
            ser.pushKV("wire_segment", wr.wire_segment);
            ser.pushKV("message", wr.ToString());

            UniValue out(UniValue::VOBJ);
            out.pushKV("logical", logical);
            out.pushKV("serialized", ser);
            return out;
        },
    };
}

RPCHelpMan getblockchaininfo()
{
    return RPCHelpMan{"getblockchaininfo",
        "Returns an object containing various state info regarding blockchain processing.\n",
        {},
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "chain", "current network name (main, test, regtest)"},
                {RPCResult::Type::NUM, "blocks", "the height of the most-work fully-validated chain. The genesis block has height 0"},
                {RPCResult::Type::NUM, "headers", "the current number of headers we have validated"},
                {RPCResult::Type::STR, "bestblockhash", "the hash of the currently best block"},
                {RPCResult::Type::NUM, "difficulty", "the current difficulty"},
                {RPCResult::Type::NUM_TIME, "time", "The block time expressed in " + UNIX_EPOCH_TIME},
                {RPCResult::Type::NUM_TIME, "mediantime", "The median block time expressed in " + UNIX_EPOCH_TIME},
                {RPCResult::Type::NUM, "verificationprogress", "estimate of verification progress [0..1]"},
                {RPCResult::Type::BOOL, "initialblockdownload", "(debug information) estimate of whether this node is in Initial Block Download mode"},
                {RPCResult::Type::STR_HEX, "chainwork", "total amount of work in active chain, in hexadecimal"},
                {RPCResult::Type::NUM, "size_on_disk", "the estimated size of the block and undo files on disk"},
                {RPCResult::Type::BOOL, "pruned", "if the blocks are subject to pruning"},
                {RPCResult::Type::NUM, "pruneheight", /*optional=*/true, "height of the last block pruned, plus one (only present if pruning is enabled)"},
                {RPCResult::Type::BOOL, "automatic_pruning", /*optional=*/true, "whether automatic pruning is enabled (only present if pruning is enabled)"},
                {RPCResult::Type::NUM, "prune_target_size", /*optional=*/true, "the target size used by pruning (only present if automatic pruning is enabled)"},
                {RPCResult::Type::STR, "warnings", "any network and blockchain warnings"},
                {RPCResult::Type::STR_AMOUNT, "cumulative_convergence", "Cumulative ORU destroyed by Convergence (CVG2). Not RFC-018 fee burn and not circulating supply"},
                {RPCResult::Type::STR_AMOUNT, "current_convergence_sum", "ORU convergenced since the last RES mint block (window total at tip)"},
            }},
        RPCExamples{
            HelpExampleCli("getblockchaininfo", "")
            + HelpExampleRpc("getblockchaininfo", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    LOCK(cs_main);
    Chainstate& active_chainstate = chainman.ActiveChainstate();

    const CBlockIndex& tip{*CHECK_NONFATAL(active_chainstate.m_chain.Tip())};
    const int height{tip.nHeight};
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("chain", chainman.GetParams().NetworkIDString());
    obj.pushKV("blocks", height);
    obj.pushKV("headers", chainman.m_best_header ? chainman.m_best_header->nHeight : -1);
    obj.pushKV("bestblockhash", tip.GetBlockHash().GetHex());
    obj.pushKV("difficulty", GetDifficulty(&tip));
    obj.pushKV("time", tip.GetBlockTime());
    obj.pushKV("mediantime", tip.GetMedianTimePast());
    obj.pushKV("verificationprogress", GuessVerificationProgress(chainman.GetParams().TxData(), &tip));
    obj.pushKV("initialblockdownload", active_chainstate.IsInitialBlockDownload());
    obj.pushKV("chainwork", tip.nChainWork.GetHex());
    obj.pushKV("size_on_disk", chainman.m_blockman.CalculateCurrentUsage());
    obj.pushKV("pruned", chainman.m_blockman.IsPruneMode());
    if (chainman.m_blockman.IsPruneMode()) {
        obj.pushKV("pruneheight", chainman.m_blockman.GetFirstStoredBlock(tip)->nHeight);

        const bool automatic_pruning{chainman.m_blockman.GetPruneTarget() != BlockManager::PRUNE_TARGET_MANUAL};
        obj.pushKV("automatic_pruning",  automatic_pruning);
        if (automatic_pruning) {
            obj.pushKV("prune_target_size", chainman.m_blockman.GetPruneTarget());
        }
    }

    CAmount cumulative_convergence{0};
    CAmount current_convergence_sum{0};
    if (FillTipConvergenceChainSummary(tip, chainman, cumulative_convergence, current_convergence_sum)) {
        obj.pushKV("cumulative_convergence", ValueFromAmount(cumulative_convergence));
        obj.pushKV("current_convergence_sum", ValueFromAmount(current_convergence_sum));
    }

    obj.pushKV("warnings", GetWarnings(false).original);
    return obj;
},
    };
}

namespace {
const std::vector<RPCResult> RPCHelpForDeployment{
    {RPCResult::Type::STR, "type", "one of \"buried\", \"bip9\""},
    {RPCResult::Type::NUM, "height", /*optional=*/true, "height of the first block which the rules are or will be enforced (only for \"buried\" type, or \"bip9\" type with \"active\" status)"},
    {RPCResult::Type::BOOL, "active", "true if the rules are enforced for the mempool and the next block"},
    {RPCResult::Type::OBJ, "bip9", /*optional=*/true, "status of bip9 softforks (only for \"bip9\" type)",
    {
        {RPCResult::Type::NUM, "bit", /*optional=*/true, "the bit (0-28) in the block version field used to signal this softfork (only for \"started\" and \"locked_in\" status)"},
        {RPCResult::Type::NUM_TIME, "start_time", "the minimum median time past of a block at which the bit gains its meaning"},
        {RPCResult::Type::NUM_TIME, "timeout", "the median time past of a block at which the deployment is considered failed if not yet locked in"},
        {RPCResult::Type::NUM, "min_activation_height", "minimum height of blocks for which the rules may be enforced"},
        {RPCResult::Type::STR, "status", "status of deployment at specified block (one of \"defined\", \"started\", \"locked_in\", \"active\", \"failed\")"},
        {RPCResult::Type::NUM, "since", "height of the first block to which the status applies"},
        {RPCResult::Type::STR, "status_next", "status of deployment at the next block"},
        {RPCResult::Type::OBJ, "statistics", /*optional=*/true, "numeric statistics about signalling for a softfork (only for \"started\" and \"locked_in\" status)",
        {
            {RPCResult::Type::NUM, "period", "the length in blocks of the signalling period"},
            {RPCResult::Type::NUM, "threshold", /*optional=*/true, "the number of blocks with the version bit set required to activate the feature (only for \"started\" status)"},
            {RPCResult::Type::NUM, "elapsed", "the number of blocks elapsed since the beginning of the current period"},
            {RPCResult::Type::NUM, "count", "the number of blocks with the version bit set in the current period"},
            {RPCResult::Type::BOOL, "possible", /*optional=*/true, "returns false if there are not enough blocks left in this period to pass activation threshold (only for \"started\" status)"},
        }},
        {RPCResult::Type::STR, "signalling", /*optional=*/true, "indicates blocks that signalled with a # and blocks that did not with a -"},
    }},
};

UniValue DeploymentInfo(const CBlockIndex* blockindex, const ChainstateManager& chainman)
{
    UniValue softforks(UniValue::VOBJ);
    SoftForkDescPushBack(blockindex, softforks, chainman, Consensus::DEPLOYMENT_HEIGHTINCB);
    SoftForkDescPushBack(blockindex, softforks, chainman, Consensus::DEPLOYMENT_DERSIG);
    SoftForkDescPushBack(blockindex, softforks, chainman, Consensus::DEPLOYMENT_CLTV);
    SoftForkDescPushBack(blockindex, softforks, chainman, Consensus::DEPLOYMENT_CSV);
    SoftForkDescPushBack(blockindex, softforks, chainman, Consensus::DEPLOYMENT_SEGWIT);
    SoftForkDescPushBack(blockindex, softforks, chainman, Consensus::DEPLOYMENT_TESTDUMMY);
    // REMOVED: DEPLOYMENT_TAPROOT reference
    // violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 complexity)
    return softforks;
}
} // anon namespace

RPCHelpMan getdeploymentinfo()
{
    return RPCHelpMan{"getdeploymentinfo",
        "Returns an object containing various state info regarding deployments of consensus changes.",
        {
            {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Default{"hash of current chain tip"}, "The block hash at which to query deployment state"},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "", {
                {RPCResult::Type::STR, "hash", "requested block hash (or tip)"},
                {RPCResult::Type::NUM, "height", "requested block height (or tip)"},
                {RPCResult::Type::OBJ_DYN, "deployments", "", {
                    {RPCResult::Type::OBJ, "xxxx", "name of the deployment", RPCHelpForDeployment}
                }},
            }
        },
        RPCExamples{ HelpExampleCli("getdeploymentinfo", "") + HelpExampleRpc("getdeploymentinfo", "") },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const ChainstateManager& chainman = EnsureAnyChainman(request.context);
            LOCK(cs_main);
            const Chainstate& active_chainstate = chainman.ActiveChainstate();

            const CBlockIndex* blockindex;
            if (request.params[0].isNull()) {
                blockindex = CHECK_NONFATAL(active_chainstate.m_chain.Tip());
            } else {
                const uint256 hash(ParseHashV(request.params[0], "blockhash"));
                blockindex = chainman.m_blockman.LookupBlockIndex(hash);
                if (!blockindex) {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
                }
            }

            UniValue deploymentinfo(UniValue::VOBJ);
            deploymentinfo.pushKV("hash", blockindex->GetBlockHash().ToString());
            deploymentinfo.pushKV("height", blockindex->nHeight);
            deploymentinfo.pushKV("deployments", DeploymentInfo(blockindex, chainman));
            return deploymentinfo;
        },
    };
}

/** Comparison function for sorting the getchaintips heads.  */
struct CompareBlocksByHeight
{
    bool operator()(const CBlockIndex* a, const CBlockIndex* b) const
    {
        /* Make sure that unequal blocks with the same height do not compare
           equal. Use the pointers themselves to make a distinction. */

        if (a->nHeight != b->nHeight)
          return (a->nHeight > b->nHeight);

        return a < b;
    }
};

static RPCHelpMan getchaintips()
{
    return RPCHelpMan{"getchaintips",
                "Return information about all known tips in the block tree,"
                " including the main chain as well as orphaned branches.\n",
                {},
                RPCResult{
                    RPCResult::Type::ARR, "", "",
                    {{RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::NUM, "height", "height of the chain tip"},
                            {RPCResult::Type::STR_HEX, "hash", "block hash of the tip"},
                            {RPCResult::Type::NUM, "branchlen", "zero for main chain, otherwise length of branch connecting the tip to the main chain"},
                            {RPCResult::Type::STR, "status", "status of the chain, \"active\" for the main chain\n"
            "Possible values for status:\n"
            "1.  \"invalid\"               This branch contains at least one invalid block\n"
            "2.  \"headers-only\"          Not all blocks for this branch are available, but the headers are valid\n"
            "3.  \"valid-headers\"         All blocks are available for this branch, but they were never fully validated\n"
            "4.  \"valid-fork\"            This branch is not part of the active chain, but is fully validated\n"
            "5.  \"active\"                This is the tip of the active main chain, which is certainly valid"},
                        }}}},
                RPCExamples{
                    HelpExampleCli("getchaintips", "")
            + HelpExampleRpc("getchaintips", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    LOCK(cs_main);
    CChain& active_chain = chainman.ActiveChain();

    /*
     * Idea: The set of chain tips is the active chain tip, plus orphan blocks which do not have another orphan building off of them.
     * Algorithm:
     *  - Make one pass through BlockIndex(), picking out the orphan blocks, and also storing a set of the orphan block's pprev pointers.
     *  - Iterate through the orphan blocks. If the block isn't pointed to by another orphan, it is a chain tip.
     *  - Add the active chain tip
     */
    std::set<const CBlockIndex*, CompareBlocksByHeight> setTips;
    std::set<const CBlockIndex*> setOrphans;
    std::set<const CBlockIndex*> setPrevs;

    for (const auto& [_, block_index] : chainman.BlockIndex()) {
        if (!active_chain.Contains(&block_index)) {
            setOrphans.insert(&block_index);
            setPrevs.insert(block_index.pprev);
        }
    }

    for (std::set<const CBlockIndex*>::iterator it = setOrphans.begin(); it != setOrphans.end(); ++it) {
        if (setPrevs.erase(*it) == 0) {
            setTips.insert(*it);
        }
    }

    // Always report the currently active tip.
    setTips.insert(active_chain.Tip());

    /* Construct the output array.  */
    UniValue res(UniValue::VARR);
    for (const CBlockIndex* block : setTips) {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("height", block->nHeight);
        obj.pushKV("hash", block->phashBlock->GetHex());

        const int branchLen = block->nHeight - active_chain.FindFork(block)->nHeight;
        obj.pushKV("branchlen", branchLen);

        std::string status;
        if (active_chain.Contains(block)) {
            // This block is part of the currently active chain.
            status = "active";
        } else if (block->nStatus & BLOCK_FAILED_MASK) {
            // This block or one of its ancestors is invalid.
            status = "invalid";
        } else if (!block->HaveTxsDownloaded()) {
            // This block cannot be connected because full block data for it or one of its parents is missing.
            status = "headers-only";
        } else if (block->IsValid(BLOCK_VALID_SCRIPTS)) {
            // This block is fully validated, but no longer part of the active chain. It was probably the active block once, but was reorganized.
            status = "valid-fork";
        } else if (block->IsValid(BLOCK_VALID_TREE)) {
            // The headers for this block are valid, but it has not been validated. It was probably never part of the most-work chain.
            status = "valid-headers";
        } else {
            // No clue.
            status = "unknown";
        }
        obj.pushKV("status", status);

        res.push_back(obj);
    }

    return res;
},
    };
}

static RPCHelpMan preciousblock()
{
    return RPCHelpMan{"preciousblock",
                "\nTreats a block as if it were received before others with the same work.\n"
                "\nA later preciousblock call can override the effect of an earlier one.\n"
                "\nThe effects of preciousblock are not retained across restarts.\n",
                {
                    {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "the hash of the block to mark as precious"},
                },
                RPCResult{RPCResult::Type::NONE, "", ""},
                RPCExamples{
                    HelpExampleCli("preciousblock", "\"blockhash\"")
            + HelpExampleRpc("preciousblock", "\"blockhash\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    uint256 hash(ParseHashV(request.params[0], "blockhash"));
    CBlockIndex* pblockindex;

    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    {
        LOCK(cs_main);
        pblockindex = chainman.m_blockman.LookupBlockIndex(hash);
        if (!pblockindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }
    }

    BlockValidationState state;
    chainman.ActiveChainstate().PreciousBlock(state, pblockindex);

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, state.ToString());
    }

    return UniValue::VNULL;
},
    };
}

static RPCHelpMan invalidateblock()
{
    return RPCHelpMan{"invalidateblock",
                "\nPermanently marks a block as invalid, as if it violated a consensus rule.\n",
                {
                    {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "the hash of the block to mark as invalid"},
                },
                RPCResult{RPCResult::Type::NONE, "", ""},
                RPCExamples{
                    HelpExampleCli("invalidateblock", "\"blockhash\"")
            + HelpExampleRpc("invalidateblock", "\"blockhash\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    uint256 hash(ParseHashV(request.params[0], "blockhash"));
    BlockValidationState state;

    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    CBlockIndex* pblockindex;
    {
        LOCK(cs_main);
        pblockindex = chainman.m_blockman.LookupBlockIndex(hash);
        if (!pblockindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }
    }
    chainman.ActiveChainstate().InvalidateBlock(state, pblockindex);

    if (state.IsValid()) {
        chainman.ActiveChainstate().ActivateBestChain(state);
    }

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, state.ToString());
    }

    return UniValue::VNULL;
},
    };
}

static RPCHelpMan reconsiderblock()
{
    return RPCHelpMan{"reconsiderblock",
                "\nRemoves invalidity status of a block, its ancestors and its descendants, reconsider them for activation.\n"
                "This can be used to undo the effects of invalidateblock.\n",
                {
                    {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "the hash of the block to reconsider"},
                },
                RPCResult{RPCResult::Type::NONE, "", ""},
                RPCExamples{
                    HelpExampleCli("reconsiderblock", "\"blockhash\"")
            + HelpExampleRpc("reconsiderblock", "\"blockhash\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    uint256 hash(ParseHashV(request.params[0], "blockhash"));

    {
        LOCK(cs_main);
        CBlockIndex* pblockindex = chainman.m_blockman.LookupBlockIndex(hash);
        if (!pblockindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }

        chainman.ActiveChainstate().ResetBlockFailureFlags(pblockindex);
    }

    BlockValidationState state;
    chainman.ActiveChainstate().ActivateBestChain(state);

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, state.ToString());
    }

    return UniValue::VNULL;
},
    };
}

static RPCHelpMan getchaintxstats()
{
    return RPCHelpMan{"getchaintxstats",
                "\nCompute statistics about the total number and rate of transactions in the chain.\n",
                {
                    {"nblocks", RPCArg::Type::NUM, RPCArg::DefaultHint{"one month"}, "Size of the window in number of blocks"},
                    {"blockhash", RPCArg::Type::STR_HEX, RPCArg::DefaultHint{"chain tip"}, "The hash of the block that ends the window."},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM_TIME, "time", "The timestamp for the final block in the window, expressed in " + UNIX_EPOCH_TIME},
                        {RPCResult::Type::NUM, "txcount", "The total number of transactions in the chain up to that point"},
                        {RPCResult::Type::STR_HEX, "window_final_block_hash", "The hash of the final block in the window"},
                        {RPCResult::Type::NUM, "window_final_block_height", "The height of the final block in the window."},
                        {RPCResult::Type::NUM, "window_block_count", "Size of the window in number of blocks"},
                        {RPCResult::Type::NUM, "window_tx_count", /*optional=*/true, "The number of transactions in the window. Only returned if \"window_block_count\" is > 0"},
                        {RPCResult::Type::NUM, "window_interval", /*optional=*/true, "The elapsed time in the window in seconds. Only returned if \"window_block_count\" is > 0"},
                        {RPCResult::Type::NUM, "txrate", /*optional=*/true, "The average rate of transactions per second in the window. Only returned if \"window_interval\" is > 0"},
                    }},
                RPCExamples{
                    HelpExampleCli("getchaintxstats", "")
            + HelpExampleRpc("getchaintxstats", "2016")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    const CBlockIndex* pindex;
    int blockcount = 30 * 24 * 60 * 60 / chainman.GetParams().GetConsensus().nPowTargetSpacing; // By default: 1 month

    if (request.params[1].isNull()) {
        LOCK(cs_main);
        pindex = chainman.ActiveChain().Tip();
    } else {
        uint256 hash(ParseHashV(request.params[1], "blockhash"));
        LOCK(cs_main);
        pindex = chainman.m_blockman.LookupBlockIndex(hash);
        if (!pindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }
        if (!chainman.ActiveChain().Contains(pindex)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Block is not in main chain");
        }
    }

    CHECK_NONFATAL(pindex != nullptr);

    if (request.params[0].isNull()) {
        blockcount = std::max(0, std::min(blockcount, pindex->nHeight - 1));
    } else {
        blockcount = request.params[0].getInt<int>();

        if (blockcount < 0 || (blockcount > 0 && blockcount >= pindex->nHeight)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid block count: should be between 0 and the block's height - 1");
        }
    }

    const CBlockIndex& past_block{*CHECK_NONFATAL(pindex->GetAncestor(pindex->nHeight - blockcount))};
    const int64_t nTimeDiff{pindex->GetMedianTimePast() - past_block.GetMedianTimePast()};
    const int nTxDiff = pindex->nChainTx - past_block.nChainTx;

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("time", (int64_t)pindex->nTime);
    ret.pushKV("txcount", (int64_t)pindex->nChainTx);
    ret.pushKV("window_final_block_hash", pindex->GetBlockHash().GetHex());
    ret.pushKV("window_final_block_height", pindex->nHeight);
    ret.pushKV("window_block_count", blockcount);
    if (blockcount > 0) {
        ret.pushKV("window_tx_count", nTxDiff);
        ret.pushKV("window_interval", nTimeDiff);
        if (nTimeDiff > 0) {
            ret.pushKV("txrate", ((double)nTxDiff) / nTimeDiff);
        }
    }

    return ret;
},
    };
}

template<typename T>
static T CalculateTruncatedMedian(std::vector<T>& scores)
{
    size_t size = scores.size();
    if (size == 0) {
        return 0;
    }

    std::sort(scores.begin(), scores.end());
    if (size % 2 == 0) {
        return (scores[size / 2 - 1] + scores[size / 2]) / 2;
    } else {
        return scores[size / 2];
    }
}

void CalculatePercentilesByWeight(CAmount result[NUM_GETBLOCKSTATS_PERCENTILES], std::vector<std::pair<CAmount, int64_t>>& scores, int64_t total_weight)
{
    if (scores.empty()) {
        return;
    }

    std::sort(scores.begin(), scores.end());

    // 10th, 25th, 50th, 75th, and 90th percentile weight units.
    const double weights[NUM_GETBLOCKSTATS_PERCENTILES] = {
        total_weight / 10.0, total_weight / 4.0, total_weight / 2.0, (total_weight * 3.0) / 4.0, (total_weight * 9.0) / 10.0
    };

    int64_t next_percentile_index = 0;
    int64_t cumulative_weight = 0;
    for (const auto& element : scores) {
        cumulative_weight += element.second;
        while (next_percentile_index < NUM_GETBLOCKSTATS_PERCENTILES && cumulative_weight >= weights[next_percentile_index]) {
            result[next_percentile_index] = element.first;
            ++next_percentile_index;
        }
    }

    // Fill any remaining percentiles with the last value.
    for (int64_t i = next_percentile_index; i < NUM_GETBLOCKSTATS_PERCENTILES; i++) {
        result[i] = scores.back().first;
    }
}

template<typename T>
static inline bool SetHasKeys(const std::set<T>& set) {return false;}
template<typename T, typename Tk, typename... Args>
static inline bool SetHasKeys(const std::set<T>& set, const Tk& key, const Args&... args)
{
    return (set.count(key) != 0) || SetHasKeys(set, args...);
}

// outpoint (needed for the utxo index) + nHeight + fCoinBase
static constexpr size_t PER_UTXO_OVERHEAD = sizeof(COutPoint) + sizeof(uint32_t) + sizeof(bool);

static RPCHelpMan getblockstats()
{
    return RPCHelpMan{"getblockstats",
                "\nCompute per block statistics for a given window. All amounts are in Granums.\n"
                "It won't work for some heights with pruning.\n",
                {
                    {"hash_or_height", RPCArg::Type::NUM, RPCArg::Optional::NO, "The block hash or height of the target block",
                     RPCArgOptions{
                         .skip_type_check = true,
                         .type_str = {"", "string or numeric"},
                     }},
                    {"stats", RPCArg::Type::ARR, RPCArg::DefaultHint{"all values"}, "Values to plot (see result below)",
                        {
                            {"height", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Selected statistic"},
                            {"time", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Selected statistic"},
                        },
                        RPCArgOptions{.oneline_description="stats"}},
                },
                RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::NUM, "avgfee", /*optional=*/true, "Average fee in the block"},
                {RPCResult::Type::NUM, "avgfeerate", /*optional=*/true, "Average feerate (in Granums per virtual byte)"},
                {RPCResult::Type::NUM, "avgtxsize", /*optional=*/true, "Average transaction size"},
                {RPCResult::Type::STR_HEX, "blockhash", /*optional=*/true, "The block hash (to check for potential reorgs)"},
                {RPCResult::Type::ARR_FIXED, "feerate_percentiles", /*optional=*/true, "Feerates at the 10th, 25th, 50th, 75th, and 90th percentile weight unit (in Granums per virtual byte)",
                {
                    {RPCResult::Type::NUM, "10th_percentile_feerate", "The 10th percentile feerate"},
                    {RPCResult::Type::NUM, "25th_percentile_feerate", "The 25th percentile feerate"},
                    {RPCResult::Type::NUM, "50th_percentile_feerate", "The 50th percentile feerate"},
                    {RPCResult::Type::NUM, "75th_percentile_feerate", "The 75th percentile feerate"},
                    {RPCResult::Type::NUM, "90th_percentile_feerate", "The 90th percentile feerate"},
                }},
                {RPCResult::Type::NUM, "height", /*optional=*/true, "The height of the block"},
                {RPCResult::Type::NUM, "ins", /*optional=*/true, "The number of inputs (excluding coinbase)"},
                {RPCResult::Type::NUM, "maxfee", /*optional=*/true, "Maximum fee in the block"},
                {RPCResult::Type::NUM, "maxfeerate", /*optional=*/true, "Maximum feerate (in Granums per virtual byte)"},
                {RPCResult::Type::NUM, "maxtxsize", /*optional=*/true, "Maximum transaction size"},
                {RPCResult::Type::NUM, "medianfee", /*optional=*/true, "Truncated median fee in the block"},
                {RPCResult::Type::NUM, "mediantime", /*optional=*/true, "The block median time past"},
                {RPCResult::Type::NUM, "mediantxsize", /*optional=*/true, "Truncated median transaction size"},
                {RPCResult::Type::NUM, "minfee", /*optional=*/true, "Minimum fee in the block"},
                {RPCResult::Type::NUM, "minfeerate", /*optional=*/true, "Minimum feerate (in Granums per virtual byte)"},
                {RPCResult::Type::NUM, "mintxsize", /*optional=*/true, "Minimum transaction size"},
                {RPCResult::Type::NUM, "outs", /*optional=*/true, "The number of outputs"},
                {RPCResult::Type::NUM, "subsidy", /*optional=*/true, "The block subsidy"},
                {RPCResult::Type::NUM, "swtotal_size", /*optional=*/true, "Total size of all segwit transactions"},
                {RPCResult::Type::NUM, "swtotal_weight", /*optional=*/true, "Total weight of all segwit transactions"},
                {RPCResult::Type::NUM, "swtxs", /*optional=*/true, "The number of segwit transactions"},
                {RPCResult::Type::NUM, "time", /*optional=*/true, "The block time"},
                {RPCResult::Type::NUM, "total_out", /*optional=*/true, "Total amount in all outputs (excluding coinbase and thus reward [ie subsidy + totalfee])"},
                {RPCResult::Type::NUM, "total_size", /*optional=*/true, "Total size of all non-coinbase transactions"},
                {RPCResult::Type::NUM, "total_weight", /*optional=*/true, "Total weight of all non-coinbase transactions"},
                {RPCResult::Type::NUM, "totalfee", /*optional=*/true, "The fee total"},
                {RPCResult::Type::NUM, "txs", /*optional=*/true, "The number of transactions (including coinbase)"},
                {RPCResult::Type::NUM, "utxo_increase", /*optional=*/true, "The increase/decrease in the number of unspent outputs (not discounting op_return and similar)"},
                {RPCResult::Type::NUM, "utxo_size_inc", /*optional=*/true, "The increase/decrease in size for the utxo index (not discounting op_return and similar)"},
                {RPCResult::Type::NUM, "utxo_increase_actual", /*optional=*/true, "The increase/decrease in the number of unspent outputs, not counting unspendables"},
                {RPCResult::Type::NUM, "utxo_size_inc_actual", /*optional=*/true, "The increase/decrease in size for the utxo index, not counting unspendables"},
            }},
                RPCExamples{
                    HelpExampleCli("getblockstats", R"('"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09"' '["minfeerate","avgfeerate"]')") +
                    HelpExampleCli("getblockstats", R"(1000 '["minfeerate","avgfeerate"]')") +
                    HelpExampleRpc("getblockstats", R"("00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09", ["minfeerate","avgfeerate"])") +
                    HelpExampleRpc("getblockstats", R"(1000, ["minfeerate","avgfeerate"])")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    ChainstateManager& chainman = EnsureAnyChainman(request.context);
    const CBlockIndex& pindex{*CHECK_NONFATAL(ParseHashOrHeight(request.params[0], chainman))};

    std::set<std::string> stats;
    if (!request.params[1].isNull()) {
        const UniValue stats_univalue = request.params[1].get_array();
        for (unsigned int i = 0; i < stats_univalue.size(); i++) {
            const std::string stat = stats_univalue[i].get_str();
            stats.insert(stat);
        }
    }

    const CBlock& block = GetBlockChecked(chainman.m_blockman, &pindex);
    const CBlockUndo& blockUndo = GetUndoChecked(chainman.m_blockman, &pindex);

    const bool do_all = stats.size() == 0; // Calculate everything if nothing selected (default)
    const bool do_mediantxsize = do_all || stats.count("mediantxsize") != 0;
    const bool do_medianfee = do_all || stats.count("medianfee") != 0;
    const bool do_feerate_percentiles = do_all || stats.count("feerate_percentiles") != 0;
    const bool loop_inputs = do_all || do_medianfee || do_feerate_percentiles ||
        SetHasKeys(stats, "utxo_increase", "utxo_increase_actual", "utxo_size_inc", "utxo_size_inc_actual", "totalfee", "avgfee", "avgfeerate", "minfee", "maxfee", "minfeerate", "maxfeerate");
    const bool loop_outputs = do_all || loop_inputs || stats.count("total_out");
    const bool do_calculate_size = do_mediantxsize ||
        SetHasKeys(stats, "total_size", "avgtxsize", "mintxsize", "maxtxsize", "swtotal_size");
    const bool do_calculate_weight = do_all || SetHasKeys(stats, "total_weight", "avgfeerate", "swtotal_weight", "avgfeerate", "feerate_percentiles", "minfeerate", "maxfeerate");
    const bool do_calculate_sw = do_all || SetHasKeys(stats, "swtxs", "swtotal_size", "swtotal_weight");

    CAmount maxfee = 0;
    CAmount maxfeerate = 0;
    CAmount minfee = MAX_MONEY;
    CAmount minfeerate = MAX_MONEY;
    CAmount total_out = 0;
    CAmount totalfee = 0;
    int64_t inputs = 0;
    int64_t maxtxsize = 0;
    int64_t mintxsize = MAX_BLOCK_SERIALIZED_SIZE;
    int64_t outputs = 0;
    int64_t swtotal_size = 0;
    int64_t swtotal_weight = 0;
    int64_t swtxs = 0;
    int64_t total_size = 0;
    int64_t total_weight = 0;
    int64_t utxos = 0;
    int64_t utxo_size_inc = 0;
    int64_t utxo_size_inc_actual = 0;
    std::vector<CAmount> fee_array;
    std::vector<std::pair<CAmount, int64_t>> feerate_array;
    std::vector<int64_t> txsize_array;

    for (size_t i = 0; i < block.vtx.size(); ++i) {
        const auto& tx = block.vtx.at(i);
        outputs += tx->vout.size();

        CAmount tx_total_out = 0;
        if (loop_outputs) {
            for (const CTxOut& out : tx->vout) {
                tx_total_out += out.nValue;

                size_t out_size = GetSerializeSize(out, PROTOCOL_VERSION) + PER_UTXO_OVERHEAD;
                utxo_size_inc += out_size;

                // The Genesis block and the repeated BIP30 block coinbases don't change the UTXO
                // set counts, so they have to be excluded from the statistics
                if (pindex.nHeight == 0) continue;
                // Skip unspendable outputs since they are not included in the UTXO set
                if (out.scriptPubKey.IsUnspendable()) continue;

                ++utxos;
                utxo_size_inc_actual += out_size;
            }
        }

        if (tx->IsCoinBase()) {
            continue;
        }

        inputs += tx->vin.size(); // Don't count coinbase's fake input
        total_out += tx_total_out; // Don't count coinbase reward

        int64_t tx_size = 0;
        if (do_calculate_size) {

            tx_size = tx->GetTotalSize();
            if (do_mediantxsize) {
                txsize_array.push_back(tx_size);
            }
            maxtxsize = std::max(maxtxsize, tx_size);
            mintxsize = std::min(mintxsize, tx_size);
            total_size += tx_size;
        }

        int64_t weight = 0;
        if (do_calculate_weight) {
            weight = GetTransactionWeight(*tx);
            total_weight += weight;
        }

        if (do_calculate_sw && tx->HasWitness()) {
            ++swtxs;
            swtotal_size += tx_size;
            swtotal_weight += weight;
        }

        if (loop_inputs) {
            CAmount tx_total_in = 0;
            const auto& txundo = blockUndo.vtxundo.at(i - 1);
            for (const Coin& coin: txundo.vprevout) {
                const CTxOut& prevoutput = coin.out;

                tx_total_in += prevoutput.nValue;
                size_t prevout_size = GetSerializeSize(prevoutput, PROTOCOL_VERSION) + PER_UTXO_OVERHEAD;
                utxo_size_inc -= prevout_size;
                utxo_size_inc_actual -= prevout_size;
            }

            CAmount txfee = tx_total_in - tx_total_out;
            CHECK_NONFATAL(MoneyRange(txfee));
            if (do_medianfee) {
                fee_array.push_back(txfee);
            }
            maxfee = std::max(maxfee, txfee);
            minfee = std::min(minfee, txfee);
            totalfee += txfee;

            // New feerate uses Granums per virtual byte instead of per serialized byte
            CAmount feerate = weight ? (txfee * WITNESS_SCALE_FACTOR) / weight : 0;
            if (do_feerate_percentiles) {
                feerate_array.emplace_back(std::make_pair(feerate, weight));
            }
            maxfeerate = std::max(maxfeerate, feerate);
            minfeerate = std::min(minfeerate, feerate);
        }
    }

    CAmount feerate_percentiles[NUM_GETBLOCKSTATS_PERCENTILES] = { 0 };
    CalculatePercentilesByWeight(feerate_percentiles, feerate_array, total_weight);

    UniValue feerates_res(UniValue::VARR);
    for (int64_t i = 0; i < NUM_GETBLOCKSTATS_PERCENTILES; i++) {
        feerates_res.push_back(feerate_percentiles[i]);
    }

    UniValue ret_all(UniValue::VOBJ);
    ret_all.pushKV("avgfee", (block.vtx.size() > 1) ? totalfee / (block.vtx.size() - 1) : 0);
    ret_all.pushKV("avgfeerate", total_weight ? (totalfee * WITNESS_SCALE_FACTOR) / total_weight : 0); // Unit: sat/vbyte
    ret_all.pushKV("avgtxsize", (block.vtx.size() > 1) ? total_size / (block.vtx.size() - 1) : 0);
    ret_all.pushKV("blockhash", pindex.GetBlockHash().GetHex());
    ret_all.pushKV("feerate_percentiles", feerates_res);
    ret_all.pushKV("height", (int64_t)pindex.nHeight);
    ret_all.pushKV("ins", inputs);
    ret_all.pushKV("maxfee", maxfee);
    ret_all.pushKV("maxfeerate", maxfeerate);
    ret_all.pushKV("maxtxsize", maxtxsize);
    ret_all.pushKV("medianfee", CalculateTruncatedMedian(fee_array));
    ret_all.pushKV("mediantime", pindex.GetMedianTimePast());
    ret_all.pushKV("mediantxsize", CalculateTruncatedMedian(txsize_array));
    ret_all.pushKV("minfee", (minfee == MAX_MONEY) ? 0 : minfee);
    ret_all.pushKV("minfeerate", (minfeerate == MAX_MONEY) ? 0 : minfeerate);
    ret_all.pushKV("mintxsize", mintxsize == MAX_BLOCK_SERIALIZED_SIZE ? 0 : mintxsize);
    ret_all.pushKV("outs", outputs);
    ret_all.pushKV("subsidy", GetBlockSubsidy(pindex.nHeight, chainman.GetParams().GetConsensus()));
    ret_all.pushKV("swtotal_size", swtotal_size);
    ret_all.pushKV("swtotal_weight", swtotal_weight);
    ret_all.pushKV("swtxs", swtxs);
    ret_all.pushKV("time", pindex.GetBlockTime());
    ret_all.pushKV("total_out", total_out);
    ret_all.pushKV("total_size", total_size);
    ret_all.pushKV("total_weight", total_weight);
    ret_all.pushKV("totalfee", totalfee);
    ret_all.pushKV("txs", (int64_t)block.vtx.size());
    ret_all.pushKV("utxo_increase", outputs - inputs);
    ret_all.pushKV("utxo_size_inc", utxo_size_inc);
    ret_all.pushKV("utxo_increase_actual", utxos - inputs);
    ret_all.pushKV("utxo_size_inc_actual", utxo_size_inc_actual);

    if (do_all) {
        return ret_all;
    }

    UniValue ret(UniValue::VOBJ);
    for (const std::string& stat : stats) {
        const UniValue& value = ret_all[stat];
        if (value.isNull()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid selected statistic '%s'", stat));
        }
        ret.pushKV(stat, value);
    }
    return ret;
},
    };
}

namespace {
//! Search for a given set of pubkey scripts
bool FindScriptPubKey(std::atomic<int>& scan_progress, const std::atomic<bool>& should_abort, int64_t& count, CCoinsViewCursor* cursor, const std::set<CScript>& needles, std::map<COutPoint, Coin>& out_results, std::function<void()>& interruption_point)
{
    scan_progress = 0;
    count = 0;
    while (cursor->Valid()) {
        COutPoint key;
        Coin coin;
        if (!cursor->GetKey(key) || !cursor->GetValue(coin)) return false;
        if (++count % 8192 == 0) {
            interruption_point();
            if (should_abort) {
                // allow to abort the scan via the abort reference
                return false;
            }
        }
        if (count % 256 == 0) {
            // update progress reference every 256 item
            uint32_t high = 0x100 * *key.hash.begin() + *(key.hash.begin() + 1);
            scan_progress = (int)(high * 100.0 / 65536.0 + 0.5);
        }
        if (needles.count(coin.out.scriptPubKey)) {
            out_results.emplace(key, coin);
        }
        cursor->Next();
    }
    scan_progress = 100;
    return true;
}
} // namespace

/** RAII object to prevent concurrency issue when scanning the txout set */
static std::atomic<int> g_scan_progress;
static std::atomic<bool> g_scan_in_progress;
static std::atomic<bool> g_should_abort_scan;
class CoinsViewScanReserver
{
private:
    bool m_could_reserve{false};
public:
    explicit CoinsViewScanReserver() = default;

    bool reserve() {
        CHECK_NONFATAL(!m_could_reserve);
        if (g_scan_in_progress.exchange(true)) {
            return false;
        }
        CHECK_NONFATAL(g_scan_progress == 0);
        m_could_reserve = true;
        return true;
    }

    ~CoinsViewScanReserver() {
        if (m_could_reserve) {
            g_scan_in_progress = false;
            g_scan_progress = 0;
        }
    }
};

static const auto scan_action_arg_desc = RPCArg{
    "action", RPCArg::Type::STR, RPCArg::Optional::NO, "The action to execute\n"
        "\"start\" for starting a scan\n"
        "\"abort\" for aborting the current scan (returns true when abort was successful)\n"
        "\"status\" for progress report (in %) of the current scan"
};

static const auto scan_objects_arg_desc = RPCArg{
    "scanobjects", RPCArg::Type::ARR, RPCArg::Optional::OMITTED, "Array of scan objects. Required for \"start\" action\n"
        "Every scan object is either a string descriptor or an object:",
    {
        {"descriptor", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "An output descriptor"},
        {"", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED, "An object with output descriptor and metadata",
            {
                {"desc", RPCArg::Type::STR, RPCArg::Optional::NO, "An output descriptor"},
                {"range", RPCArg::Type::RANGE, RPCArg::Default{1000}, "The range of HD chain indexes to explore (either end or [begin,end])"},
            }},
    },
    RPCArgOptions{.oneline_description="[scanobjects,...]"},
};

static const auto scan_result_abort = RPCResult{
    "when action=='abort'", RPCResult::Type::BOOL, "success",
    "True if scan will be aborted (not necessarily before this RPC returns), or false if there is no scan to abort"
};
static const auto scan_result_status_none = RPCResult{
    "when action=='status' and no scan is in progress - possibly already completed", RPCResult::Type::NONE, "", ""
};
static const auto scan_result_status_some = RPCResult{
    "when action=='status' and a scan is currently in progress", RPCResult::Type::OBJ, "", "",
    {{RPCResult::Type::NUM, "progress", "Approximate percent complete"},}
};


static RPCHelpMan scantxoutset()
{
    // RFC 8032 Ed25519 test vector 1, ORUM-wrapped. Hits the UTXO set (unlike RIGOR_MARKER_RETURN).
    const std::string wpkh_example = "wpkh(00d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a)";
    const std::string wpkh_cs = GetDescriptorChecksum(wpkh_example);
    const std::string EXAMPLE_DESCRIPTOR = wpkh_cs.empty() ? wpkh_example : wpkh_example + "#" + wpkh_cs;

    return RPCHelpMan{"scantxoutset",
        "\nScans the unspent transaction output set for entries that match certain output descriptors.\n"
        "Examples of output descriptors are:\n"
        "    addr(<address>)                      Outputs whose scriptPubKey corresponds to the specified address\n"
        "    wpkh(<pubkey>)                       P2WPKH (ORU) outputs for the given pubkey\n"
        "    res(<ed25519_key>)                   Witness v1 native RES (RigorChain; 32-byte pubkey or 64-byte secret hex)\n"
        "    raw(<hex script>)                    CONVERGE, RES_MINT, or RIGOR_MARKER_RETURN whose scriptPubKey equals the hex\n"
        "                                         (wpkh/res locks must use addr(), wpkh(), or res(); RIGOR_MARKER_RETURN is not in the UTXO set)\n"
        "\nIn the above, <pubkey> either refers to a fixed public key in hexadecimal notation, or to an xpub/xprv optionally followed by one\n"
        "or more path elements separated by \"/\". Ed25519 BIP32 is hardened-only (SLIP-0010);\n"
        "paths must use hardened steps and ranged descriptors must end in \"/*'\" or \"/*h\". Unhardened \"/*\" is rejected.\n"
        "In the latter case, a range needs to be specified by below if different from 1000.\n"
        "For more information on output descriptors, see the documentation in the doc/descriptors.md file.\n",
        {
            scan_action_arg_desc,
            scan_objects_arg_desc,
        },
        {
            RPCResult{"when action=='start'; only returns after scan completes", RPCResult::Type::OBJ, "", "", {
                {RPCResult::Type::BOOL, "success", "Whether the scan was completed"},
                {RPCResult::Type::NUM, "txouts", "The number of unspent transaction outputs scanned"},
                {RPCResult::Type::NUM, "height", "The current block height (index)"},
                {RPCResult::Type::STR_HEX, "bestblock", "The hash of the block at the tip of the chain"},
                {RPCResult::Type::ARR, "unspents", "",
                {
                    {RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR_HEX, "txid", "The transaction id"},
                        {RPCResult::Type::NUM, "vout", "The vout value"},
                        {RPCResult::Type::STR_HEX, "scriptPubKey", "The script key"},
                        {RPCResult::Type::STR, "desc", "A specialized descriptor for the matched scriptPubKey"},
                        {RPCResult::Type::STR_AMOUNT, "amount", "The total amount in " + CURRENCY_UNIT + " of the unspent output"},
                        {RPCResult::Type::BOOL, "coinbase", "Whether this is a coinbase output"},
                        {RPCResult::Type::BOOL, "res_mint", "True if this UTXO is a body RES mint output (immature until nCoinbaseMaturity)"},
                        {RPCResult::Type::NUM, "height", "Height of the unspent transaction output"},
                    }},
                }},
                {RPCResult::Type::STR_AMOUNT, "total_amount", "The total amount of all found unspent outputs in " + CURRENCY_UNIT},
            }},
            scan_result_abort,
            scan_result_status_some,
            scan_result_status_none,
        },
        RPCExamples{
            HelpExampleCli("scantxoutset", "start \'[\"" + EXAMPLE_DESCRIPTOR + "\"]\'") +
            HelpExampleCli("scantxoutset", "status") +
            HelpExampleCli("scantxoutset", "abort") +
            HelpExampleRpc("scantxoutset", "\"start\", [\"" + EXAMPLE_DESCRIPTOR + "\"]") +
            HelpExampleRpc("scantxoutset", "\"status\"") +
            HelpExampleRpc("scantxoutset", "\"abort\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    UniValue result(UniValue::VOBJ);
    if (request.params[0].get_str() == "status") {
        CoinsViewScanReserver reserver;
        if (reserver.reserve()) {
            // no scan in progress
            return UniValue::VNULL;
        }
        result.pushKV("progress", g_scan_progress.load());
        return result;
    } else if (request.params[0].get_str() == "abort") {
        CoinsViewScanReserver reserver;
        if (reserver.reserve()) {
            // reserve was possible which means no scan was running
            return false;
        }
        // set the abort flag
        g_should_abort_scan = true;
        return true;
    } else if (request.params[0].get_str() == "start") {
        CoinsViewScanReserver reserver;
        if (!reserver.reserve()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Scan already in progress, use action \"abort\" or \"status\"");
        }

        if (request.params.size() < 2) {
            throw JSONRPCError(RPC_MISC_ERROR, "scanobjects argument is required for the start action");
        }

        std::set<CScript> needles;
        std::map<CScript, std::string> descriptors;
        CAmount total_in = 0;

        // loop through the scan objects
        for (const UniValue& scanobject : request.params[1].get_array().getValues()) {
            FlatSigningProvider provider;
            auto scripts = EvalDescriptorStringOrObject(scanobject, provider);
            for (CScript& script : scripts) {
                std::string inferred;
                if (auto desc = InferDescriptor(script, provider)) {
                    inferred = desc->ToString();
                }
                needles.emplace(script);
                descriptors.emplace(std::move(script), std::move(inferred));
            }
        }

        // Scan the unspent transaction output set for inputs
        UniValue unspents(UniValue::VARR);
        std::vector<CTxOut> input_txos;
        std::map<COutPoint, Coin> coins;
        g_should_abort_scan = false;
        int64_t count = 0;
        std::unique_ptr<CCoinsViewCursor> pcursor;
        const CBlockIndex* tip;
        NodeContext& node = EnsureAnyNodeContext(request.context);
        {
            ChainstateManager& chainman = EnsureChainman(node);
            LOCK(cs_main);
            Chainstate& active_chainstate = chainman.ActiveChainstate();
            active_chainstate.ForceFlushStateToDisk();
            pcursor = CHECK_NONFATAL(active_chainstate.CoinsDB().Cursor());
            tip = CHECK_NONFATAL(active_chainstate.m_chain.Tip());
        }
        bool res = FindScriptPubKey(g_scan_progress, g_should_abort_scan, count, pcursor.get(), needles, coins, node.rpc_interruption_point);
        result.pushKV("success", res);
        result.pushKV("txouts", count);
        result.pushKV("height", tip->nHeight);
        result.pushKV("bestblock", tip->GetBlockHash().GetHex());

        for (const auto& it : coins) {
            const COutPoint& outpoint = it.first;
            const Coin& coin = it.second;
            const CTxOut& txo = coin.out;
            input_txos.push_back(txo);
            total_in += txo.nValue;

            UniValue unspent(UniValue::VOBJ);
            unspent.pushKV("txid", outpoint.hash.GetHex());
            unspent.pushKV("vout", (int32_t)outpoint.n);
            unspent.pushKV("scriptPubKey", HexStr(txo.scriptPubKey));
            unspent.pushKV("desc", descriptors[txo.scriptPubKey]);
            unspent.pushKV("amount", ValueFromAmount(txo.nValue));
            unspent.pushKV("coinbase", coin.IsCoinBase());
            unspent.pushKV("res_mint", IsResMintScript(txo.scriptPubKey));
            unspent.pushKV("height", (int32_t)coin.nHeight);

            unspents.push_back(unspent);
        }
        result.pushKV("unspents", unspents);
        result.pushKV("total_amount", ValueFromAmount(total_in));
    } else {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid action '%s'", request.params[0].get_str()));
    }
    return result;
},
    };
}

/** RAII object to prevent concurrency issue when scanning blockfilters */
static std::atomic<int> g_scanfilter_progress;
static std::atomic<int> g_scanfilter_progress_height;
static std::atomic<bool> g_scanfilter_in_progress;
static std::atomic<bool> g_scanfilter_should_abort_scan;
class BlockFiltersScanReserver
{
private:
    bool m_could_reserve{false};
public:
    explicit BlockFiltersScanReserver() = default;

    bool reserve() {
        CHECK_NONFATAL(!m_could_reserve);
        if (g_scanfilter_in_progress.exchange(true)) {
            return false;
        }
        m_could_reserve = true;
        return true;
    }

    ~BlockFiltersScanReserver() {
        if (m_could_reserve) {
            g_scanfilter_in_progress = false;
        }
    }
};

static bool CheckBlockFilterMatches(BlockManager& blockman, const CBlockIndex& blockindex, const GCSFilter::ElementSet& needles)
{
    const CBlock block{GetBlockChecked(blockman, &blockindex)};
    const CBlockUndo block_undo{GetUndoChecked(blockman, &blockindex)};

    // Check if any of the outputs match the scriptPubKey
    for (const auto& tx : block.vtx) {
        if (std::any_of(tx->vout.cbegin(), tx->vout.cend(), [&](const auto& txout) {
                return needles.count(std::vector<unsigned char>(txout.scriptPubKey.begin(), txout.scriptPubKey.end())) != 0;
            })) {
            return true;
        }
    }
    // Check if any of the inputs match the scriptPubKey
    for (const auto& txundo : block_undo.vtxundo) {
        if (std::any_of(txundo.vprevout.cbegin(), txundo.vprevout.cend(), [&](const auto& coin) {
                return needles.count(std::vector<unsigned char>(coin.out.scriptPubKey.begin(), coin.out.scriptPubKey.end())) != 0;
            })) {
            return true;
        }
    }

    return false;
}

static RPCHelpMan scanblocks()
{
    return RPCHelpMan{"scanblocks",
        "\nReturn relevant blockhashes for given descriptors (requires blockfilterindex).\n"
        "This call may take several minutes. Make sure to use no RPC timeout (rigor-cli -rpcclienttimeout=0)",
        {
            scan_action_arg_desc,
            scan_objects_arg_desc,
            RPCArg{"start_height", RPCArg::Type::NUM, RPCArg::Default{0}, "Height to start to scan from"},
            RPCArg{"stop_height", RPCArg::Type::NUM, RPCArg::DefaultHint{"chain tip"}, "Height to stop to scan"},
            RPCArg{"filtertype", RPCArg::Type::STR, RPCArg::Default{BlockFilterTypeName(BlockFilterType::BASIC)}, "The type name of the filter"},
            RPCArg{"options", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED, "",
                {
                    {"filter_false_positives", RPCArg::Type::BOOL, RPCArg::Default{false}, "Filter false positives (slower and may fail on pruned nodes). Otherwise they may occur at a rate of 1/M"},
                },
                RPCArgOptions{.oneline_description="\"options\""}},
        },
        {
            scan_result_status_none,
            RPCResult{"When action=='start'; only returns after scan completes", RPCResult::Type::OBJ, "", "", {
                {RPCResult::Type::NUM, "from_height", "The height we started the scan from"},
                {RPCResult::Type::NUM, "to_height", "The height we ended the scan at"},
                {RPCResult::Type::ARR, "relevant_blocks", "Blocks that may have matched a scanobject.", {
                    {RPCResult::Type::STR_HEX, "blockhash", "A relevant blockhash"},
                }},
            }},
            RPCResult{"when action=='status' and a scan is currently in progress", RPCResult::Type::OBJ, "", "", {
                    {RPCResult::Type::NUM, "progress", "Approximate percent complete"},
                    {RPCResult::Type::NUM, "current_height", "Height of the block currently being scanned"},
                },
            },
            scan_result_abort,
        },
        RPCExamples{
            HelpExampleCli("scanblocks", "start '[\"addr(rgrt1q4u4nsgk6ug0sqz7r3rj9tykjxrsl0yy4fyxuwc)\"]' 300000") +
            HelpExampleCli("scanblocks", "start '[\"addr(rgrt1q4u4nsgk6ug0sqz7r3rj9tykjxrsl0yy4fyxuwc)\"]' 100 150 basic") +
            HelpExampleCli("scanblocks", "status") +
            HelpExampleRpc("scanblocks", "\"start\", [\"addr(rgrt1q4u4nsgk6ug0sqz7r3rj9tykjxrsl0yy4fyxuwc)\"], 300000") +
            HelpExampleRpc("scanblocks", "\"start\", [\"addr(rgrt1q4u4nsgk6ug0sqz7r3rj9tykjxrsl0yy4fyxuwc)\"], 100, 150, \"basic\"") +
            HelpExampleRpc("scanblocks", "\"status\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    UniValue ret(UniValue::VOBJ);
    if (request.params[0].get_str() == "status") {
        BlockFiltersScanReserver reserver;
        if (reserver.reserve()) {
            // no scan in progress
            return NullUniValue;
        }
        ret.pushKV("progress", g_scanfilter_progress.load());
        ret.pushKV("current_height", g_scanfilter_progress_height.load());
        return ret;
    } else if (request.params[0].get_str() == "abort") {
        BlockFiltersScanReserver reserver;
        if (reserver.reserve()) {
            // reserve was possible which means no scan was running
            return false;
        }
        // set the abort flag
        g_scanfilter_should_abort_scan = true;
        return true;
    }
    else if (request.params[0].get_str() == "start") {
        BlockFiltersScanReserver reserver;
        if (!reserver.reserve()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Scan already in progress, use action \"abort\" or \"status\"");
        }
        const std::string filtertype_name{request.params[4].isNull() ? "basic" : request.params[4].get_str()};

        BlockFilterType filtertype;
        if (!BlockFilterTypeByName(filtertype_name, filtertype)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown filtertype");
        }

        UniValue options{request.params[5].isNull() ? UniValue::VOBJ : request.params[5]};
        bool filter_false_positives{options.exists("filter_false_positives") ? options["filter_false_positives"].get_bool() : false};

        BlockFilterIndex* index = GetBlockFilterIndex(filtertype);
        if (!index) {
            throw JSONRPCError(RPC_MISC_ERROR, "Index is not enabled for filtertype " + filtertype_name);
        }

        NodeContext& node = EnsureAnyNodeContext(request.context);
        ChainstateManager& chainman = EnsureChainman(node);

        // set the start-height
        const CBlockIndex* block = nullptr;
        const CBlockIndex* stop_block = nullptr;
        {
            LOCK(cs_main);
            CChain& active_chain = chainman.ActiveChain();
            block = active_chain.Genesis();
            stop_block = active_chain.Tip();
            if (!request.params[2].isNull()) {
                block = active_chain[request.params[2].getInt<int>()];
                if (!block) {
                    throw JSONRPCError(RPC_MISC_ERROR, "Invalid start_height");
                }
            }
            if (!request.params[3].isNull()) {
                stop_block = active_chain[request.params[3].getInt<int>()];
                if (!stop_block || stop_block->nHeight < block->nHeight) {
                    throw JSONRPCError(RPC_MISC_ERROR, "Invalid stop_height");
                }
            }
        }
        CHECK_NONFATAL(block);

        // loop through the scan objects, add scripts to the needle_set
        GCSFilter::ElementSet needle_set;
        for (const UniValue& scanobject : request.params[1].get_array().getValues()) {
            FlatSigningProvider provider;
            std::vector<CScript> scripts = EvalDescriptorStringOrObject(scanobject, provider);
            for (const CScript& script : scripts) {
                needle_set.emplace(script.begin(), script.end());
            }
        }
        UniValue blocks(UniValue::VARR);
        const int amount_per_chunk = 10000;
        const CBlockIndex* start_index = block; // for remembering the start of a blockfilter range
        std::vector<BlockFilter> filters;
        const CBlockIndex* start_block = block; // for progress reporting
        const int total_blocks_to_process = stop_block->nHeight - start_block->nHeight;

        g_scanfilter_should_abort_scan = false;
        g_scanfilter_progress = 0;
        g_scanfilter_progress_height = start_block->nHeight;

        while (block) {
            node.rpc_interruption_point(); // allow a clean shutdown
            if (g_scanfilter_should_abort_scan) {
                LogPrintf("scanblocks RPC aborted at height %d.\n", block->nHeight);
                break;
            }
            const CBlockIndex* next = nullptr;
            {
                LOCK(cs_main);
                CChain& active_chain = chainman.ActiveChain();
                next = active_chain.Next(block);
                if (block == stop_block) next = nullptr;
            }
            if (start_index->nHeight + amount_per_chunk == block->nHeight || next == nullptr) {
                LogPrint(BCLog::RPC, "Fetching blockfilters from height %d to height %d.\n", start_index->nHeight, block->nHeight);
                if (index->LookupFilterRange(start_index->nHeight, block, filters)) {
                    for (const BlockFilter& filter : filters) {
                        // compare the elements-set with each filter
                        if (filter.GetFilter().MatchAny(needle_set)) {
                            if (filter_false_positives) {
                                // Double check the filter matches by scanning the block
                                const CBlockIndex& blockindex = *CHECK_NONFATAL(WITH_LOCK(cs_main, return chainman.m_blockman.LookupBlockIndex(filter.GetBlockHash())));

                                if (!CheckBlockFilterMatches(chainman.m_blockman, blockindex, needle_set)) {
                                    continue;
                                }
                            }

                            blocks.push_back(filter.GetBlockHash().GetHex());
                            LogPrint(BCLog::RPC, "scanblocks: found match in %s\n", filter.GetBlockHash().GetHex());
                        }
                    }
                }
                start_index = block;

                // update progress
                int blocks_processed = block->nHeight - start_block->nHeight;
                if (total_blocks_to_process > 0) { // avoid division by zero
                    g_scanfilter_progress = (int)(100.0 / total_blocks_to_process * blocks_processed);
                } else {
                    g_scanfilter_progress = 100;
                }
                g_scanfilter_progress_height = block->nHeight;
            }
            block = next;
        }
        ret.pushKV("from_height", start_block->nHeight);
        ret.pushKV("to_height", g_scanfilter_progress_height.load());
        ret.pushKV("relevant_blocks", blocks);
    }
    else {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid action '%s'", request.params[0].get_str()));
    }
    return ret;
},
    };
}

static RPCHelpMan getblockfilter()
{
    return RPCHelpMan{"getblockfilter",
                "\nRetrieve a BIP 157 content filter for a particular block.\n",
                {
                    {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The hash of the block"},
                    {"filtertype", RPCArg::Type::STR, RPCArg::Default{BlockFilterTypeName(BlockFilterType::BASIC)}, "The type name of the filter"},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR_HEX, "filter", "the hex-encoded filter data"},
                        {RPCResult::Type::STR_HEX, "header", "the hex-encoded filter header"},
                    }},
                RPCExamples{
                    HelpExampleCli("getblockfilter", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\" \"basic\"") +
                    HelpExampleRpc("getblockfilter", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\", \"basic\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    uint256 block_hash = ParseHashV(request.params[0], "blockhash");
    std::string filtertype_name = BlockFilterTypeName(BlockFilterType::BASIC);
    if (!request.params[1].isNull()) {
        filtertype_name = request.params[1].get_str();
    }

    BlockFilterType filtertype;
    if (!BlockFilterTypeByName(filtertype_name, filtertype)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown filtertype");
    }

    BlockFilterIndex* index = GetBlockFilterIndex(filtertype);
    if (!index) {
        throw JSONRPCError(RPC_MISC_ERROR, "Index is not enabled for filtertype " + filtertype_name);
    }

    const CBlockIndex* block_index;
    bool block_was_connected;
    {
        ChainstateManager& chainman = EnsureAnyChainman(request.context);
        LOCK(cs_main);
        block_index = chainman.m_blockman.LookupBlockIndex(block_hash);
        if (!block_index) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }
        block_was_connected = block_index->IsValid(BLOCK_VALID_SCRIPTS);
    }

    bool index_ready = index->BlockUntilSyncedToCurrentChain();

    BlockFilter filter;
    uint256 filter_header;
    if (!index->LookupFilter(block_index, filter) ||
        !index->LookupFilterHeader(block_index, filter_header)) {
        int err_code;
        std::string errmsg = "Filter not found.";

        if (!block_was_connected) {
            err_code = RPC_INVALID_ADDRESS_OR_KEY;
            errmsg += " Block was not connected to active chain.";
        } else if (!index_ready) {
            err_code = RPC_MISC_ERROR;
            errmsg += " Block filters are still in the process of being indexed.";
        } else {
            err_code = RPC_INTERNAL_ERROR;
            errmsg += " This error is unexpected and indicates index corruption.";
        }

        throw JSONRPCError(err_code, errmsg);
    }

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("filter", HexStr(filter.GetEncodedFilter()));
    ret.pushKV("header", filter_header.GetHex());
    return ret;
},
    };
}

/**
 * Serialize the UTXO set to a file for loading elsewhere.
 *
 * @see SnapshotMetadata
 */
static RPCHelpMan dumptxoutset()
{
    return RPCHelpMan{
        "dumptxoutset",
        "Write the serialized UTXO set to disk.",
        {
            {"path", RPCArg::Type::STR, RPCArg::Optional::NO, "Path to the output file. If relative, will be prefixed by datadir."},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
                {
                    {RPCResult::Type::NUM, "coins_written", "the number of coins written in the snapshot"},
                    {RPCResult::Type::STR_HEX, "base_hash", "the hash of the base of the snapshot"},
                    {RPCResult::Type::NUM, "base_height", "the height of the base of the snapshot"},
                    {RPCResult::Type::STR, "path", "the absolute path that the snapshot was written to"},
                    {RPCResult::Type::STR_HEX, "txoutset_hash", "the hash of the UTXO set contents"},
                    {RPCResult::Type::NUM, "nchaintx", "the number of transactions in the chain up to and including the base block"},
                }
        },
        RPCExamples{
            HelpExampleCli("dumptxoutset", "utxo.dat")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    const ArgsManager& args{EnsureAnyArgsman(request.context)};
    const fs::path path = fsbridge::AbsPathJoin(args.GetDataDirNet(), fs::u8path(request.params[0].get_str()));
    // Write to a temporary path and then move into `path` on completion
    // to avoid confusion due to an interruption.
    const fs::path temppath = fsbridge::AbsPathJoin(args.GetDataDirNet(), fs::u8path(request.params[0].get_str() + ".incomplete"));

    if (fs::exists(path)) {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER,
            path.u8string() + " already exists. If you are sure this is what you want, "
            "move it out of the way first");
    }

    FILE* file{fsbridge::fopen(temppath, "wb")};
    CAutoFile afile{file, SER_DISK, PROTOCOL_VERSION};
    if (afile.IsNull()) {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER,
            "Couldn't open file " + temppath.u8string() + " for writing.");
    }

    NodeContext& node = EnsureAnyNodeContext(request.context);
    UniValue result = CreateUTXOSnapshot(
        node, node.chainman->ActiveChainstate(), afile, path, temppath);
    fs::rename(temppath, path);

    result.pushKV("path", path.u8string());
    return result;
},
    };
}

static RPCHelpMan getblockconvergence()
{
    return RPCHelpMan{"getblockconvergence",
        "\nReturn the cumulative destroyed ORU (convergence) as of a block.\n"
        "\nThis is the chain's own path-fold over the block index, not a coinbase\n"
        "commitment. The retired CVGE coinbase commitment was never a ConnectBlock\n"
        "input and is no longer produced.\n",
        {
            {"hash_or_height", RPCArg::Type::NUM, RPCArg::Optional::NO, "The block hash or height of the target block",
             RPCArgOptions{
                 .skip_type_check = true,
                 .type_str = {"", "string or numeric"},
             }},
        },
        RPCResult{RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR_HEX, "hash", "Block hash"},
                {RPCResult::Type::NUM, "height", "Block height"},
                {RPCResult::Type::STR_AMOUNT, "cumulative_convergence", "Destroyed ORU in this block and all its ancestors"},
            }},
        RPCExamples{
            HelpExampleCli("getblockconvergence", "\"100\"")
            + HelpExampleRpc("getblockconvergence", "\"100\"")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);
            const CBlockIndex* blockindex;
            {
                LOCK(cs_main);
                if (request.params[0].isStr()) {
                    const std::string in = request.params[0].get_str();
                    const bool all_digits = !in.empty() && std::all_of(in.begin(), in.end(), [](unsigned char c) { return c >= '0' && c <= '9'; });
                    if (all_digits) {
                        UniValue as_num(UniValue::VNUM, in);
                        blockindex = ParseHashOrHeight(as_num, chainman);
                    } else {
                        blockindex = ParseHashOrHeight(request.params[0], chainman);
                    }
                } else {
                    blockindex = ParseHashOrHeight(request.params[0], chainman);
                }
            }
            CHECK_NONFATAL(blockindex);

            // Reads the index only, so this answers on a pruned node too.
            UniValue result(UniValue::VOBJ);
            result.pushKV("hash", blockindex->GetBlockHash().GetHex());
            result.pushKV("height", blockindex->nHeight);
            {
                LOCK(cs_main);
                result.pushKV("cumulative_convergence",
                    ValueFromAmount(static_cast<CAmount>(blockindex->Observational().GetCumulativeConvergence())));
            }
            return result;
        },
    };
}

bool FillTipConvergenceChainSummary(const CBlockIndex& tip, ChainstateManager& chainman, CAmount& cumulative_convergence_out, CAmount& current_convergence_sum_out)
{
    AssertLockHeld(::cs_main);
    // FENCE-W2: RPC/observer projection of the committed cumulative. Not a draw or mint input.
    cumulative_convergence_out = static_cast<CAmount>(tip.Observational().GetCumulativeConvergence());
    if (!tip.last_res_mint_block_hash.IsNull()) {
        const CBlockIndex* pindex_last_res_mint = chainman.ActiveChainstate().m_blockman.LookupBlockIndex(tip.last_res_mint_block_hash);
        if (pindex_last_res_mint == nullptr) return false;
        if (tip.Observational().GetCumulativeConvergence() < pindex_last_res_mint->Observational().GetCumulativeConvergence()) return false;
        current_convergence_sum_out = static_cast<CAmount>(tip.Observational().GetCumulativeConvergence() - pindex_last_res_mint->Observational().GetCumulativeConvergence());
    } else {
        current_convergence_sum_out = static_cast<CAmount>(tip.Observational().GetCumulativeConvergence());
    }
    return true;
}

static RPCHelpMan getconvergencestatus()
{
    return RPCHelpMan{"getconvergencestatus",
        "\nReturn the current continuous RES destruction cost and cumulative issuance.\n"
        "No draw window remains. destroy_cost(h) is a protocol cost, not a valuation.\n",
        {},
        RPCResult{RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::NUM, "tip_height", "Current active tip height"},
                {RPCResult::Type::NUM, "confirm_height", "Height a new mint would confirm at (tip + 1)"},
                {RPCResult::Type::NUM, "destroy_cost", /*optional=*/true, "Current destruction cost at confirm_height; do not feed this back as B"},
                {RPCResult::Type::NUM, "min_atom_burn", /*optional=*/true, "Smallest ORU burn that mints 1 RES atom at confirm_height"},
                {RPCResult::Type::NUM, "min_whole_res_burn", /*optional=*/true, "Smallest ORU burn that mints 1 whole RES at confirm_height"},
                {RPCResult::Type::NUM, "destroy_cost_next", /*optional=*/true, "Destruction cost at confirm_height + 1"},
                {RPCResult::Type::NUM, "next_height", /*optional=*/true, "confirm_height + 1"},
                {RPCResult::Type::BOOL, "issuance_authorized", "Always true for confirm_height >= 0 under RFC-020"},
                {RPCResult::Type::STR_AMOUNT, "res_total", "Cumulative RES minted on the active chain"},
                {RPCResult::Type::STR_AMOUNT, "cumulative_convergence", "Cumulative ORU destroyed on the active chain"},
                {RPCResult::Type::NUM, "last_res_height", "Height of the last RES mint block, or -1 if never minted"},
            }},
        RPCExamples{HelpExampleCli("getconvergencestatus", "")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            NodeContext& node = EnsureAnyNodeContext(request.context);
            ChainstateManager& chainman = EnsureChainman(node);

            const CBlockIndex* tip{nullptr};
            const CBlockIndex* pindex_last_res_mint{nullptr};
            CAmount cumulative_convergence{0};
            {
                LOCK(cs_main);
                tip = chainman.ActiveChain().Tip();
                CHECK_NONFATAL(tip);
                CAmount unused_window{0};
                if (!FillTipConvergenceChainSummary(*tip, chainman, cumulative_convergence, unused_window)) {
                    throw JSONRPCError(RPC_INTERNAL_ERROR, "inconsistent cumulative convergence state");
                }
                if (!tip->last_res_mint_block_hash.IsNull()) {
                    pindex_last_res_mint = chainman.ActiveChainstate().m_blockman.LookupBlockIndex(tip->last_res_mint_block_hash);
                    if (pindex_last_res_mint == nullptr) {
                        throw JSONRPCError(RPC_INTERNAL_ERROR, "inconsistent cumulative convergence state");
                    }
                }
            }

            const int confirm_height = tip->nHeight + 1;
            const bool authorized = Consensus::ResMintAuthorizedAtHeight(confirm_height);

            UniValue result(UniValue::VOBJ);
            result.pushKV("tip_height", tip->nHeight);
            result.pushKV("confirm_height", confirm_height);
            result.pushKV("issuance_authorized", authorized);
            if (authorized) {
                result.pushKV("destroy_cost", Consensus::ResDestroyCostAtHeight(confirm_height));
                result.pushKV("min_atom_burn", Consensus::ResMinMintBurnAtoms(confirm_height));
                result.pushKV("min_whole_res_burn", Consensus::ResMinBurnForAtoms(Consensus::RES_COIN, confirm_height));
                result.pushKV("destroy_cost_next", Consensus::ResDestroyCostAtHeight(confirm_height + 1));
                result.pushKV("next_height", confirm_height + 1);
            }
            result.pushKV("res_total", ValueFromAmountAsset(CachedActiveTipCumulativeResMinted(tip), ASSET_RES));
            result.pushKV("cumulative_convergence", ValueFromAmount(cumulative_convergence));
            result.pushKV("last_res_height", pindex_last_res_mint ? pindex_last_res_mint->nHeight : -1);
            return result;
        },
    };
}

UniValue CreateUTXOSnapshot(
    NodeContext& node,
    Chainstate& chainstate,
    CAutoFile& afile,
    const fs::path& path,
    const fs::path& temppath)
{
    std::unique_ptr<CCoinsViewCursor> pcursor;
    std::optional<CCoinsStats> maybe_stats;
    const CBlockIndex* tip;

    {
        // Hold cs_main through stats, cursor creation, and the full serialize pass
        // so no other thread can mutate the UTXO set between counting coins and
        // writing them (otherwise metadata.m_coins_count can disagree with bytes
        // written, breaking snapshot activation tests and RPC snapshots).
        LOCK(::cs_main);

        chainstate.ForceFlushStateToDisk();

        maybe_stats = GetUTXOStats(&chainstate.CoinsDB(), chainstate.m_blockman, CoinStatsHashType::HASH_SERIALIZED, node.rpc_interruption_point);
        if (!maybe_stats) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Unable to read UTXO set");
        }

        pcursor = chainstate.CoinsDB().Cursor();
        tip = CHECK_NONFATAL(chainstate.m_blockman.LookupBlockIndex(maybe_stats->hashBlock));

        LOG_TIME_SECONDS(strprintf("writing UTXO snapshot at height %s (%s) to file %s (via %s)",
            tip->nHeight, tip->GetBlockHash().ToString(),
            fs::PathToString(path), fs::PathToString(temppath)));

        SnapshotMetadata metadata{tip->GetBlockHash(), maybe_stats->coins_count, tip->nChainTx};

        // Serialize through CDataStream so the on-disk layout matches what ActivateSnapshot
        // parses with CAutoFile (both use SER_DISK + PROTOCOL_VERSION). Chained writes
        // directly to AutoFile/CAutoFile can diverge for some Coin encodings.
        CDataStream blob(SER_DISK, PROTOCOL_VERSION);
        blob << metadata;

        COutPoint key;
        Coin coin;
        unsigned int iter{0};

        while (pcursor->Valid()) {
            if (iter % 5000 == 0) node.rpc_interruption_point();
            ++iter;
            if (pcursor->GetKey(key) && pcursor->GetValue(coin)) {
                blob << key << coin;
            }

            pcursor->Next();
        }

        afile.write({blob.data(), blob.size()});
        afile.fclose();
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("coins_written", maybe_stats->coins_count);
    result.pushKV("base_hash", tip->GetBlockHash().ToString());
    result.pushKV("base_height", tip->nHeight);
    result.pushKV("path", path.u8string());
    result.pushKV("txoutset_hash", maybe_stats->hashSerialized.ToString());
    // Cast required because univalue doesn't have serialization specified for
    // `unsigned int`, nChainTx's type.
    result.pushKV("nchaintx", uint64_t{tip->nChainTx});
    return result;
}

/**
 * RFC-NODE-007: Node-level RPC to query asset balances from chainstate.
 * Returns ORU or RES balance for a given address by scanning UTXO set.
 */
static RPCHelpMan getassetbalance()
{
    return RPCHelpMan{"getassetbalance",
        "\nReturns the balance for a specific asset type (ORU or RES) for a given address.\n",
        {
            {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The Rigor address"},
            {"asset", RPCArg::Type::STR, RPCArg::Optional::NO, "The asset type: \"ORU\" or \"RES\""},
        },
        RPCResults{
            RPCResult{RPCResult::Type::STR_AMOUNT, "balance", "The balance"},
        },
        RPCExamples{
            HelpExampleCli("getassetbalance", "\"rg1...\" \"ORU\"") +
            HelpExampleRpc("getassetbalance", "\"rg1...\", \"ORU\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    NodeContext& node = EnsureAnyNodeContext(request.context);
    ChainstateManager& chainman = EnsureChainman(node);
    LOCK(cs_main);

    std::string address_str = request.params[0].get_str();
    std::string asset_str = request.params[1].get_str();

    // Parse asset type
    AssetType asset_type;
    if (asset_str == "ORU") {
        asset_type = ASSET_ORU;
    } else if (asset_str == "RES") {
        asset_type = ASSET_RES;
    } else {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset type. Must be \"ORU\" or \"RES\".");
    }

    // Decode address to scriptPubKey
    CTxDestination dest = DecodeDestination(address_str);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }
    CScript scriptPubKey = GetScriptForDestination(dest);

    // Query UTXO set using cursor from base view
    Chainstate& active_chainstate = chainman.ActiveChainstate();
    std::unique_ptr<CCoinsViewCursor> pcursor = active_chainstate.CoinsDB().Cursor();
    if (!pcursor) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Cannot read UTXO set");
    }

    CAmount balance = 0;
    while (pcursor->Valid()) {
        COutPoint key;
        Coin coin;
        if (pcursor->GetKey(key) && pcursor->GetValue(coin)) {
            if (coin.out.scriptPubKey == scriptPubKey) {
                AssetType utxo_asset = AssetTypeFromScriptPubKey(coin.out.scriptPubKey);
                if (utxo_asset == asset_type) {
                    balance += coin.out.nValue;
                }
            }
        }
        pcursor->Next();
    }

    return ValueFromAmountAsset(balance, asset_type);
},
    };
}

void RegisterBlockchainRPCCommands(CRPCTable& t)
{
    static const CRPCCommand commands[]{
        {"blockchain", &getblockchaininfo},
        {"blockchain", &getdeploymentinfo},
        {"blockchain", &getchaintxstats},
        {"blockchain", &getblockstats},
        {"blockchain", &getbestblockhash},
        {"blockchain", &getblockcount},
        {"blockchain", &getdifficulty},
        {"blockchain", &getblockhash},
        {"blockchain", &getblockheader},
        {"blockchain", &getblock},
        {"blockchain", &getchaintips},
        {"blockchain", &gettxout},
        {"blockchain", &gettxoutsetinfo},
        {"blockchain", &getdeterministicstatehash},
        {"blockchain", &getruntimesupply},
        {"blockchain", &getruntimesupplyinvariants},
        {"blockchain", &getruntimesupplywitness},
        {"blockchain", &getruntimesupplywitnessbyheight},
        {"blockchain", &getruntimesupplywitnesscatalog},
        {"blockchain", &getphase5runtimealignment},
        {"blockchain", &getruntimeequivalencecandidate},
        {"blockchain", &getruntimeequivalencecandidatebyheight},
        {"blockchain", &exportruntimeequivalencecandidate},
        {"blockchain", &exportruntimeglobalcandidateequivalence},
        {"blockchain", &getruntimeequivalenceclosure},
        {"blockchain", &res_mint_unit_cost},
        {"blockchain", &getphase5summary},
        {"blockchain", &getphase5summaryfinal},
        {"blockchain", &getruntimevectorstatus},
        {"blockchain", &exportruntimesupply},
        {"blockchain", &getchainstatesummary},
        {"blockchain", &diffchainstatesummaries},
        {"blockchain", &getreplaysnapshot},
        {"blockchain", &getassetbalance},
        {"blockchain", &waitfornewblock},
        {"blockchain", &waitforblock},
        {"blockchain", &waitforblockheight},
        {"blockchain", &syncwithvalidationinterfacequeue},
        {"blockchain", &getblockfrompeer},
        {"blockchain", &pruneblockchain},
        {"blockchain", &verifychain},
        {"blockchain", &preciousblock},
        {"blockchain", &invalidateblock},
        {"blockchain", &reconsiderblock},
        {"blockchain", &scantxoutset},
        {"blockchain", &scanblocks},
        {"blockchain", &getblockfilter},
        {"blockchain", &dumptxoutset},
        {"blockchain", &getblockconvergence},
        {"blockchain", &getconvergencestatus},
    };
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}
