// Copyright (c) 2012-2020 The Bitcoin Core developers
// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VERSION_H
#define BITCOIN_VERSION_H

/**
 * Serialization stream version (historical Bitcoin Core value).
 * Used by CDataStream / GetSerializeSize / CNode::GetCommonVersion after VERSION.
 * NOT a P2P feature gate. Must not collide with SERIALIZE_TRANSACTION_NO_WITNESS
 * or ADDRV2_FORMAT. Never store RIGOR_PROTOCOL_* in the stream nVersion.
 *
 * PROTOCOL_VERSION is the serialization constant. SERIALIZATION_VERSION is the
 * RIGOR name for the same value. Feature enablement is HasCapability().
 */
static const int PROTOCOL_VERSION = 70016;
static const int SERIALIZATION_VERSION = PROTOCOL_VERSION;

//! Initial stream version used to serialize the VERSION message itself
//! (historical Bitcoin Core handshake framing). Increased after version/verack.
static const int INIT_PROTO_VERSION = 209;

/**
 * RIGOR P2P protocol versions (capability negotiation).
 *
 * Feature differences are peer.HasCapability(), never `if (peerVersion >= X)`.
 * MIN <= CURRENT is the accepted VERSION.nVersion range. Raise CURRENT
 * (and MIN if dropping old peers) on a protocol upgrade. There is no
 * spare ceiling above CURRENT.
 */
static const int RIGOR_PROTOCOL_MIN_VERSION = 1;
static const int RIGOR_PROTOCOL_CURRENT_VERSION = 1;

static_assert(RIGOR_PROTOCOL_MIN_VERSION <= RIGOR_PROTOCOL_CURRENT_VERSION,
              "RIGOR protocol min cannot exceed current");
static_assert(RIGOR_PROTOCOL_MIN_VERSION != PROTOCOL_VERSION,
              "P2P min version must not be the Bitcoin serialization constant");
static_assert(RIGOR_PROTOCOL_CURRENT_VERSION != PROTOCOL_VERSION,
              "P2P current version must not be the Bitcoin serialization constant");
static_assert(PROTOCOL_VERSION > RIGOR_PROTOCOL_CURRENT_VERSION,
              "Bitcoin serialization constant must be rejected as a P2P version");

#endif // BITCOIN_VERSION_H
