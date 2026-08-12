// Copyright (c) 2026 The RIGOR developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/rigor-config.h>
#endif

#include <qt/keymanagementdialog.h>

#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/platformstyle.h>
#include <qt/walletmodel.h>

#include <interfaces/node.h>
#include <rpc/protocol.h>
#include <univalue.h>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {

std::string WalletRpcUri(WalletModel* model)
{
    if (!model) return "/";
    const QByteArray encoded = QUrl::toPercentEncoding(model->getWalletName());
    return "/wallet/" + std::string(encoded.constData(), encoded.length());
}

QString RpcErrorMessage(const std::exception& e)
{
    return QString::fromStdString(e.what());
}

} // namespace

KeyManagementDialog::KeyManagementDialog(const PlatformStyle* platformStyle, QWidget* parent)
    : QDialog(parent, GUIUtil::dialog_flags),
      m_platform_style(platformStyle)
{
    setWindowTitle(tr("Key Management"));
    resize(640, 480);
    GUIUtil::ApplyRigorDialogChrome(this);

    auto* root = new QVBoxLayout(this);
    auto* intro = new QLabel(tr(
        "Descriptor-first key management for ORU (wpkh / Bech32) and RES (res / Bech32m). "
        "Private keys never leave this machine. Prefer exporting public descriptors for watch-only cold storage. "
        "Use Export Descriptor (include private) instead of dumpprivkey."));
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto* tabs = new QTabWidget(this);
    root->addWidget(tabs, 1);

    // --- Import private key ---
    {
        auto* page = new QWidget(this);
        auto* layout = new QVBoxLayout(page);
        auto* form = new QFormLayout();
        m_privkey_edit = new QLineEdit(page);
        m_privkey_edit->setEchoMode(QLineEdit::Password);
        m_privkey_edit->setPlaceholderText(tr("WIF (ORU) or 64-byte Ed25519 secret hex (RES)"));
        form->addRow(tr("Private key"), m_privkey_edit);

        m_asset_combo = new QComboBox(page);
        m_asset_combo->addItem(tr("Auto-detect"), QStringLiteral("auto"));
        m_asset_combo->addItem(tr("ORU (wpkh / Bech32)"), QStringLiteral("ORU"));
        m_asset_combo->addItem(tr("RES (res / Bech32m)"), QStringLiteral("RES"));
        form->addRow(tr("Asset"), m_asset_combo);

        m_privkey_label = new QLineEdit(page);
        form->addRow(tr("Label (optional)"), m_privkey_label);

        m_privkey_rescan = new QCheckBox(tr("Rescan blockchain after import"), page);
        m_privkey_rescan->setChecked(true);
        form->addRow(QString(), m_privkey_rescan);
        layout->addLayout(form);

        auto* note = new QLabel(tr("RIGOR uses res() for Witness v1 RES — not Bitcoin Taproot tr()."));
        note->setWordWrap(true);
        layout->addWidget(note);

        auto* btn = new QPushButton(tr("Import Private Key"), page);
        connect(btn, &QPushButton::clicked, this, &KeyManagementDialog::importPrivateKey);
        layout->addWidget(btn);

        m_privkey_status = new QLabel(page);
        m_privkey_status->setWordWrap(true);
        layout->addWidget(m_privkey_status);
        layout->addStretch();
        tabs->addTab(page, tr("Import Private Key"));
    }

    // --- Import descriptor ---
    {
        auto* page = new QWidget(this);
        auto* layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel(tr("Paste a checksummed descriptor (wpkh(...)#… or res(...)#…):")));
        m_desc_import_edit = new QPlainTextEdit(page);
        m_desc_import_edit->setPlaceholderText(tr("wpkh([fingerprint/path]xpub…)#checksum\nor res(<32-byte pubkey hex>)#checksum"));
        layout->addWidget(m_desc_import_edit, 1);

        m_desc_active = new QCheckBox(tr("Mark as active receive descriptor (ranged ORU only; RES res() is fixed)"), page);
        layout->addWidget(m_desc_active);

        auto* btn = new QPushButton(tr("Import Descriptor"), page);
        connect(btn, &QPushButton::clicked, this, &KeyManagementDialog::importDescriptor);
        layout->addWidget(btn);

        m_desc_status = new QLabel(page);
        m_desc_status->setWordWrap(true);
        layout->addWidget(m_desc_status);
        tabs->addTab(page, tr("Import Descriptor"));
    }

    // --- Export cold storage ---
    {
        auto* page = new QWidget(this);
        auto* layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel(tr(
            "Export descriptors for cold-storage backup or an online watch-only wallet. "
            "Public descriptors contain no private keys.")));

        m_export_private = new QCheckBox(tr("Include private descriptors (keep offline only)"), page);
        layout->addWidget(m_export_private);

        auto* row = new QHBoxLayout();
        auto* export_btn = new QPushButton(tr("Export Cold Storage Descriptors"), page);
        connect(export_btn, &QPushButton::clicked, this, &KeyManagementDialog::exportDescriptors);
        row->addWidget(export_btn);
        auto* copy_btn = new QPushButton(tr("Copy"), page);
        connect(copy_btn, &QPushButton::clicked, this, &KeyManagementDialog::copyExport);
        row->addWidget(copy_btn);
        row->addStretch();
        layout->addLayout(row);

        m_desc_export_edit = new QPlainTextEdit(page);
        m_desc_export_edit->setReadOnly(true);
        layout->addWidget(m_desc_export_edit, 1);
        tabs->addTab(page, tr("Export Descriptor"));
    }

    auto* close_btn = new QPushButton(tr("Close"), this);
    connect(close_btn, &QPushButton::clicked, this, &QDialog::accept);
    root->addWidget(close_btn, 0, Qt::AlignRight);
}

