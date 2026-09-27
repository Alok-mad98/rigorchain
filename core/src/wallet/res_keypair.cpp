// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/res_keypair.h>

#include <crypto/ed25519_provider.h>
#include <random.h>
#include <span.h>

#include <array>
#include <cstddef>
#include <vector>

// see header for the relocation rationale.

namespace Consensus {

bool GenerateResKeypair(std::vector<unsigned char>& program32_out,
                        std::array<unsigned char, ed25519::SECRETKEY_SIZE>& secret_out)
{
    std::array<unsigned char, ed25519::SEED_SIZE> seed{};
    GetStrongRandBytes(seed);
    program32_out.assign(ed25519::PUBKEY_SIZE, 0);
    return ed25519::SeedToKeypair(Span<const unsigned char>{seed.data(), seed.size()},
                                  program32_out,
                                  secret_out);
}

} // namespace Consensus
