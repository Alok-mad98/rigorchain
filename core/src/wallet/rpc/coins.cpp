// Copyright (c) 2011-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <core_io.h>
#include <primitives/transaction.h>
#include <logging.h>
#include <script/conv_sign_trace.h>
#include <script/standard.h>
#include <hash.h>
#include <key_io.h>
#include <rpc/util.h>
#include <util/moneystr.h>
#include <wallet/coincontrol.h>
#include <wallet/receive.h>
#include <wallet/reuse_domain.h>
#include <wallet/rpc/amount_domain.h>
#include <wallet/rpc/util.h>
#include <wallet/spend.h>
#include <wallet/wallet.h>

#include <univalue.h>


namespace wallet {
struct ReceivedTally {
    CAmount oru{0};
    CAmount res{0};
};

static ReceivedTally GetReceivedTally(const CWallet& wallet, const UniValue& params, bool by_label)
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet)
{
    std::vector<CTxDestination> addresses;
    if (by_label) {
        addresses = wallet.ListAddrBookAddresses(CWallet::AddrBookFilter{LabelFromValue(params[0])});
        if (addresses.empty()) throw JSONRPCError(RPC_WALLET_ERROR, "Label not found in wallet");
    } else {
        CTxDestination dest = DecodeDestination(params[0].get_str());
        if (!IsValidDestination(dest)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Bitcoin address");
        }
        addresses.emplace_back(dest);
    }

    std::set<CScript> output_scripts;
    for (const auto& address : addresses) {
        auto output_script{GetScriptForDestination(address)};
        if (wallet.IsMine(output_script)) {
            output_scripts.insert(output_script);
        }
    }

    if (output_scripts.empty()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Address not found in wallet");
    }

    int min_depth = 1;
    if (!params[1].isNull()) {
        min_depth = params[1].getInt<int>();
    }

    const bool include_immature_coinbase{params[2].isNull() ? false : params[2].get_bool()};

    ReceivedTally tally;
    for (const std::pair<const uint256, CWalletTx>& wtx_pair : wallet.mapWallet) {
        const CWalletTx& wtx = wtx_pair.second;
        int depth{wallet.GetTxDepthInMainChain(wtx)};
        if (depth < min_depth
            || (wtx.IsCoinBase() && (depth < 1))
            || (wallet.IsTxImmatureCoinBase(wtx) && !include_immature_coinbase))
        {
            continue;
        }

        for (const CTxOut& txout : wtx.tx->vout) {
            if (output_scripts.count(txout.scriptPubKey) == 0) continue;
            const AssetType asset = AssetTypeFromScriptPubKey(txout.scriptPubKey);
            if (asset == ASSET_RES) {
                tally.res += txout.nValue;
            } else if (asset == ASSET_ORU) {
                tally.oru += txout.nValue;
            }
        }
    }

    return tally;
}


