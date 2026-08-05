// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_UI_READINESS_H
#define BITCOIN_QT_RIGORWALLET_UI_READINESS_H

namespace RigorWallet {

/** Set true when PROGRAM-017 release UI checks pass. Does not affect consensus. */
constexpr bool UI_RELEASE_CANDIDATE_READY = true;

/** Set true when PROGRAM-022 testnet candidate freeze is active. Presentation-only. */
constexpr bool FINAL_FREEZE_MODE = true;

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_UI_READINESS_H
