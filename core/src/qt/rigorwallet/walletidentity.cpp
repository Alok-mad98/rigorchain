// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/rigor-config.h>
#endif

#include <qt/rigorwallet/walletidentity.h>

#include <chainparams.h>
#include <chainparamsbase.h>
#include <clientversion.h>

#ifdef HAVE_BUILD_INFO
#include <obj/build.h>
#endif

#include <QCoreApplication>
#include <QString>

namespace RigorWallet {

BrandInfo WalletIdentity::brand()
{
    BrandInfo info;
    info.productName = QStringLiteral("Rigor Wallet");
    info.applicationName = QStringLiteral("RIGOR-Wallet");
    info.organizationName = QStringLiteral("RIGOR");
    info.organizationDomain = QStringLiteral("rigor.org");
    info.protocolName = QStringLiteral("RIGOR Protocol");
    info.stateMachineLabel = QStringLiteral("Deterministic State Machine");
    info.verificationStatus = QStringLiteral("Experimental Verification In Progress");
    info.equivalenceClaimed = false;
    return info;
}

VersionInfo WalletIdentity::version()
{
    VersionInfo info;
    info.fullVersion = QString::fromUtf8(RIGOR_RELEASE_VERSION);
    info.clientName = QString::fromStdString(CLIENT_NAME);
    info.clientVersion = CLIENT_VERSION;
    return info;
}

BuildInfo WalletIdentity::build()
{
    BuildInfo info;
    info.isReleaseBuild = CLIENT_VERSION_IS_RELEASE;
#if defined(BUILD_GIT_COMMIT)
    info.buildSuffix = QStringLiteral(BUILD_GIT_COMMIT);
#elif defined(GIT_COMMIT_ID)
    info.buildSuffix = QStringLiteral(GIT_COMMIT_ID);
#else
    info.buildSuffix = QString();
#endif
    return info;
}

QString WalletIdentity::networkDisplayName()
{
    const std::string id = Params().NetworkIDString();
    if (id == CBaseChainParams::TESTNET) {
        return QCoreApplication::translate("WalletIdentity", "RIGOR Testnet");
    }
    if (id == CBaseChainParams::REGTEST) {
        return QCoreApplication::translate("WalletIdentity", "RIGOR Regtest");
    }
    if (id == CBaseChainParams::DEVTEST) {
        return QCoreApplication::translate("WalletIdentity", "RIGOR Devtest");
    }
    if (id == CBaseChainParams::SLOWTEST) {
        return QCoreApplication::translate("WalletIdentity", "RIGOR Slowtest");
    }
    return QCoreApplication::translate("WalletIdentity", "RIGOR Mainnet");
}

QString WalletIdentity::equivalenceDisclaimer()
{
    return QCoreApplication::translate("WalletIdentity",
        "This wallet does not claim equivalence with any other network or ledger.");
}

QString WalletIdentity::aboutComplianceHtml()
{
    const BrandInfo b = brand();
    const VersionInfo v = version();
    const BuildInfo buildInfo = WalletIdentity::build();

    QString html;
    html += QStringLiteral("<p><b>%1</b></p>").arg(b.productName.toHtmlEscaped());
    html += QStringLiteral("<p>%1</p>").arg(QCoreApplication::translate(
        "WalletIdentity",
        "A sovereign Bitcoin-derived blockchain wallet.").toHtmlEscaped());

    html += QStringLiteral("<p><b>%1</b></p>").arg(QCoreApplication::translate(
        "WalletIdentity", "Two native assets:").toHtmlEscaped());
    html += QStringLiteral("<p><b>ORU</b><br>%1</p>").arg(QCoreApplication::translate(
        "WalletIdentity",
        "Proof-of-work security and economic coordination asset.").toHtmlEscaped());
    html += QStringLiteral("<p><b>RES</b><br>%1</p>").arg(QCoreApplication::translate(
        "WalletIdentity",
        "Independent scarce resource.").toHtmlEscaped());
    html += QStringLiteral("<p>%1</p>").arg(QCoreApplication::translate(
        "WalletIdentity",
        "RES is minted through deterministic consensus allocation. RES is not mined.").toHtmlEscaped());
    html += QStringLiteral("<p><i>%1</i></p>").arg(QCoreApplication::translate(
        "WalletIdentity",
        "The script is the compliance layer.").toHtmlEscaped());

    html += QStringLiteral("<hr>");
    html += QStringLiteral("<p><b>%1</b> %2</p>")
                .arg(QCoreApplication::translate("WalletIdentity", "Version").toHtmlEscaped(),
                     QString::fromUtf8(RIGOR_RELEASE_VERSION).toHtmlEscaped());
    {
        const QString copy = QString(QChar(0x00A9));
        html += QStringLiteral("<p>Copyright %1 2009-2026 The Bitcoin Core developers</p>").arg(copy);
        html += QStringLiteral("<p>Copyright %1 2026- The RIGOR developers</p>").arg(copy);
    }
    html += QStringLiteral("<p>%1</p>").arg(QCoreApplication::translate(
        "WalletIdentity",
        "License: MIT").toHtmlEscaped());

    {
        const QString releases_url = QStringLiteral("https://github.com/rigorchain/rigorchain/releases");
        html += QStringLiteral("<p><b>%1</b><br><a href=\"%2\">%2</a></p>")
                    .arg(QCoreApplication::translate("WalletIdentity", "GitHub releases").toHtmlEscaped(),
                         releases_url.toHtmlEscaped());
    }

    html += QStringLiteral("<p>%1</p>").arg(equivalenceDisclaimer().toHtmlEscaped());

    html += QStringLiteral("<p>%1</p>").arg(v.clientName.toHtmlEscaped());

    if (!buildInfo.buildSuffix.isEmpty()) {
        html += QStringLiteral("<p><small>Build: %1</small></p>").arg(buildInfo.buildSuffix.toHtmlEscaped());
    }

    return html;
}

} // namespace RigorWallet