RPCHelpMan getreceivedbyaddress()
{
    return RPCHelpMan{"getreceivedbyaddress",
                "\nReturns the total amount received by the given address in transactions with at least minconf confirmations.\n",
                {
                    {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The bitcoin address for transactions."},
                    {"minconf", RPCArg::Type::NUM, RPCArg::Default{1}, "Only include transactions confirmed at least this many times."},
                    {"include_immature_coinbase", RPCArg::Type::BOOL, RPCArg::Default{false}, "Include immature coinbase transactions."},
                },
                RPCResult{
                    RPCResult::Type::STR_AMOUNT, "amount", "The total amount in " + CURRENCY_UNIT + " received at this address."
                },
                RPCExamples{
            "\nThe amount from transactions with at least 1 confirmation\n"
            + HelpExampleCli("getreceivedbyaddress", "\"" + EXAMPLE_ADDRESS[0] + "\"") +
            "\nThe amount including unconfirmed transactions, zero confirmations\n"
            + HelpExampleCli("getreceivedbyaddress", "\"" + EXAMPLE_ADDRESS[0] + "\" 0") +
            "\nThe amount with at least 6 confirmations\n"
            + HelpExampleCli("getreceivedbyaddress", "\"" + EXAMPLE_ADDRESS[0] + "\" 6") +
            "\nThe amount with at least 6 confirmations including immature coinbase outputs\n"
            + HelpExampleCli("getreceivedbyaddress", "\"" + EXAMPLE_ADDRESS[0] + "\" 6 true") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("getreceivedbyaddress", "\"" + EXAMPLE_ADDRESS[0] + "\", 6")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    const std::shared_ptr<const CWallet> pwallet = GetWalletForJSONRPCRequest(request);
    if (!pwallet) return UniValue::VNULL;

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK(pwallet->cs_wallet);

    const ReceivedTally tally = GetReceivedTally(*pwallet, request.params, /*by_label=*/false);
    const CScript addr_script = GetScriptForDestination(DecodeDestination(request.params[0].get_str()));
    const AssetType domain = AssetTypeFromScriptPubKey(addr_script);
    if (domain == ASSET_RES) {
        return ValueFromAmountAsset(tally.res, ASSET_RES);
    }
    return ValueFromAmountAsset(tally.oru, ASSET_ORU);
},
    };
}


RPCHelpMan getreceivedbylabel()
{
    return RPCHelpMan{"getreceivedbylabel",
                "\nReturns the total amount received by addresses with <label> in transactions with at least [minconf] confirmations.\n",
                {
                    {"label", RPCArg::Type::STR, RPCArg::Optional::NO, "The selected label, may be the default label using \"\"."},
                    {"minconf", RPCArg::Type::NUM, RPCArg::Default{1}, "Only include transactions confirmed at least this many times."},
                    {"include_immature_coinbase", RPCArg::Type::BOOL, RPCArg::Default{false}, "Include immature coinbase transactions."},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR_AMOUNT, "amount_oru", "ORU received for this label"},
                        {RPCResult::Type::NUM, "amount_res", "RES received for this label (2 decimal places)"},
                        {RPCResult::Type::STR_AMOUNT, "amount", "ORU received (legacy; same as amount_oru)"},
                    },
                },
                RPCExamples{
            "\nAmount received by the default label with at least 1 confirmation\n"
            + HelpExampleCli("getreceivedbylabel", "\"\"") +
            "\nAmount received at the tabby label including unconfirmed amounts with zero confirmations\n"
            + HelpExampleCli("getreceivedbylabel", "\"tabby\" 0") +
            "\nThe amount with at least 6 confirmations\n"
            + HelpExampleCli("getreceivedbylabel", "\"tabby\" 6") +
            "\nThe amount with at least 6 confirmations including immature coinbase outputs\n"
            + HelpExampleCli("getreceivedbylabel", "\"tabby\" 6 true") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("getreceivedbylabel", "\"tabby\", 6, true")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    const std::shared_ptr<const CWallet> pwallet = GetWalletForJSONRPCRequest(request);
    if (!pwallet) return UniValue::VNULL;

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK(pwallet->cs_wallet);

    const ReceivedTally tally = GetReceivedTally(*pwallet, request.params, /*by_label=*/true);
    UniValue ret(UniValue::VOBJ);
    PushReceivedTallyAmountFields(ret, tally.oru, tally.res);
    return ret;
},
    };
}


RPCHelpMan getbalance()
{
    return RPCHelpMan{"getbalance",
                "\nReturns the total spendable ORU value (UTXO sum in " + CURRENCY_UNIT + ").\n"
                "RES balances are not included; use getbalances for per-asset totals.\n"
                "The spendable value is what the wallet considers currently spendable, and is\n"
                "thus affected by options which limit spendability such as -spendzeroconfchange.\n",
                {
                    {"dummy", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Remains for backward compatibility. Must be excluded or set to \"*\"."},
                    {"minconf", RPCArg::Type::NUM, RPCArg::Default{0}, "Only include transactions confirmed at least this many times."},
                    {"include_watchonly", RPCArg::Type::BOOL, RPCArg::DefaultHint{"true for watch-only wallets, otherwise false"}, "Also include spendable value in watch-only addresses (see 'importaddress')"},
                    {"avoid_reuse", RPCArg::Type::BOOL, RPCArg::Default{true}, "(only available if avoid_reuse wallet flag is set) For ORU only: exclude spendable value on reused witness-v0 addresses. Does not affect RES balances."},
                },
                RPCResult{
                    RPCResult::Type::STR_AMOUNT, "amount", "The total spendable ORU in " + CURRENCY_UNIT + " (RES excluded; see getbalances)."
                },
                RPCExamples{
            "\nThe total spendable value in the wallet with 0 or more confirmations\n"
            + HelpExampleCli("getbalance", "") +
            "\nThe total spendable value in the wallet with at least 6 confirmations\n"
            + HelpExampleCli("getbalance", "\"*\" 6") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("getbalance", "\"*\", 6")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    const std::shared_ptr<const CWallet> pwallet = GetWalletForJSONRPCRequest(request);
    if (!pwallet) return UniValue::VNULL;

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK(pwallet->cs_wallet);

    const UniValue& dummy_value = request.params[0];
    if (!dummy_value.isNull() && dummy_value.get_str() != "*") {
        throw JSONRPCError(RPC_METHOD_DEPRECATED, "dummy first argument must be excluded or set to \"*\".");
    }

    int min_depth = 0;
    if (!request.params[1].isNull()) {
        min_depth = request.params[1].getInt<int>();
    }

    bool include_watchonly = ParseIncludeWatchonly(request.params[2], *pwallet);

    bool avoid_reuse = GetAvoidReuseFlag(*pwallet, request.params[3]);

    const CAmount oru_trusted = GetAssetBalance(*pwallet, ASSET_ORU, min_depth, avoid_reuse).trusted;
    if (!include_watchonly) {
        return ValueFromAmount(oru_trusted);
    }
    const CAmount oru_wo = GetAssetBalance(*pwallet, ASSET_ORU, min_depth, avoid_reuse, ISMINE_WATCH_ONLY).trusted;
    return ValueFromAmount(oru_trusted + oru_wo);
},
    };
}

