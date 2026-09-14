// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Optional consensus decision trace. Default off: every Note* returns immediately.
// Does not change accept/reject. Not Coq KernelInput. No keys.

#ifndef BITCOIN_CONSENSUS_TRACE_CONSENSUS_TRACE_H
#define BITCOIN_CONSENSUS_TRACE_CONSENSUS_TRACE_H

#include <consensus/amount.h>
#include <consensus/validation.h>
#include <uint256.h>

#include <string>

void RigorConsensusTraceInit(bool enabled, const std::string& out_dir);
bool RigorConsensusTraceEnabled();

void RigorConsensusTraceNotePow(bool ok, const char* reason);
void RigorConsensusTraceNoteAsert(bool ok, unsigned expected_nbits, unsigned got_nbits);
void RigorConsensusTraceNoteAsertExpected(unsigned expected_nbits);
void RigorConsensusTraceNoteHeader(const char* function, bool ok, const char* reason);
void RigorConsensusTraceNoteCheckBlock(bool ok, const char* reason);
void RigorConsensusTraceNoteContextualCheckBlock(bool ok, const char* reason);
void RigorConsensusTraceNoteTx(const uint256& txid, const char* function,
                               bool inputs_checked, bool utxo_ok, bool script_ok, bool amount_ok,
                               CAmount fee, const char* reason);

/** Environment observation only. Never read by accept/reject. Not a KernelInput field. */
void RigorConsensusTraceNoteNetworkContext(const uint256& block_hash, int peer_count,
                                           const char* source, int64_t peer_id, int64_t noted_ms);

/** RAII: begin a ConnectBlock session; destructor writes JSON if tracing is on. */
class RigorConsensusTraceConnectGuard
{
public:
    RigorConsensusTraceConnectGuard(const uint256& block_hash, int height, const BlockValidationState& state);
    ~RigorConsensusTraceConnectGuard();
    RigorConsensusTraceConnectGuard(const RigorConsensusTraceConnectGuard&) = delete;
    RigorConsensusTraceConnectGuard& operator=(const RigorConsensusTraceConnectGuard&) = delete;

private:
    const BlockValidationState& m_state;
    bool m_active;
};

#endif
