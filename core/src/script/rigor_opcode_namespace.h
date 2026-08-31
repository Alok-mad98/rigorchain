// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_RIGOR_OPCODE_NAMESPACE_H
#define BITCOIN_SCRIPT_RIGOR_OPCODE_NAMESPACE_H

#include <script/rigor_bytes.h>

/**
 * P0-027: RIGOR native opcode namespace.
 *
 * Bytes keep their historic numeric values (ScriptPubKey compatibility).
 * None of them is an instruction. There is no evaluator and no Script VM;      script-vm-allow
 * test/lint/lint-no-script-vm.py fails the build if one reappears.
 * Consensus spend uses the predicate registry, not this enum.
 *
 * Frozen: 0x6a RETURN, 0xbb RES mint, 0xbd CONVERGE, 0xbc burned,
 * 0xac RIGOR_RESERVED_LEGACY_CHECKSIG (inert).
 */
enum class RigorOpcodeClass {
    PUSH_MARKER,
    ASSET_MARKER,
    RESERVED_INERT,
    BURNED,
    INVALID
};

static_assert(static_cast<unsigned>(RIGOR_RESERVED_LEGACY_CHECKSIG) == 0xacu);
static_assert(static_cast<unsigned>(RIGOR_RESERVED_LEGACY_CHECKMULTISIG) == 0xaeu);
static_assert(static_cast<unsigned>(RIGOR_RESERVED_LEGACY_HASH160) == 0xa9u);
static_assert(static_cast<unsigned>(RIGOR_MARKER_RETURN) == 0x6au);
static_assert(static_cast<unsigned>(RIGOR_MARKER_RES_MINT) == 0xbbu);
static_assert(static_cast<unsigned>(RIGOR_MARKER_CONVERGE) == 0xbdu);
static_assert(static_cast<unsigned>(RIGOR_BURNED_CVG_CLAIM) == 0xbcu);

/**
 * Total over all 256 byte values. Every class is inert: none of them names an
 * operation, so there is no `IsRigorInstruction` predicate to consult and no
 * class this function could return that would make a byte executable.
 *
 * The absence of a VM is enforced by test/lint/lint-no-script-vm.py, which
 * fails the build if evaluator symbols reappear under core/src/script/. A
 * function that returns a constant `false` cannot enforce it: nothing a future
 * commit adds would make such a function start returning true.
 */
RigorOpcodeClass ClassifyRigorOpcode(unsigned int b);

#endif // BITCOIN_SCRIPT_RIGOR_OPCODE_NAMESPACE_H
