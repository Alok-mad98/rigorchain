// Copyright (c) 2011-2021 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/receiverequestdialog.h>
#include <qt/forms/ui_receiverequestdialog.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/qrimagewidget.h>
#include <qt/walletmodel.h>

#include <QDialog>
#include <QString>

#if defined(HAVE_CONFIG_H)
#include <config/rigor-config.h> /* for USE_QRCODE */
#endif

ReceiveRequestDialog::ReceiveRequestDialog(QWidget* parent)
    : QDialog(parent, GUIUtil::dialog_flags),
      ui(new Ui::ReceiveRequestDialog)
{
    ui->setupUi(this);
    GUIUtil::ApplyRigorDialogChrome(this);
    GUIUtil::handleCloseWindowShortcut(this);
}

ReceiveRequestDialog::~ReceiveRequestDialog()
{
    delete ui;
}

void ReceiveRequestDialog::setModel(WalletModel *_model)
{
    this->model = _model;

    if (_model)
        connect(_model->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &ReceiveRequestDialog::updateDisplayUnit);

    // update the display unit if necessary
    update();
}

void ReceiveRequestDialog::setInfo(const SendCoinsRecipient &_info)
{
    this->info = _info;
    setWindowTitle(tr("Request payment to %1").arg(info.label.isEmpty() ? info.address : info.label));
    QString uri = GUIUtil::formatBitcoinURI(info);

#ifdef USE_QRCODE
    if (ui->qr_code->setQR(uri, info.address)) {
        connect(ui->btnSaveAs, &QPushButton::clicked, ui->qr_code, &QRImageWidget::saveImage);
    } else {
        ui->btnSaveAs->setEnabled(false);
    }
#else
    ui->btnSaveAs->hide();
    ui->qr_code->hide();
#endif

    ui->uri_content->setText("<a href=\"" + uri + "\">" + GUIUtil::HtmlEscape(uri) + "</a>");
    ui->address_content->setText(info.address);
    switch (GUIUtil::ClassifyNativeReceiveAddress(info.address)) {
    case GUIUtil::NativeReceiveAssetKind::ORU:
        ui->address_tag->setText(tr("Address (ORU):"));
        ui->address_content->setToolTip(tr(
            "Native Segwit v0 (Bech32), ORU on this chain. Encoded addresses contain \"…1q…\" after the human-readable prefix (BIP-173 witness version). "
            "The wallet classifies by decoding the address, not by guessing from text."));
        break;
    case GUIUtil::NativeReceiveAssetKind::RES:
        ui->address_tag->setText(tr("Address (Res):"));
        ui->address_content->setToolTip(tr(
            "Native Segwit v1 (Bech32m), Res on this chain. Encoded addresses contain \"…1p…\" after the human-readable prefix (BIP-350). "
            "Uses Witness v1 Res (ORB2-tagged) script semantics. The wallet classifies by decoding the address."));
        break;
    default:
        ui->address_tag->setText(tr("Address:"));
        ui->address_content->setToolTip({});
        break;
    }

    if (!info.amount) {
        ui->amount_tag->hide();
        ui->amount_content->hide();
    } // Amount is set in updateDisplayUnit() slot.
    updateDisplayUnit();

    if (!info.label.isEmpty()) {
        ui->label_content->setText(info.label);
    } else {
        ui->label_tag->hide();
        ui->label_content->hide();
    }

    if (!info.message.isEmpty()) {
        ui->message_content->setText(info.message);
    } else {
        ui->message_tag->hide();
        ui->message_content->hide();
    }

    if (!model->getWalletName().isEmpty()) {
        ui->wallet_content->setText(model->getWalletName());
    } else {
        ui->wallet_tag->hide();
        ui->wallet_content->hide();
    }

    ui->btnVerify->setVisible(model->wallet().hasExternalSigner());

    connect(ui->btnVerify, &QPushButton::clicked, [this] {
        model->displayAddress(info.address.toStdString());
    });
}

void ReceiveRequestDialog::updateDisplayUnit()
{
    if (!model) return;
    const AssetType asset = info.asset.value_or(ASSET_ORU);
    ui->amount_content->setText(BitcoinUnits::formatAmountForAsset(
        asset, info.amount, model->getOptionsModel()->getDisplayUnit(), false,
        BitcoinUnits::SeparatorStyle::STANDARD, true));
}

void ReceiveRequestDialog::on_btnCopyURI_clicked()
{
    GUIUtil::setClipboard(GUIUtil::formatBitcoinURI(info));
}

void ReceiveRequestDialog::on_btnCopyAddress_clicked()
{
    GUIUtil::setClipboard(info.address);
}
