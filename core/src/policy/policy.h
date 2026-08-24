// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POLICY_POLICY_H
#define BITCOIN_POLICY_POLICY_H

#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/standard.h>

#include <cstdint>
#include <string>

#include <consensus/validation.h>

class CCoinsViewCache;
class CFeeRate;
class CScript;
class CTxMemPool;

/** Mempool min-fee / min-relay checks for ORU network fees (POOL-F). All network fees are ORU-only. */
namespace Policy {
bool MinFeeMet_ORU(const CTxMemPool& pool, size_t package_size, CAmount package_fee, TxValidationState& state);
} // namespace Policy

/** Default for -blockmaxweight, which controls the range of block weights the mining code will create **/
static constexpr unsigned int DEFAULT_BLOCK_MAX_WEIGHT{MAX_BLOCK_WEIGHT - 4000};
/** Default for -blockmintxfee, which sets the minimum feerate for a transaction in blocks created by mining code **/
static constexpr unsigned int DEFAULT_BLOCK_MIN_TX_FEE{1000};
/** The maximum weight for transactions we're willing to relay/mine */
static constexpr unsigned int MAX_STANDARD_TX_WEIGHT{400000};
/** The minimum non-witness size for transactions we're willing to relay/mine: one larger than 64  */
static constexpr unsigned int MIN_STANDARD_TX_NONWITNESS_SIZE{65};
/** Maximum number of signature check operations in an IsStandard() P2SH script */
static constexpr unsigned int MAX_P2SH_SIGOPS{15};
/** The maximum number of sigops we're willing to relay/mine in a single tx */
static constexpr unsigned int MAX_STANDARD_TX_SIGOPS_COST{MAX_BLOCK_SIGOPS_COST/5};
/** Default for -incrementalrelayfee, which sets the minimum feerate increase for mempool limiting or replacement **/
static constexpr unsigned int DEFAULT_INCREMENTAL_RELAY_FEE{1000};
/** Default for -bytespersigop */
static constexpr unsigned int DEFAULT_BYTES_PER_SIGOP{20};
/** Default for -permitbaremultisig */
static constexpr bool DEFAULT_PERMIT_BAREMULTISIG{true};
/** The maximum number of witness stack items in a standard P2WSH script */
static constexpr unsigned int MAX_STANDARD_P2WSH_STACK_ITEMS{100};
/** The maximum size in bytes of each witness stack item in a standard P2WSH script */
static constexpr unsigned int MAX_STANDARD_P2WSH_STACK_ITEM_SIZE{80};
/** The maximum size in bytes of each witness stack item in a standard script */
// REMOVED: BIP 342 Taproot reference (leaf version 0xc0)
// violates RIGOR "Script as Asset" constitution (no Taproot/BIP341 complexity)
static constexpr unsigned int MAX_STANDARD_WITNESS_SCRIPT_STACK_ITEM_SIZE{80};
/** The maximum size in bytes of a standard witnessScript */
static constexpr unsigned int MAX_STANDARD_P2WSH_SCRIPT_SIZE{3600};
/** The maximum size of a standard ScriptSig */
static constexpr unsigned int MAX_STANDARD_SCRIPTSIG_SIZE{1650};
/** Min feerate for defining dust.
 * Changing the dust limit changes which transactions are
 * standard and should be done with care and ideally rarely. It makes sense to
 * only increase the dust limit after prior releases were already not creating
 * outputs below the new threshold */
static constexpr unsigned int DUST_RELAY_TX_FEE{3000};
/** Default for -minrelaytxfee, minimum relay fee for transactions */
static constexpr unsigned int DEFAULT_MIN_RELAY_TX_FEE{1000};
/** Default for -limitancestorcount, max number of in-mempool ancestors */
static constexpr unsigned int DEFAULT_ANCESTOR_LIMIT{25};
/** Default for -limitancestorsize, maximum kilobytes of tx + all in-mempool ancestors */
static constexpr unsigned int DEFAULT_ANCESTOR_SIZE_LIMIT_KVB{101};
/** Default for -limitdescendantcount, max number of in-mempool descendants */
static constexpr unsigned int DEFAULT_DESCENDANT_LIMIT{25};
/** Default for -limitdescendantsize, maximum kilobytes of in-mempool descendants */
static constexpr unsigned int DEFAULT_DESCENDANT_SIZE_LIMIT_KVB{101};
/**
 * An extra transaction can be added to a package, as long as it only has one
 * ancestor and is no larger than this. Not really any reason to make this
 * configurable as it doesn't materially change DoS parameters.
 */
