// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <hash.h>
#include <key.h>
#include <key_io.h>
#include <pubkey.h>
#include <script/standard.h>
#include <uint256.h>
#include <util/message.h>
#include <util/strencodings.h>

#include <cassert>
#include <optional>
#include <string>
#include <variant>
#include <vector>

/**
 * Text used to signify that a signed message follows and to prevent
 * inadvertently signing a transaction.
 */
const std::string MESSAGE_MAGIC = "Bitcoin Signed Message:\n";

MessageVerificationResult MessageVerify(
    const std::string& address,
    const std::string& signature,
    const std::string& message)
{
    CTxDestination destination = DecodeDestination(address);
    if (!IsValidDestination(destination)) {
        return MessageVerificationResult::ERR_INVALID_ADDRESS;
    }

    const WitnessV0KeyHash* wpkh = std::get_if<WitnessV0KeyHash>(&destination);
    if (!wpkh) {
        return MessageVerificationResult::ERR_ADDRESS_NO_KEY;
    }

    auto signature_bytes = DecodeBase64(signature);
    if (!signature_bytes) {
        return MessageVerificationResult::ERR_MALFORMED_SIGNATURE;
    }

    if (signature_bytes->size() != 1 + CPubKey::COMPRESSED_SIZE + 64 || (*signature_bytes)[0] != 0xED) {
        return MessageVerificationResult::ERR_MALFORMED_SIGNATURE;
    }
    CPubKey pubkey(signature_bytes->data() + 1, signature_bytes->data() + 1 + CPubKey::COMPRESSED_SIZE);
    std::vector<unsigned char> sig(signature_bytes->begin() + 1 + CPubKey::COMPRESSED_SIZE, signature_bytes->end());
    if (!pubkey.Verify(MessageHash(message), sig)) {
        return MessageVerificationResult::ERR_NOT_SIGNED;
    }

    if (!(CTxDestination(WitnessV0KeyHash(pubkey)) == destination)) {
        return MessageVerificationResult::ERR_NOT_SIGNED;
    }

    return MessageVerificationResult::OK;
}

bool MessageSign(
    const CKey& privkey,
    const std::string& message,
    std::string& signature)
{
    std::vector<unsigned char> sig64;
    if (!privkey.Sign(MessageHash(message), sig64)) {
        return false;
    }
    const CPubKey pubkey = privkey.GetPubKey();
    std::vector<unsigned char> signature_bytes;
    signature_bytes.reserve(1 + CPubKey::COMPRESSED_SIZE + sig64.size());
    signature_bytes.push_back(0xED);
    signature_bytes.insert(signature_bytes.end(), pubkey.begin(), pubkey.end());
    signature_bytes.insert(signature_bytes.end(), sig64.begin(), sig64.end());
    signature = EncodeBase64(signature_bytes);

    return true;
}

uint256 MessageHash(const std::string& message)
{
    HashWriter hasher{};
    hasher << MESSAGE_MAGIC << message;

    return hasher.GetHash();
}

std::string SigningResultString(const SigningResult res)
{
    switch (res) {
        case SigningResult::OK:
            return "No error";
        case SigningResult::PRIVATE_KEY_NOT_AVAILABLE:
            return "Private key not available";
        case SigningResult::SIGNING_FAILED:
            return "Sign failed";
        // no default case, so the compiler can warn about missing cases
    }
    assert(false);
}