RPCHelpMan getunconfirmedbalance()
{
    return RPCHelpMan{"getunconfirmedbalance",
                "DEPRECATED\nIdentical to getbalances().orum.untrusted_pending (ORU mempool credit only).\n",
                {},
                RPCResult{RPCResult::Type::NUM, "", "The spendable value"},
                RPCExamples{""},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    const std::shared_ptr<const CWallet> pwallet = GetWalletForJSONRPCRequest(request);
    if (!pwallet) return UniValue::VNULL;

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK(pwallet->cs_wallet);

    return ValueFromAmount(GetAssetBalance(*pwallet, ASSET_ORU).pending);
},
    };
}

RPCHelpMan lockunspent()
{
    return RPCHelpMan{"lockunspent",
                "\nUpdates list of temporarily unspendable outputs.\n"
                "Temporarily lock (unlock=false) or unlock (unlock=true) specified transaction outputs.\n"
                "If no transaction outputs are specified when unlocking then all current locked transaction outputs are unlocked.\n"
                "A locked transaction output will not be chosen by automatic coin selection, when spending bitcoins.\n"
                "Manually selected coins are automatically unlocked.\n"
                "Locks are stored in memory only, unless persistent=true, in which case they will be written to the\n"
                "wallet database and loaded on node start. Unwritten (persistent=false) locks are always cleared\n"
                "(by virtue of process exit) when a node stops or fails. Unlocking will clear both persistent and not.\n"
                "Also see the listunspent call\n",
                {
                    {"unlock", RPCArg::Type::BOOL, RPCArg::Optional::NO, "Whether to unlock (true) or lock (false) the specified transactions"},
                    {"transactions", RPCArg::Type::ARR, RPCArg::Default{UniValue::VARR}, "The transaction outputs and within each, the txid (string) vout (numeric).",
                        {
                            {"", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED, "",
                                {
                                    {"txid", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The transaction id"},
                                    {"vout", RPCArg::Type::NUM, RPCArg::Optional::NO, "The output number"},
                                },
                            },
                        },
                    },
                    {"persistent", RPCArg::Type::BOOL, RPCArg::Default{false}, "Whether to write/erase this lock in the wallet database, or keep the change in memory only. Ignored for unlocking."},
                },
                RPCResult{
                    RPCResult::Type::BOOL, "", "Whether the command was successful or not"
                },
                RPCExamples{
            "\nList the unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n"
            + HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n"
            + HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nLock the transaction persistently in the wallet database\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\" true") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("lockunspent", "false, \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    std::shared_ptr<CWallet> const pwallet = GetWalletForJSONRPCRequest(request);
    if (!pwallet) return UniValue::VNULL;

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK(pwallet->cs_wallet);

    bool fUnlock = request.params[0].get_bool();

    const bool persistent{request.params[2].isNull() ? false : request.params[2].get_bool()};

    if (request.params[1].isNull()) {
        if (fUnlock) {
            if (!pwallet->UnlockAllCoins())
                throw JSONRPCError(RPC_WALLET_ERROR, "Unlocking coins failed");
        }
        return true;
    }

    const UniValue& output_params = request.params[1].get_array();

    // Create and validate the COutPoints first.

    std::vector<COutPoint> outputs;
    outputs.reserve(output_params.size());

    for (unsigned int idx = 0; idx < output_params.size(); idx++) {
        const UniValue& o = output_params[idx].get_obj();

        RPCTypeCheckObj(o,
            {
                {"txid", UniValueType(UniValue::VSTR)},
                {"vout", UniValueType(UniValue::VNUM)},
            });

        const uint256 txid(ParseHashO(o, "txid"));
        const int nOutput = find_value(o, "vout").getInt<int>();
        if (nOutput < 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout cannot be negative");
        }

        const COutPoint outpt(txid, nOutput);

        const auto it = pwallet->mapWallet.find(outpt.hash);
        if (it == pwallet->mapWallet.end()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, unknown transaction");
        }

        const CWalletTx& trans = it->second;

        if (outpt.n >= trans.tx->vout.size()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout index out of bounds");
        }

        if (pwallet->IsSpent(outpt)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected unspent output");
        }

        const bool is_locked = pwallet->IsLockedCoin(outpt);

        if (fUnlock && !is_locked) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected locked output");
        }

        if (!fUnlock && is_locked && !persistent) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, output already locked");
        }

        outputs.push_back(outpt);
    }

    std::unique_ptr<WalletBatch> batch = nullptr;
    // Unlock is always persistent
    if (fUnlock || persistent) batch = std::make_unique<WalletBatch>(pwallet->GetDatabase());

    // Atomically set (un)locked status for the outputs.
    for (const COutPoint& outpt : outputs) {
        if (fUnlock) {
            if (!pwallet->UnlockCoin(outpt, batch.get())) throw JSONRPCError(RPC_WALLET_ERROR, "Unlocking coin failed");
        } else {
            if (!pwallet->LockCoin(outpt, batch.get())) throw JSONRPCError(RPC_WALLET_ERROR, "Locking coin failed");
        }
    }

    return true;
},
    };
}

