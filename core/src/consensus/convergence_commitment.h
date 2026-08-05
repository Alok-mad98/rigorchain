// Copyright (c) The Rigor Chain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_CONVERGENCE_COMMITMENT_H
#define BITCOIN_CONSENSUS_CONVERGENCE_COMMITMENT_H

#include <primitives/transaction.h>
#include <script/script.h>
#include <uint256.h>

#include <cstdint>
#include <optional>

/**
 * @ingroup consensus
 *
 * @par The sword and the convergence
 * In the legendarium, the shards of Narsil were **forged** into ConvergenceDrawCandidates — many fragments made
 * one true edge. **OpuHash** names the immovable foundation beneath our chain: proof-of-work as
 * dwarven labour under the mountain, honest weight that cannot be argued away. **ConvergenceDrawCandidates** names
 * what rises from that foundation in the convergence-RES layer: not a trinket of chance, but the
 * **Flame of the West** made concrete — a single, sharp outcome the whole network can agree on.
 *
 * @par Fields (metaphor and wire)
 * - `hash` — **the blade**: the per-transaction verdict digest when evaluated in full context;
 *            unset (zero) inside parsed CVGE payloads.
 * - `root` — **the hilt**: the candidate-set Merkle root committed in coinbase CVGE (v2 on-chain
 *            bytes); what the block **grips** so the draw cannot drift from the body.
 *
 * @note CVGE version 2 serializes only `root` (32 bytes). `hash` is for off-chain / runtime use.
 */
struct ConvergenceDrawCandidates {
    uint256 hash{}; // The blade — per-tx ConvergenceDrawCandidatesResult when computed off-chain; zero in parsed CVGE payloads
    uint256 root{}; // The hilt — sorted candidate txid Merkle root (on-chain bytes)

    bool operator==(const ConvergenceDrawCandidates& o) const { return hash == o.hash && root == o.root; }

    void SetNull()
    {
        hash.SetNull();
        root.SetNull();
    }
};

/** Coinbase OP_RETURN convergence commitment. Magic: ASCII "CVGE", version 2: cumulative + ConvergenceDrawCandidates root only. */
struct ConvergenceCommitment
{
    static constexpr uint8_t EXPECTED_VERSION = 2;
    uint8_t m_version{EXPECTED_VERSION};
    uint64_t m_cumulative_convergence{0};
    ConvergenceDrawCandidates m_draw_candidates{};

    bool operator==(const ConvergenceCommitment& o) const
    {
        return m_version == o.m_version && m_cumulative_convergence == o.m_cumulative_convergence &&
               m_draw_candidates == o.m_draw_candidates;
    }
};

/** Build scriptPubKey: OP_RETURN <serialized commitment>. */
CScript ConvergenceCommitmentScript(const ConvergenceCommitment& fc);

std::optional<ConvergenceCommitment> ParseConvergenceCommitmentPayload(const std::vector<unsigned char>& data);

std::optional<ConvergenceCommitment> FindLastConvergenceCommitmentInCoinbase(const CTransaction& coinbase);

#endif // BITCOIN_CONSENSUS_CONVERGENCE_COMMITMENT_H
