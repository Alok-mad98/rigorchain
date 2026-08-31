// Copyright (c) 2009-2018 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/uritests.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/psbt_format.h>
#include <consensus/res_amount.h>
#include <qt/walletmodel.h>
#include <wallet/psbt_domain.h>

#include <array>
#include <key.h>
#include <key_io.h>
#include <script/standard.h>
#include <primitives/transaction.h>
#include <pubkey.h>

#include <QUrl>

namespace {

QString RegtestOruAddr()
{
    uint160 id;
    id.SetHex("6f58bac9506f9bfb821d62e69330410bb56f0085");
    return QString::fromStdString(EncodeDestination(WitnessV0KeyHash(id)));
}

QString RegtestResAddr()
{
    // IsValidDestination(WitnessV1Res) requires a canonical on-curve Ed25519
    // program (TASK-010 R-2). A hardcoded 32-byte hex is not a spendable RES key.
    CKey key;
    key.MakeNewKey(true);
    const std::vector<unsigned char> pk32 = key.GetPubKey().GetEd25519Payload();
    return QString::fromStdString(EncodeDestination(WitnessV1Res(XOnlyPubKey(pk32))));
}

} // namespace

void URITests::uriTests()
{
    const QString oru = RegtestOruAddr();
    const QString res = RegtestResAddr();

    SendCoinsRecipient rv;
    QUrl uri;
    uri.setUrl(QString("rigor:%1?req-dontexist=").arg(oru));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("rigor:%1?dontexist=&asset=oru").arg(oru));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == oru);
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 0);
    QVERIFY(rv.asset.has_value() && *rv.asset == ASSET_ORU);

    uri.setUrl(QString("rigor:%1?label=Wikipedia Example Address&asset=oru").arg(oru));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == oru);
    QVERIFY(rv.label == QString("Wikipedia Example Address"));
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("rigor:%1?amount=0.001&asset=oru").arg(oru));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == oru);
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100000);

    uri.setUrl(QString("rigor:%1?amount=1.001&asset=oru").arg(oru));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == oru);
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100100000);

    uri.setUrl(QString("rigor:%1?amount=100&label=Wikipedia Example&asset=oru").arg(oru));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == oru);
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("Wikipedia Example"));

    uri.setUrl(QString("rigor:%1?message=Wikipedia Example Address&asset=oru").arg(oru));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == oru);
    QVERIFY(rv.label == QString());

    QVERIFY(GUIUtil::parseBitcoinURI(QString("rigor:%1?message=Wikipedia Example Address&asset=oru").arg(oru), &rv));
    QVERIFY(rv.address == oru);
    QVERIFY(rv.label == QString());

    uri.setUrl(QString("rigor:%1?req-message=Wikipedia Example Address&asset=oru").arg(oru));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("rigor:%1?amount=1,000&label=Wikipedia Example&asset=oru").arg(oru));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("rigor:%1?amount=1,000.0&label=Wikipedia Example&asset=oru").arg(oru));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    // Missing asset= is rejected
    uri.setUrl(QString("rigor:%1?label=x").arg(oru));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    // Conflicting duplicate asset=
    uri.setUrl(QString("rigor:%1?asset=oru&asset=res").arg(oru));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    // asset=res with witness-v0 ORU address
    uri.setUrl(QString("rigor:%1?asset=res").arg(oru));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    // asset=oru with RES witness address
    uri.setUrl(QString("rigor:%1?asset=oru").arg(res));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    // RES amount uses RES scale
    uri.setUrl(QString("rigor:%1?amount=1.01&asset=res").arg(res));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.asset.has_value() && *rv.asset == ASSET_RES);
    QVERIFY(rv.amount > 0);
}

void URITests::assetAmountFormatTests()
{
    const CAmount oru_amt = COIN;
    const CAmount res_amt = RES_COIN;
    const QString oru_fmt = BitcoinUnits::formatAmountForAsset(
        ASSET_ORU, oru_amt, BitcoinUnit::ORU, false, BitcoinUnits::SeparatorStyle::NEVER, true);
    QVERIFY(oru_fmt.contains(QStringLiteral("ORU")));
    const QString res_fmt = BitcoinUnits::formatAmountForAsset(
        ASSET_RES, res_amt, BitcoinUnit::ORU, false, BitcoinUnits::SeparatorStyle::NEVER, true);
    QVERIFY(res_fmt.contains(QStringLiteral("RES")));
    QVERIFY(!res_fmt.contains(QStringLiteral("ORU")));
    const QString fee_fmt = BitcoinUnits::formatFeeOru(oru_amt, BitcoinUnit::ORU);
    QVERIFY(fee_fmt.contains(QStringLiteral("ORU")));
}

void URITests::psbtFormatTests()
{
    QVERIFY(PsbtFormat::FormatPsbtFeeOru(COIN, BitcoinUnit::ORU).contains(QStringLiteral("ORU")));
    CKey key;
    key.MakeNewKey(true);
    const CScript oru_spk = GetScriptForDestination(WitnessV0KeyHash(key.GetPubKey().GetID()));
    QVERIFY(PsbtFormat::FormatPsbtAmountForAsset(ASSET_ORU, COIN, BitcoinUnit::ORU).contains(QStringLiteral("ORU")));
    std::array<unsigned char, 32> res_prog{};
    res_prog.fill(0x11);
    const CScript res_spk = GetScriptForDestination(WitnessV1Res{XOnlyPubKey{res_prog}});
    QVERIFY(PsbtFormat::FormatPsbtAmountForAsset(ASSET_RES, RES_COIN, BitcoinUnit::ORU).contains(QStringLiteral("RES")));
    const std::string dest = wallet::PsbtOutputDestinationSummary(res_spk);
    QVERIFY(!QString::fromStdString(dest).contains(QStringLiteral("Taproot")));
}