static constexpr unsigned int EXTRA_DESCENDANT_TX_SIZE_LIMIT{10000};
/**
 * Standard script verification flags that standard transactions will comply
 * with. However scripts violating these flags may still be present in valid
 * blocks and we must accept those blocks.
 */
static constexpr unsigned int STANDARD_SCRIPT_VERIFY_FLAGS{MANDATORY_SCRIPT_VERIFY_FLAGS |
                                                             SCRIPT_VERIFY_DERSIG |
                                                             SCRIPT_VERIFY_STRICTENC |
                                                             SCRIPT_VERIFY_MINIMALDATA |
                                                             SCRIPT_VERIFY_NULLDUMMY |
                                                             SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS |
                                                             SCRIPT_VERIFY_CLEANSTACK |
                                                             SCRIPT_VERIFY_MINIMALIF |
                                                             SCRIPT_VERIFY_NULLFAIL |
                                                             SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY |
                                                             SCRIPT_VERIFY_CHECKSEQUENCEVERIFY |
                                                             SCRIPT_VERIFY_LOW_S |
                                                             SCRIPT_VERIFY_WITNESS |
                                                             SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM |
                                                             SCRIPT_VERIFY_WITNESS_PUBKEYTYPE |
                                                             SCRIPT_VERIFY_CONST_SCRIPTCODE |
                                                             SCRIPT_VERIFY_DISCOURAGE_OP_SUCCESS |
                                                             SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_PUBKEYTYPE};

/** For convenience, standard but not mandatory verify flags. */
static constexpr unsigned int STANDARD_NOT_MANDATORY_VERIFY_FLAGS{STANDARD_SCRIPT_VERIFY_FLAGS & ~MANDATORY_SCRIPT_VERIFY_FLAGS};

/** Used as the flags parameter to sequence and nLocktime checks in non-consensus code. */
static constexpr unsigned int STANDARD_LOCKTIME_VERIFY_FLAGS{LOCKTIME_VERIFY_SEQUENCE};

CAmount GetDustThreshold(const CTxOut& txout, const CFeeRate& dustRelayFee);

bool IsDust(const CTxOut& txout, const CFeeRate& dustRelayFee);

bool IsStandard(const CScript& scriptPubKey, const std::optional<unsigned>& max_datacarrier_bytes, TxoutType& whichType);


// Changing the default transaction version requires a two step process: first
// adapting relay policy by bumping TX_MAX_STANDARD_VERSION, and then later
// allowing the new transaction version in the wallet/RPC.
static constexpr decltype(CTransaction::nVersion) TX_MAX_STANDARD_VERSION{2};

/**
* Check for standard transaction types
* @return True if all outputs (scriptPubKeys) use only standard transaction forms
*/
bool IsStandardTx(const CTransaction& tx, const std::optional<unsigned>& max_datacarrier_bytes, bool permit_bare_multisig, const CFeeRate& dust_relay_fee, std::string& reason);
/**
* Check for standard transaction types
* @param[in] mapInputs       Map of previous transactions that have outputs we're spending
* @return True if all inputs (scriptSigs) use only standard transaction forms
*/
bool AreInputsStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs);
/**
* Check if the transaction is over standard P2WSH resources limit:
* 3600bytes witnessScript size, 80bytes per witness stack element, 100 witness stack elements
* These limits are adequate for multisignatures up to n-of-100 using OP_CHECKSIG, OP_ADD, and OP_EQUAL.
*
* For witness v1 RES, enforce stack shapes that match consensus (RES2-prefixed and native: 2-item Ed25519 key path).
* The historical 11-item PCORB path is not consensus and is not standard.
*/
bool IsWitnessStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs);

// RIGOR Fee Mechanism: GetVirtualTransactionSize is DELETED
// Use rigor::GetTransactionSizeBytes instead (exact byte count, NO weight/vsize)

#endif // BITCOIN_POLICY_POLICY_H
