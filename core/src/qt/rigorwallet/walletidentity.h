// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WALLETIDENTITY_H
#define BITCOIN_QT_RIGORWALLET_WALLETIDENTITY_H

#include <QString>

namespace RigorWallet {

/** Product and compliance branding constants. */
struct BrandInfo {
    QString productName;          //!< "Rigor Wallet"
    QString applicationName;      //!< QSettings / desktop entry name
    QString organizationName;     //!< "RIGOR"
    QString organizationDomain;   //!< "rigor.org"
    QString protocolName;         //!< "RIGOR Protocol"
    QString stateMachineLabel;    //!< "Deterministic State Machine"
    QString verificationStatus;   //!< "Experimental Verification In Progress"
    bool equivalenceClaimed;      //!< always false for this product line
};

/** Runtime version strings from client build. */
struct VersionInfo {
    QString fullVersion;          //!< e.g. "Rigor Core version x.y.z"
    QString clientName;
    int clientVersion;            //!< packed CLIENT_VERSION integer
};

/** Build metadata (git suffix, release flag). */
struct BuildInfo {
    QString buildSuffix;          //!< git commit suffix when present
    bool isReleaseBuild;
};

/** Centralized wallet identity accessor — single source for UI branding. */
class WalletIdentity
{
public:
    WalletIdentity() = delete;

    static BrandInfo brand();
    static VersionInfo version();
    static BuildInfo build();

    /** HTML block for About dialog compliance strip. */
    static QString aboutComplianceHtml();

    /** Plain-text equivalence disclaimer. */
    static QString equivalenceDisclaimer();

    /** User-facing chain label (Mainnet / Testnet / …). Not a consensus value. */
    static QString networkDisplayName();
};

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_WALLETIDENTITY_H
