// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_PSBT_FORMAT_H
#define BITCOIN_QT_PSBT_FORMAT_H

#include <consensus/amount.h>
#include <node/psbt.h>
#include <primitives/transaction.h>
#include <psbt.h>
#include <qt/bitcoinunits.h>
#include <wallet/psbt_domain.h>

#include <QString>

namespace PsbtFormat {

QString AssetDomainName(AssetType asset);
QString FormatPsbtAmountForAsset(AssetType asset, const CAmount& amount, BitcoinUnit oru_display_unit);
QString FormatPsbtAmountHtmlForAsset(AssetType asset, const CAmount& amount, BitcoinUnit oru_display_unit);
QString FormatPsbtFeeOru(const CAmount& fee_oru, BitcoinUnit oru_display_unit);
QString FormatPsbtFeeOruHtml(const CAmount& fee_oru, BitcoinUnit oru_display_unit);
QString RenderPsbtHtmlSummary(const PartiallySignedTransaction& psbt, const node::PSBTAnalysis& analysis,
    BitcoinUnit oru_display_unit);

} // namespace PsbtFormat

#endif // BITCOIN_QT_PSBT_FORMAT_H