RPCHelpMan listlockunspent()
{
    return RPCHelpMan{"listlockunspent",
                "\nReturns list of temporarily unspendable outputs.\n"
                "See the lockunspent call to lock and unlock transactions for spending.\n",
                {},
                RPCResult{
                    RPCResult::Type::ARR, "", "",
                    {
                        {RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::STR_HEX, "txid", "The transaction id locked"},
                            {RPCResult::Type::NUM, "vout", "The vout value"},
                        }},
                    }
                },
                RPCExamples{
            "\nList the unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n"
            + HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n"
            + HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("listlockunspent", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    const std::shared_ptr<const CWallet> pwallet = GetWalletForJSONRPCRequest(request);
    if (!pwallet) return UniValue::VNULL;

    LOCK(pwallet->cs_wallet);

    std::vector<COutPoint> vOutpts;
    pwallet->ListLockedCoins(vOutpts);

    UniValue ret(UniValue::VARR);

    for (const COutPoint& outpt : vOutpts) {
        UniValue o(UniValue::VOBJ);

        o.pushKV("txid", outpt.hash.GetHex());
        o.pushKV("vout", (int)outpt.n);
        ret.push_back(o);
    }

    return ret;
},
    };
}

namespace {
/** Spendable buckets for one native asset (ORU uses COIN scale; RES uses RES_COIN in ValueFromAmountAsset). */
UniValue SpendableBucketsForAsset(const CWallet& wallet, AssetType asset)
{
    UniValue o(UniValue::VOBJ);
    const AssetBalanceDetail cur = GetAssetBalance(wallet, asset, 0, true);
    const CAmount immature = GetAssetImmatureBalance(wallet, asset, ISMINE_SPENDABLE);
    o.pushKV("trusted", ValueFromAmountAsset(cur.trusted, asset));
    o.pushKV("untrusted_pending", ValueFromAmountAsset(cur.pending, asset));
    o.pushKV("immature", ValueFromAmountAsset(immature, asset));
    if (wallet.IsWalletFlagSet(WALLET_FLAG_AVOID_REUSE) && OruReuseSemanticsApply(asset)) {
        const AssetBalanceDetail full = GetAssetBalance(wallet, asset, 0, false);
        const CAmount used = (full.trusted + full.pending) - (cur.trusted + cur.pending);
        o.pushKV("used", ValueFromAmountAsset(used, asset));
    }
    return o;
}

/** Match interfaces::WalletImpl::getBalances watch-only visibility (legacy import + descriptor no-private-key wallets). */
bool IncludeWatchOnlyBalanceObject(const CWallet& wallet)
{
    if (wallet.IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) return true;
    const LegacyScriptPubKeyMan* spk_man = wallet.GetLegacyScriptPubKeyMan();
    return spk_man && spk_man->HaveWatchOnly();
}
} // namespace

