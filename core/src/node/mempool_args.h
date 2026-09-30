// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_MEMPOOL_ARGS_H
#define BITCOIN_NODE_MEMPOOL_ARGS_H

#include <cstdint>
#include <optional>

class ArgsManager;
class CChainParams;
struct bilingual_str;
namespace kernel {
struct MemPoolOptions;
};

//! CVE-2025-46597: a 32-bit node must not be asked to hold a multi-gigabyte mempool.
static constexpr int MAX_32BIT_MEMPOOL_MB{500};

/** False when a 32-bit build is asked for more than MAX_32BIT_MEMPOOL_MB. 64-bit always allows the value. */
inline bool MempoolSizeAllowed(bool is_32bit, int64_t megabytes)
{
    return !(is_32bit && megabytes > MAX_32BIT_MEMPOOL_MB);
}

/**
 * Overlay the options set in \p argsman on top of corresponding members in \p mempool_opts.
 * Returns an error if one was encountered.
 *
 * @param[in]  argsman The ArgsManager in which to check set options.
 * @param[in,out] mempool_opts The MemPoolOptions to modify according to \p argsman.
 */
[[nodiscard]] std::optional<bilingual_str> ApplyArgsManOptions(const ArgsManager& argsman, const CChainParams& chainparams, kernel::MemPoolOptions& mempool_opts);


#endif // BITCOIN_NODE_MEMPOOL_ARGS_H
