// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/psbt_format.h>

#include <qt/guiutil.h>

#include <QCoreApplication>

namespace PsbtFormat {

QString AssetDomainName(AssetType asset)
{
    return asset == ASSET_RES ? QStringLiteral("RES") : QStringLiteral("ORU");
}

QString FormatPsbtAmountForAsset(AssetType asset, const CAmount& amount, BitcoinUnit oru_display_unit)
{
    return BitcoinUnits::formatAmountForAsset(asset, amount, oru_display_unit, false,
        BitcoinUnits::SeparatorStyle::STANDARD, true);
}

QString FormatPsbtAmountHtmlForAsset(AssetType asset, const CAmount& amount, BitcoinUnit oru_display_unit)
{
    return BitcoinUnits::formatAmountHtmlForAsset(asset, amount, oru_display_unit);
}

QString FormatPsbtFeeOru(const CAmount& fee_oru, BitcoinUnit oru_display_unit)
{
    return BitcoinUnits::formatFeeOru(fee_oru, oru_display_unit);
}

QString FormatPsbtFeeOruHtml(const CAmount& fee_oru, BitcoinUnit oru_display_unit)
{
    return BitcoinUnits::formatFeeOruHtml(fee_oru, oru_display_unit);
}

QString RenderPsbtHtmlSummary(const PartiallySignedTransaction& psbt, const node::PSBTAnalysis& analysis,
    BitcoinUnit oru_display_unit)
{
    const wallet::PsbtDomainSummary dom = wallet::BuildPsbtDomainSummary(psbt, analysis);
    QString html;

    html += QStringLiteral("<b>%1</b> ").arg(QCoreApplication::translate("PsbtFormat", "Input asset domain:"));
    if (dom.mixed_input_domains) {
        html += QCoreApplication::translate("PsbtFormat", "Mixed (cannot infer single domain)");
    } else if (dom.unified_input_domain.has_value()) {
        html += AssetDomainName(*dom.unified_input_domain);
    } else if (!dom.input_domains.empty()) {
        QStringList parts;
        for (const AssetType a : dom.input_domains) {
            parts.append(AssetDomainName(a));
        }
        html += parts.join(QStringLiteral(", "));
    } else {
        html += QCoreApplication::translate("PsbtFormat", "Unknown (missing input UTXO data)");
    }
    html += QStringLiteral("<br>");

    html += QStringLiteral("<b>%1</b> %2<br>")
                .arg(QCoreApplication::translate("PsbtFormat", "Fee asset:"),
                    AssetDomainName(dom.fee_asset));

    for (const CTxOut& out : psbt.tx->vout) {
        const AssetType out_asset = wallet::PsbtTransferAssetFromScript(out.scriptPubKey);
        const QString dest = QString::fromStdString(wallet::PsbtOutputDestinationSummary(out.scriptPubKey));
        html += QStringLiteral(" * %1 %2 %3<br>")
                    .arg(QCoreApplication::translate("PsbtFormat", "Transfer"))
                    .arg(FormatPsbtAmountHtmlForAsset(out_asset, out.nValue, oru_display_unit))
                    .arg(GUIUtil::HtmlEscape(dest));
    }

    html += QStringLiteral("<hr />");
    if (!dom.fee_oru.has_value()) {
        html += QCoreApplication::translate("PsbtFormat",
            "Unable to calculate network fee (ORU): missing input UTXO data or mixed input asset domains.");
    } else {
        html += QStringLiteral("<b>%1</b> ")
                    .arg(QCoreApplication::translate("PsbtFormat", "Network fee (ORU)"));
        html += FormatPsbtFeeOruHtml(*dom.fee_oru, oru_display_unit);
    }

    html += QStringLiteral("<br><span style='font-size:9pt;'>");
    html += QCoreApplication::translate("PsbtFormat",
        "Semantic authority is input asset domain + StateTag (RFC-003), not witness layout or output majority.");
    html += QStringLiteral("</span>");

    return html;
}

} // namespace PsbtFormat
