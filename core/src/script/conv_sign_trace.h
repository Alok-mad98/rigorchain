// Copyright (c) 2026 The RigorChain developers
// SPDX-License-Identifier: MIT

#ifndef BITCOIN_SCRIPT_CONV_SIGN_TRACE_H
#define BITCOIN_SCRIPT_CONV_SIGN_TRACE_H

#include <logging.h>
#include <uint256.h>

#include <string>

struct CMutableTransaction;
class CScript;
class Coin;

/** True when -debug=convergesign is enabled (BCLog::CONVERGESIGN). */
bool ConvSignTraceActive();

/** Raw tx spends at least one input and has a witness-v1 CVG2 convergence vout. */
bool ConvSignTxShape(const CMutableTransaction& tx);

std::string ConvSignHexScript(const CScript& script);

void ConvTraceLogCoinSingle(const char* tag, const uint256& txid, unsigned int nout, const Coin& coin);

#endif // BITCOIN_SCRIPT_CONV_SIGN_TRACE_H
