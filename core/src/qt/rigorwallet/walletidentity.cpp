// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/rigor-config.h>
#endif

#include <qt/rigorwallet/walletidentity.h>

#include <clientversion.h>

#ifdef HAVE_BUILD_INFO
#include <obj/build.h>
#endif

#include <QCoreApplication>
#include <QString>

#include <QCoreApplication>

namespace RigorWallet {

BrandInfo WalletIdentity::brand()
{
    BrandInfo info;
    info.productName = QStringLiteral("RIGOR Wallet");
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
    info.fullVersion = QString::fromStdString(FormatFullVersion());
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
    html += QStringLiteral("<hr>");
    html += QStringLiteral("<p><b>%1</b></p>").arg(b.protocolName.toHtmlEscaped());
    html += QStringLiteral("<p>%1</p>").arg(b.stateMachineLabel.toHtmlEscaped());
    html += QStringLiteral("<p><i>%1</i></p>").arg(b.verificationStatus.toHtmlEscaped());
    html += QStringLiteral("<p>%1</p>").arg(equivalenceDisclaimer().toHtmlEscaped());

    html += QStringLiteral("<p>%1<br>%2</p>")
                .arg(QString::fromStdString(FormatFullVersion()).toHtmlEscaped(),
                     v.clientName.toHtmlEscaped());

    if (!buildInfo.buildSuffix.isEmpty()) {
        html += QStringLiteral("<p><small>Build: %1</small></p>").arg(buildInfo.buildSuffix.toHtmlEscaped());
    }

    html += QStringLiteral("<p><small>");
    html += QStringLiteral("<a href=\"https://github.com/rigor-chain/rigor/blob/master/docs/rfcs/RFC-000-consensus-constitution.md\">RFC-000</a> · ");
    html += QStringLiteral("<a href=\"https://github.com/rigor-chain/rigor/blob/master/docs/rfcs/RFC-000A-rigor-implementation-bible.md\">RFC-000A</a> · ");
    html += QStringLiteral("<a href=\"https://github.com/rigor-chain/rigor/blob/master/docs/rfcs/RFC-001-deterministic-selection.md\">RFC-001</a> · ");
    html += QStringLiteral("<a href=\"https://github.com/rigor-chain/rigor/blob/master/docs/rfcs/RFC-002-supply-evolution.md\">RFC-002</a>");
    html += QStringLiteral("</small></p>");

    return html;
}

} // namespace RigorWallet