void KeyManagementDialog::setModel(WalletModel* model)
{
    m_model = model;
}

void KeyManagementDialog::importPrivateKey()
{
    if (!m_model) return;
    const QString key = m_privkey_edit->text().trimmed();
    if (key.isEmpty()) {
        m_privkey_status->setText(tr("Enter a private key."));
        return;
    }
    if (m_model->wallet().privateKeysDisabled()) {
        m_privkey_status->setText(tr("This wallet is watch-only (private keys disabled). Import a public descriptor instead."));
        return;
    }

    try {
        UniValue params(UniValue::VARR);
        params.push_back(key.toStdString());
        params.push_back(m_privkey_label->text().trimmed().toStdString());
        params.push_back(m_privkey_rescan->isChecked());
        params.push_back(m_asset_combo->currentData().toString().toStdString());

        const UniValue result = m_model->node().executeRpc("importrigorprivkey", params, WalletRpcUri(m_model));
        const QString asset = QString::fromStdString(result["asset"].get_str());
        const QString address = QString::fromStdString(result["address"].get_str());
        const QString desc = QString::fromStdString(result["descriptor"].get_str());
        m_privkey_status->setText(tr("Imported %1 key.\nAddress: %2\nDescriptor: %3")
                                      .arg(asset, address, desc));
        m_privkey_edit->clear();
    } catch (const UniValue& e) {
        m_privkey_status->setText(tr("Import failed: %1").arg(QString::fromStdString(e.write())));
    } catch (const std::exception& e) {
        m_privkey_status->setText(tr("Import failed: %1").arg(RpcErrorMessage(e)));
    }
}

void KeyManagementDialog::importDescriptor()
{
    if (!m_model) return;
    const QString desc = m_desc_import_edit->toPlainText().trimmed();
    if (desc.isEmpty()) {
        m_desc_status->setText(tr("Paste a descriptor first."));
        return;
    }

    try {
        UniValue req(UniValue::VOBJ);
        req.pushKV("desc", desc.toStdString());
        req.pushKV("timestamp", "now");
        if (m_desc_active->isChecked()) {
            req.pushKV("active", true);
        }

        UniValue requests(UniValue::VARR);
        requests.push_back(req);
        UniValue params(UniValue::VARR);
        params.push_back(requests);

        const UniValue result = m_model->node().executeRpc("importdescriptors", params, WalletRpcUri(m_model));
        if (result.isArray() && !result.empty() && result[0]["success"].get_bool()) {
            m_desc_status->setText(tr("Descriptor imported successfully. Back up the wallet if this added spendable keys."));
        } else {
            m_desc_status->setText(tr("Import result: %1").arg(QString::fromStdString(result.write(2))));
        }
    } catch (const UniValue& e) {
        m_desc_status->setText(tr("Import failed: %1").arg(QString::fromStdString(e.write())));
    } catch (const std::exception& e) {
        m_desc_status->setText(tr("Import failed: %1").arg(RpcErrorMessage(e)));
    }
}

void KeyManagementDialog::exportDescriptors()
{
    if (!m_model) return;
    try {
        UniValue params(UniValue::VARR);
        params.push_back(m_export_private->isChecked());
        const UniValue result = m_model->node().executeRpc("listdescriptors", params, WalletRpcUri(m_model));

        QStringList lines;
        lines << tr("# RIGOR cold-storage descriptor export");
        lines << tr("# wallet: %1").arg(m_model->getWalletName());
        lines << tr("# private: %1").arg(m_export_private->isChecked() ? tr("yes — keep offline") : tr("no"));
        lines << QString();

        const UniValue& descs = result["descriptors"];
        for (const UniValue& d : descs.getValues()) {
            lines << QString::fromStdString(d["desc"].get_str());
            if (d.exists("active") && d["active"].get_bool()) {
                lines << tr("# active=%1 internal=%2")
                             .arg(d["active"].get_bool() ? "true" : "false")
                             .arg(d.exists("internal") && d["internal"].get_bool() ? "true" : "false");
            }
            lines << QString();
        }
        m_desc_export_edit->setPlainText(lines.join(QLatin1Char('\n')));
    } catch (const UniValue& e) {
        QMessageBox::warning(this, tr("Export failed"), QString::fromStdString(e.write()));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Export failed"), RpcErrorMessage(e));
    }
}

void KeyManagementDialog::copyExport()
{
    QApplication::clipboard()->setText(m_desc_export_edit->toPlainText());
}
