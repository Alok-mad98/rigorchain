// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/fingerprint.h>

#include <crypto/common.h>
#include <hash.h>

#include <array>
#include <cstring>

namespace {

inline constexpr std::size_t kOffConvergenceRoot = 0;
inline constexpr std::size_t kOffWinnerTxid = 32;
inline constexpr std::size_t kOffSigma = 64;
inline constexpr std::size_t kOffHeight = 96;
inline constexpr std::size_t kOffTargetPrev = 100;
inline constexpr std::size_t kUint256Len = 32;
inline constexpr std::size_t kUint32Len = 4;

static_assert(kOffConvergenceRoot == 0, "Fingerprint layout: ConvergenceRoot at offset 0");
static_assert(kOffWinnerTxid == 32, "Fingerprint layout: winner_txid at offset 32");
static_assert(kOffSigma == 64, "Fingerprint layout: Sigma at offset 64");
static_assert(kOffHeight == 96, "Fingerprint layout: height at offset 96");
static_assert(kOffTargetPrev == 100, "Fingerprint layout: target_prev at offset 100");
static_assert(kOffTargetPrev + kUint32Len == CONSENSUS_FINGERPRINT_INPUT_SIZE,
              "Fingerprint preimage must be exactly 104 bytes");

} // namespace

uint256 ComputeConsensusFingerprint(
    const uint256& convergence_root,
    const uint256& winner_txid,
    const uint256& sigma,
    uint32_t height,
    uint32_t target_prev)
{
    // Native serialization only. Field order is mandatory:
    // ConvergenceRoot || winner_txid || Sigma || height || target_prev
    std::array<unsigned char, CONSENSUS_FINGERPRINT_INPUT_SIZE> buf{};
    std::memcpy(buf.data() + kOffConvergenceRoot, convergence_root.begin(), kUint256Len);
    std::memcpy(buf.data() + kOffWinnerTxid, winner_txid.begin(), kUint256Len);
    std::memcpy(buf.data() + kOffSigma, sigma.begin(), kUint256Len);
    WriteLE32(buf.data() + kOffHeight, height);
    WriteLE32(buf.data() + kOffTargetPrev, target_prev);

    uint256 out;
    CHash256().Write(buf).Finalize(out);
    return out;
}
