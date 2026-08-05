// Copyright (c) 2026 The RigorChain developers
// SPDX-License-Identifier: MIT

#include <script/conv_sign_trace.h>

#include <coins.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <uint256.h>
#include <util/moneystr.h>
#include <util/strencodings.h>

bool ConvSignTraceActive()
{
    return LogAcceptCategory(BCLog::CONVERGESIGN, BCLog::Level::Debug);
}

bool ConvSignTxShape(const CMutableTransaction& tx)
{
    if (tx.vin.empty()) return false;
    bool has_conv = false;
    for (const auto& o : tx.vout) {
        if (IsConvergenceScript(o.scriptPubKey)) {
            has_conv = true;
            break;
        }
    }
    return has_conv;
}

std::string ConvSignHexScript(const CScript& script)
{
    return HexStr(script);
}

void ConvTraceLogCoinSingle(const char* tag, const uint256& txid, unsigned int nout, const Coin& coin)
{
    if (!ConvSignTraceActive()) return;
    const CTxOut& o = coin.out;
    LogPrint(BCLog::CONVERGESIGN,
             "convtrace %s prevout=%s:%u nValue=%s scriptPubKey=%s coinbase=%i height=%u\n",
             tag,
             txid.ToString(),
             nout,
             FormatMoney(o.nValue),
             HexStr(o.scriptPubKey),
             coin.IsCoinBase() ? 1 : 0,
             static_cast<unsigned>(coin.nHeight));
}