RPCHelpMan getbalances()
{
    return RPCHelpMan{
        "getbalances",
        "Returns per-asset spendable buckets (ORU in " + CURRENCY_UNIT + " with 8 decimals; RES with 2 decimals).\n"
        "Canonical ORU bucket key is \"oru\". Legacy keys \"orum\" and \"mine\" duplicate \"oru\" for backward compatibility.\n"
        "\"watchonly\" retains legacy ORU keys (trusted / untrusted_pending / immature) and adds an optional nested \"res\" object for RES watch-only buckets (additive; same field names).\n",
        {},
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::OBJ, "oru", "spendable ORU (witness v0 / native first asset)",
                {
                    {RPCResult::Type::STR_AMOUNT, "trusted", "trusted spendable ORU"},
                    {RPCResult::Type::STR_AMOUNT, "untrusted_pending", "untrusted pending ORU (mempool)"},
                    {RPCResult::Type::STR_AMOUNT, "immature", "immature ORU coinbase"},
                    {RPCResult::Type::STR_AMOUNT, "used", /*optional=*/true, "(only if avoid_reuse) ORU on reused addresses"},
                }},
                {RPCResult::Type::OBJ, "orum", "deprecated alias of \"oru\" (backward compatibility)",
                {
                    {RPCResult::Type::STR_AMOUNT, "trusted", "trusted spendable ORU"},
                    {RPCResult::Type::STR_AMOUNT, "untrusted_pending", "untrusted pending ORU (mempool)"},
                    {RPCResult::Type::STR_AMOUNT, "immature", "immature ORU coinbase"},
                    {RPCResult::Type::STR_AMOUNT, "used", /*optional=*/true, "(only if avoid_reuse) ORU on reused addresses"},
                }},
                {RPCResult::Type::OBJ, "res", "spendable RES (witness v1 RES programs)",
                {
                    {RPCResult::Type::STR_AMOUNT, "trusted", "trusted spendable RES"},
                    {RPCResult::Type::STR_AMOUNT, "untrusted_pending", "untrusted pending RES (mempool)"},
                    {RPCResult::Type::STR_AMOUNT, "immature", "immature RES coinbase"},
                }},
                {RPCResult::Type::OBJ, "mine", "same fields as \"oru\" (backward compatibility)",
                {
                    {RPCResult::Type::STR_AMOUNT, "trusted", "trusted spendable ORU"},
                    {RPCResult::Type::STR_AMOUNT, "untrusted_pending", "untrusted pending ORU (mempool)",},
                    {RPCResult::Type::STR_AMOUNT, "immature", "immature ORU coinbase"},
                    {RPCResult::Type::STR_AMOUNT, "used", /*optional=*/true, "(only if avoid_reuse) ORU on reused addresses"},
                }},
                {RPCResult::Type::OBJ, "watchonly", /*optional=*/true, "Watch-only buckets: legacy ORU keys at the top level plus optional nested \"res\" for RES (additive).",
                {
                    {RPCResult::Type::STR_AMOUNT, "trusted", "trusted watch-only ORU"},
                    {RPCResult::Type::STR_AMOUNT, "untrusted_pending", "untrusted pending watch-only ORU"},
                    {RPCResult::Type::STR_AMOUNT, "immature", "immature watch-only ORU coinbase"},
                    {RPCResult::Type::OBJ, "res", /*optional=*/true, "RES watch-only (same three buckets as top-level ORU keys; amounts in RES display units)",
                    {
                        {RPCResult::Type::STR_AMOUNT, "trusted", "trusted watch-only RES"},
                        {RPCResult::Type::STR_AMOUNT, "untrusted_pending", "untrusted pending watch-only RES"},
                        {RPCResult::Type::STR_AMOUNT, "immature", "immature watch-only RES coinbase"},
                    }},
                }},
            }
            },
        RPCExamples{
            HelpExampleCli("getbalances", "") +
            HelpExampleRpc("getbalances", "")},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    const std::shared_ptr<const CWallet> rpc_wallet = GetWalletForJSONRPCRequest(request);
    if (!rpc_wallet) return UniValue::VNULL;
    const CWallet& wallet = *rpc_wallet;

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    wallet.BlockUntilSyncedToCurrentChain();

    LOCK(wallet.cs_wallet);

    UniValue balances{UniValue::VOBJ};
    const UniValue oru_buckets = SpendableBucketsForAsset(wallet, ASSET_ORU);
    balances.pushKV("oru", oru_buckets);
    balances.pushKV("orum", oru_buckets); // deprecated alias
    balances.pushKV("res", SpendableBucketsForAsset(wallet, ASSET_RES));
    balances.pushKV("mine", oru_buckets);

    if (IncludeWatchOnlyBalanceObject(wallet)) {
        const AssetBalanceDetail wo_oru = GetAssetBalance(wallet, ASSET_ORU, 0, true, ISMINE_WATCH_ONLY);
        const CAmount wo_imm_oru = GetAssetImmatureBalance(wallet, ASSET_ORU, ISMINE_WATCH_ONLY);
        UniValue balances_watchonly{UniValue::VOBJ};
        balances_watchonly.pushKV("trusted", ValueFromAmount(wo_oru.trusted));
        balances_watchonly.pushKV("untrusted_pending", ValueFromAmount(wo_oru.pending));
        balances_watchonly.pushKV("immature", ValueFromAmount(wo_imm_oru));
        const AssetBalanceDetail wo_res = GetAssetBalance(wallet, ASSET_RES, 0, true, ISMINE_WATCH_ONLY);
        const CAmount wo_imm_res = GetAssetImmatureBalance(wallet, ASSET_RES, ISMINE_WATCH_ONLY);
        UniValue watch_res{UniValue::VOBJ};
        watch_res.pushKV("trusted", ValueFromAmountAsset(wo_res.trusted, ASSET_RES));
        watch_res.pushKV("untrusted_pending", ValueFromAmountAsset(wo_res.pending, ASSET_RES));
        watch_res.pushKV("immature", ValueFromAmountAsset(wo_imm_res, ASSET_RES));
        balances_watchonly.pushKV("res", watch_res);
        balances.pushKV("watchonly", balances_watchonly);
    }
    return balances;
},
    };
}

