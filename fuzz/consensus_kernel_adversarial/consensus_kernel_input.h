// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-063: byte-oriented Consensus Kernel input. Not a second authority.

#ifndef RIGOR_FUZZ_CONSENSUS_KERNEL_INPUT_H
#define RIGOR_FUZZ_CONSENSUS_KERNEL_INPUT_H

#include <cstdint>
#include <cstring>
#include <vector>

struct ConsensusKernelInput {
    int32_t height{0};
    uint8_t kind{0};
    std::vector<uint8_t> script;
    int32_t n_candidates{0};
    uint64_t weight0{0};
    uint64_t weight1{0};
    uint8_t txid0[32]{};
    uint8_t txid1[32]{};
    int64_t fees_oru{0};
    int64_t fee_burn{0};
    int64_t converge_sum{0};
    int64_t res_mint{0};
    int64_t oru_coinbase{0};
    int64_t res_coinbase{0};
    uint8_t has_winner{0};
    int32_t draw_cardinality{0};
    uint8_t t_mode{0};
};

inline ConsensusKernelInput ParseConsensusKernelInput(const uint8_t* data, size_t size)
{
    ConsensusKernelInput in;
    if (data == nullptr || size == 0) return in;
    auto take = [&](void* dst, size_t n, size_t& off) {
        if (off + n > size) {
            std::memset(dst, 0, n);
            off = size;
            return;
        }
        std::memcpy(dst, data + off, n);
        off += n;
    };
    size_t off = 0;
    take(&in.height, sizeof(in.height), off);
    take(&in.kind, sizeof(in.kind), off);
    uint16_t script_len = 0;
    take(&script_len, sizeof(script_len), off);
    if (script_len > 4096) script_len = 4096;
    if (off < size) {
        const size_t avail = size - off;
        const size_t n = script_len < avail ? script_len : avail;
        in.script.assign(data + off, data + off + n);
        off += n;
    }
    take(&in.n_candidates, sizeof(in.n_candidates), off);
    take(&in.weight0, sizeof(in.weight0), off);
    take(&in.weight1, sizeof(in.weight1), off);
    take(in.txid0, sizeof(in.txid0), off);
    take(in.txid1, sizeof(in.txid1), off);
    take(&in.fees_oru, sizeof(in.fees_oru), off);
    take(&in.fee_burn, sizeof(in.fee_burn), off);
    take(&in.converge_sum, sizeof(in.converge_sum), off);
    take(&in.res_mint, sizeof(in.res_mint), off);
    take(&in.oru_coinbase, sizeof(in.oru_coinbase), off);
    take(&in.res_coinbase, sizeof(in.res_coinbase), off);
    take(&in.has_winner, sizeof(in.has_winner), off);
    take(&in.draw_cardinality, sizeof(in.draw_cardinality), off);
    take(&in.t_mode, sizeof(in.t_mode), off);
    return in;
}

#endif
