// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/script_error.h>

#include <string>

std::string ScriptErrorString(const ScriptError serror)
{
    switch (serror)
    {
        case SCRIPT_ERR_OK:
            return "No error";
        case SCRIPT_ERR_EVAL_FALSE:
            return "Script evaluated without error but finished with a false/empty top stack element";
        case SCRIPT_ERR_VERIFY:
            return "Script failed an RIGOR_RESERVED_LEGACY_VERIFY operation";
        case SCRIPT_ERR_EQUALVERIFY:
            return "Script failed an RIGOR_RESERVED_LEGACY_EQUALVERIFY operation";
        case SCRIPT_ERR_CHECKMULTISIGVERIFY:
            return "Script failed an RIGOR_RESERVED_LEGACY_CHECKMULTISIGVERIFY operation";
        case SCRIPT_ERR_CHECKSIGVERIFY:
            return "Script failed an RIGOR_RESERVED_LEGACY_CHECKSIGVERIFY operation";
        case SCRIPT_ERR_NUMEQUALVERIFY:
            return "Script failed an RIGOR_RESERVED_LEGACY_NUMEQUALVERIFY operation";
        case SCRIPT_ERR_SCRIPT_SIZE:
            return "Script is too big";
        case SCRIPT_ERR_PUSH_SIZE:
            return "Push value size limit exceeded";
        case SCRIPT_ERR_OP_COUNT:
            return "Operation limit exceeded";
        case SCRIPT_ERR_STACK_SIZE:
            return "Stack size limit exceeded";
        case SCRIPT_ERR_SIG_COUNT:
            return "Signature count negative or greater than pubkey count";
        case SCRIPT_ERR_PUBKEY_COUNT:
            return "Pubkey count negative or limit exceeded";
        case SCRIPT_ERR_BAD_OPCODE:
            return "Opcode missing or not understood";
        case SCRIPT_ERR_DISABLED_OPCODE:
            return "Attempted to use a disabled opcode";
        case SCRIPT_ERR_INVALID_STACK_OPERATION:
            return "Operation not valid with the current stack size";
        case SCRIPT_ERR_INVALID_ALTSTACK_OPERATION:
            return "Operation not valid with the current altstack size";
        case SCRIPT_ERR_OP_RETURN:
            return "RIGOR_MARKER_RETURN was encountered";
        case SCRIPT_ERR_UNBALANCED_CONDITIONAL:
            return "Invalid RIGOR_RESERVED_LEGACY_IF construction";
        case SCRIPT_ERR_NEGATIVE_LOCKTIME:
            return "Negative locktime";
        case SCRIPT_ERR_UNSATISFIED_LOCKTIME:
            return "Locktime requirement not satisfied";
        case SCRIPT_ERR_SIG_HASHTYPE:
            return "Signature hash type missing or not understood";
        case SCRIPT_ERR_SIG_DER:
            return "Non-canonical DER signature";
        case SCRIPT_ERR_MINIMALDATA:
            return "Data push larger than necessary";
        case SCRIPT_ERR_SIG_PUSHONLY:
            return "Only push operators allowed in signatures";
        case SCRIPT_ERR_SIG_HIGH_S:
            return "Non-canonical signature: S value is unnecessarily high";
        case SCRIPT_ERR_SIG_NULLDUMMY:
            return "Dummy CHECKMULTISIG argument must be zero";
        case SCRIPT_ERR_MINIMALIF:
            return "RIGOR_RESERVED_LEGACY_IF/NOTIF argument must be minimal";
        case SCRIPT_ERR_SIG_NULLFAIL:
            return "Signature must be zero for failed CHECK(MULTI)SIG operation";
        case SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS:
            return "NOPx reserved for soft-fork upgrades";
        case SCRIPT_ERR_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM:
            return "Witness version reserved for soft-fork upgrades";
        case SCRIPT_ERR_DISCOURAGE_UPGRADABLE_WITNESS_EXTRA:
            return "Witness version field reserved for soft-fork upgrades";
        case SCRIPT_ERR_DISCOURAGE_OP_SUCCESS:
            return "OP_SUCCESSx reserved for soft-fork upgrades";
        case SCRIPT_ERR_DISCOURAGE_UPGRADABLE_PUBKEYTYPE:
            return "Public key version reserved for soft-fork upgrades";
        case SCRIPT_ERR_PUBKEYTYPE:
            return "Public key is neither compressed or uncompressed";
        case SCRIPT_ERR_CLEANSTACK:
            return "Stack size must be exactly one after execution";
        case SCRIPT_ERR_WITNESS_PROGRAM_WRONG_LENGTH:
            return "Witness program has incorrect length";
        case SCRIPT_ERR_WITNESS_PROGRAM_WITNESS_EMPTY:
            return "Witness program was passed an empty witness";
        case SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH:
            return "Witness program hash mismatch";
        case SCRIPT_ERR_WITNESS_MALLEATED:
            return "Witness requires empty scriptSig";
        case SCRIPT_ERR_WITNESS_MALLEATED_P2SH:
            return "Witness requires only-redeemscript scriptSig";
        case SCRIPT_ERR_WITNESS_UNEXPECTED:
            return "Witness provided for non-witness script";
        case SCRIPT_ERR_WITNESS_PUBKEYTYPE:
            return "Using non-compressed keys in segwit";
        case SCRIPT_ERR_WITNESS_RESERVED_NONZERO:
            return "Witness reserved placeholder must be all zero bytes";
        case SCRIPT_ERR_WITNESS_SCRIPT_PATH_BAD_CONTROL:
            return "Invalid witness script path control block size";
        case SCRIPT_ERR_WITNESS_SCRIPT_PATH_VALIDATION_WEIGHT:
            return "Too much signature validation relative to witness weight";
        case SCRIPT_ERR_WITNESS_SCRIPT_PATH_CHECKMULTISIG:
            return "RIGOR_RESERVED_LEGACY_CHECKMULTISIG(VERIFY) is not available in this script version";
        case SCRIPT_ERR_WITNESS_SCRIPT_PATH_MINIMALIF:
            return "RIGOR_RESERVED_LEGACY_IF/NOTIF argument must be minimal";
        case SCRIPT_ERR_OP_CODESEPARATOR:
            return "Using RIGOR_RESERVED_LEGACY_CODESEPARATOR in non-witness script";
        case SCRIPT_ERR_SIG_FINDANDDELETE:
            return "Signature is found in scriptCode";
        case SCRIPT_ERR_SECP256K1_DISABLED:
            return "secp256k1/ECDSA is disabled; RIGOR spend signatures are Ed25519 only";
        case SCRIPT_ERR_CONVERGE_UNSPENDABLE:
            return "RIGOR_MARKER_CONVERGE output is unspendable";
        case SCRIPT_ERR_RIGOR_SCRIPT_TYPE:
            return "Script type is not a RIGOR consensus lock";
        case SCRIPT_ERR_RIGOR_CLASSIFIER_DIVERGENCE:
            return "Rigor predicate classifier diverged from the production boundary";
        case SCRIPT_ERR_UNKNOWN_ERROR:
        case SCRIPT_ERR_ERROR_COUNT:
        default: break;
    }
    return "unknown error";
}