RPCHelpMan listunspent()
{
    return RPCHelpMan{
                "listunspent",
                "\nReturns array of unspent transaction outputs\n"
                "with between minconf and maxconf (inclusive) confirmations.\n"
                "Optionally filter to only include txouts paid to specified addresses.\n",
                {
                    {"minconf", RPCArg::Type::NUM, RPCArg::Default{1}, "The minimum confirmations to filter"},
                    {"maxconf", RPCArg::Type::NUM, RPCArg::Default{9999999}, "The maximum confirmations to filter"},
                    {"addresses", RPCArg::Type::ARR, RPCArg::Default{UniValue::VARR}, "The bitcoin addresses to filter",
                        {
                            {"address", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "bitcoin address"},
                        },
                    },
                    {"include_unsafe", RPCArg::Type::BOOL, RPCArg::Default{true}, "Include outputs that are not safe to spend\n"
                              "See description of \"safe\" attribute below."},
                    {"query_options", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED, "JSON with query options",
                        {
                            {"minimumAmount", RPCArg::Type::AMOUNT, RPCArg::Default{FormatMoney(0)}, "Minimum value of each UTXO in " + CURRENCY_UNIT + ""},
                            {"maximumAmount", RPCArg::Type::AMOUNT, RPCArg::DefaultHint{"unlimited"}, "Maximum value of each UTXO in " + CURRENCY_UNIT + ""},
                            {"maximumCount", RPCArg::Type::NUM, RPCArg::DefaultHint{"unlimited"}, "Maximum number of UTXOs"},
                            {"minimumSumAmount", RPCArg::Type::AMOUNT, RPCArg::DefaultHint{"unlimited"}, "Minimum sum value of all UTXOs in " + CURRENCY_UNIT + ""},
                            {"include_immature_coinbase", RPCArg::Type::BOOL, RPCArg::Default{false}, "Include immature coinbase UTXOs"}
                        },
                        RPCArgOptions{.oneline_description="query_options"}},
                },
                RPCResult{
                    RPCResult::Type::ARR, "", "",
                    {
                        {RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::STR_HEX, "txid", "the transaction id"},
                            {RPCResult::Type::NUM, "vout", "the vout value"},
                            {RPCResult::Type::STR, "address", /*optional=*/true, "the bitcoin address"},
                            {RPCResult::Type::STR, "label", /*optional=*/true, "The associated label, or \"\" for the default label"},
                            {RPCResult::Type::STR, "scriptPubKey", "the script key"},
                            {RPCResult::Type::STR, "asset", "RIGOR native asset of this output: \"ORU\" | \"RES\" | \"NONE\" (W1.1C explicit asset tag)"},
                            {RPCResult::Type::STR_AMOUNT, "amount", "the transaction output amount in the asset's display units (ORU = " + CURRENCY_UNIT + ", RES = display units)"},
                            {RPCResult::Type::NUM, "confirmations", "The number of confirmations"},
                            {RPCResult::Type::NUM, "ancestorcount", /*optional=*/true, "The number of in-mempool ancestor transactions, including this one (if transaction is in the mempool)"},
                            {RPCResult::Type::NUM, "ancestorsize", /*optional=*/true, "The virtual transaction size of in-mempool ancestors, including this one (if transaction is in the mempool)"},
                            {RPCResult::Type::STR_AMOUNT, "ancestorfees", /*optional=*/true, "The total fees of in-mempool ancestors (including this one) with fee deltas used for mining priority in " + CURRENCY_ATOM + " (if transaction is in the mempool)"},
                            {RPCResult::Type::STR_HEX, "redeemScript", /*optional=*/true, "The redeemScript if scriptPubKey is P2SH"},
                            {RPCResult::Type::STR, "witnessScript", /*optional=*/true, "witnessScript if the scriptPubKey is P2WSH or P2SH-P2WSH"},
                            {RPCResult::Type::BOOL, "spendable", "Whether we have the private keys to spend this output"},
                            {RPCResult::Type::BOOL, "solvable", "Whether we know how to spend this output, ignoring the lack of keys"},
                            {RPCResult::Type::BOOL, "reused", /*optional=*/true, "(only if avoid_reuse) ORU witness-v0 outputs on previously spent-from addresses; omitted for RES (single-slot container is reusable)"},
                            {RPCResult::Type::STR, "desc", /*optional=*/true, "(only when solvable) A descriptor for spending this output"},
                            {RPCResult::Type::ARR, "parent_descs", /*optional=*/false, "List of parent descriptors for the scriptPubKey of this coin.", {
                                {RPCResult::Type::STR, "desc", "The descriptor string."},
                            }},
                            {RPCResult::Type::BOOL, "safe", "Whether this output is considered safe to spend. Unconfirmed transactions\n"
                                                            "from outside keys and unconfirmed replacement transactions are considered unsafe\n"
                                                            "and are not eligible for spending by fundrawtransaction and sendtoaddress."},
                        }},
                    }
                },
                RPCExamples{
                    HelpExampleCli("listunspent", "")
            + HelpExampleCli("listunspent", "6 9999999 \"[\\\"" + EXAMPLE_ADDRESS[0] + "\\\",\\\"" + EXAMPLE_ADDRESS[1] + "\\\"]\"")
            + HelpExampleRpc("listunspent", "6, 9999999 \"[\\\"" + EXAMPLE_ADDRESS[0] + "\\\",\\\"" + EXAMPLE_ADDRESS[1] + "\\\"]\"")
            + HelpExampleCli("listunspent", "6 9999999 '[]' true '{ \"minimumAmount\": 0.005 }'")
            + HelpExampleRpc("listunspent", "6, 9999999, [] , true, { \"minimumAmount\": 0.005 } ")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    const std::shared_ptr<const CWallet> pwallet = GetWalletForJSONRPCRequest(request);
    if (!pwallet) return UniValue::VNULL;

    int nMinDepth = 1;
    if (!request.params[0].isNull()) {
        nMinDepth = request.params[0].getInt<int>();
    }

    int nMaxDepth = 9999999;
    if (!request.params[1].isNull()) {
        nMaxDepth = request.params[1].getInt<int>();
    }

    std::set<CTxDestination> destinations;
    if (!request.params[2].isNull()) {
        UniValue inputs = request.params[2].get_array();
        for (unsigned int idx = 0; idx < inputs.size(); idx++) {
            const UniValue& input = inputs[idx];
            CTxDestination dest = DecodeDestination(input.get_str());
            if (!IsValidDestination(dest)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: ") + input.get_str());
            }
            if (!destinations.insert(dest).second) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid parameter, duplicated address: ") + input.get_str());
            }
        }
    }

    bool include_unsafe = true;
    if (!request.params[3].isNull()) {
        include_unsafe = request.params[3].get_bool();
    }

    CoinFilterParams filter_coins;
    filter_coins.min_amount = 0;

    if (!request.params[4].isNull()) {
        const UniValue& options = request.params[4].get_obj();

        RPCTypeCheckObj(options,
            {
                {"minimumAmount", UniValueType()},
                {"maximumAmount", UniValueType()},
                {"minimumSumAmount", UniValueType()},
                {"maximumCount", UniValueType(UniValue::VNUM)},
                {"include_immature_coinbase", UniValueType(UniValue::VBOOL)}
            },
            true, true);

        if (options.exists("minimumAmount"))
            filter_coins.min_amount = AmountFromValue(options["minimumAmount"]);

        if (options.exists("maximumAmount"))
            filter_coins.max_amount = AmountFromValue(options["maximumAmount"]);

        if (options.exists("minimumSumAmount"))
            filter_coins.min_sum_amount = AmountFromValue(options["minimumSumAmount"]);

        if (options.exists("maximumCount"))
            filter_coins.max_count = options["maximumCount"].getInt<int64_t>();

        if (options.exists("include_immature_coinbase")) {
            filter_coins.include_immature_coinbase = options["include_immature_coinbase"].get_bool();
        }
    }

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    UniValue results(UniValue::VARR);
    std::vector<COutput> vecOutputs;
    {
        CCoinControl cctl;
        cctl.m_avoid_address_reuse = false;
        cctl.m_min_depth = nMinDepth;
        cctl.m_max_depth = nMaxDepth;
        cctl.m_include_unsafe_inputs = include_unsafe;
        LOCK(pwallet->cs_wallet);
        vecOutputs = AvailableCoinsListUnspent(*pwallet, &cctl, filter_coins).All();
    }

    LOCK(pwallet->cs_wallet);

    const bool avoid_reuse = pwallet->IsWalletFlagSet(WALLET_FLAG_AVOID_REUSE);

    for (const COutput& out : vecOutputs) {
        CTxDestination address;
        const CScript& scriptPubKey = out.txout.scriptPubKey;
        bool fValidAddress = ExtractDestination(scriptPubKey, address);
        const bool reused = avoid_reuse && OruReuseSemanticsApply(scriptPubKey) && pwallet->IsSpentKey(scriptPubKey);

        if (destinations.size() && (!fValidAddress || !destinations.count(address)))
            continue;

        UniValue entry(UniValue::VOBJ);
        entry.pushKV("txid", out.outpoint.hash.GetHex());
        entry.pushKV("vout", (int)out.outpoint.n);

        if (fValidAddress) {
            entry.pushKV("address", EncodeDestination(address));

            const auto* address_book_entry = pwallet->FindAddressBookEntry(address);
            if (address_book_entry) {
                entry.pushKV("label", address_book_entry->GetLabel());
            }

            std::unique_ptr<SigningProvider> provider = pwallet->GetSolvingProvider(scriptPubKey);
            if (provider) {
                if (scriptPubKey.IsPayToScriptHash()) {
                    const CScriptID& hash = CScriptID(std::get<ScriptHash>(address));
                    CScript redeemScript;
                    if (provider->GetCScript(hash, redeemScript)) {
                        entry.pushKV("redeemScript", HexStr(redeemScript));
                        // Now check if the redeemScript is actually a P2WSH script
                        CTxDestination witness_destination;
                        if (redeemScript.IsPayToWitnessScriptHash()) {
                            bool extracted = ExtractDestination(redeemScript, witness_destination);
                            CHECK_NONFATAL(extracted);
                            // Also return the witness script
                            const WitnessV0ScriptHash& whash = std::get<WitnessV0ScriptHash>(witness_destination);
                            CScriptID id{RIPEMD160(whash)};
                            CScript witnessScript;
                            if (provider->GetCScript(id, witnessScript)) {
                                entry.pushKV("witnessScript", HexStr(witnessScript));
                            }
                        }
                    }
                } else if (scriptPubKey.IsPayToWitnessScriptHash()) {
                    const WitnessV0ScriptHash& whash = std::get<WitnessV0ScriptHash>(address);
                    CScriptID id{RIPEMD160(whash)};
                    CScript witnessScript;
                    if (provider->GetCScript(id, witnessScript)) {
                        entry.pushKV("witnessScript", HexStr(witnessScript));
                    }
                }
            }
        }

        entry.pushKV("scriptPubKey", HexStr(scriptPubKey));
        if (ConvSignTraceActive()) {
            int wver = 0;
            std::vector<unsigned char> wprog;
            if (scriptPubKey.IsWitnessProgram(wver, wprog) && wver == 0 && wprog.size() == 20) {
                const AssetType at = AssetTypeFromScriptPubKey(scriptPubKey);
                std::string desc_snip;
                if (out.solvable) {
                    std::unique_ptr<SigningProvider> provider = pwallet->GetSolvingProvider(scriptPubKey);
                    if (provider) {
                        auto descriptor = InferDescriptor(scriptPubKey, *provider);
                        desc_snip = descriptor->ToString();
                    }
                }
                LogPrint(BCLog::CONVERGESIGN,
                         "convtrace listunspent txid=%s vout=%i asset_enum=%i spendable=%i solvable=%i scriptPubKey=%s desc=%s\n",
                         out.outpoint.hash.GetHex(),
                         static_cast<int>(out.outpoint.n),
                         static_cast<int>(at),
                         out.spendable ? 1 : 0,
                         out.solvable ? 1 : 0,
                         HexStr(scriptPubKey),
                         desc_snip);
            }
        }
        // W1.1C FREEZE INVARIANT: explicit per-UTXO asset tag. Integrators MUST NOT
        // infer asset from witness version alone; classification is via script-as-asset
        // (AssetTypeFromScriptPubKey). Canonical strings: "ORU" | "RES" | "NONE".
        const AssetType lu_asset = AssetTypeFromScriptPubKey(out.txout.scriptPubKey);
        entry.pushKV("asset", lu_asset == ASSET_ORU ? "ORU" : (lu_asset == ASSET_RES ? "RES" : "NONE"));
        entry.pushKV("amount", ValueFromAmountAsset(out.txout.nValue, lu_asset));
        entry.pushKV("confirmations", out.depth);
        if (!out.depth) {
            size_t ancestor_count, descendant_count, ancestor_size;
            CAmount ancestor_fees;
            pwallet->chain().getTransactionAncestry(out.outpoint.hash, ancestor_count, descendant_count, &ancestor_size, &ancestor_fees);
            if (ancestor_count) {
                entry.pushKV("ancestorcount", uint64_t(ancestor_count));
                entry.pushKV("ancestorsize", uint64_t(ancestor_size));
                entry.pushKV("ancestorfees", uint64_t(ancestor_fees));
            }
        }
        entry.pushKV("spendable", out.spendable);
        entry.pushKV("solvable", out.solvable);
        if (out.solvable) {
            std::unique_ptr<SigningProvider> provider = pwallet->GetSolvingProvider(scriptPubKey);
            if (provider) {
                auto descriptor = InferDescriptor(scriptPubKey, *provider);
                entry.pushKV("desc", descriptor->ToString());
            }
        }
        PushParentDescriptors(*pwallet, scriptPubKey, entry);
        if (avoid_reuse && OruReuseSemanticsApply(scriptPubKey)) {
            entry.pushKV("reused", reused);
        }
        entry.pushKV("safe", out.safe);
        results.push_back(entry);
    }

    return results;
},
    };
}
} // namespace wallet
