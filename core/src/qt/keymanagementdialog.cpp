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

#include <chainparams.h>
#include <interfaces/node.h>
#include <key_io.h>
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
#include <QRegularExpression>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <vector>

namespace {

std::string WalletRpcUri(WalletModel* model)
{
    if (!model) return "/";
    const QByteArray encoded = QUrl::toPercentEncoding(model->getWalletName());
    return "/wallet/" + std::string(encoded.constData(), encoded.length());
}

/** One descriptor line extracted from a Key Management export or a raw paste. */
struct ParsedImportDescriptor {
    QString desc;
    bool active{false};
    bool internal{false};
    bool have_active{false};
    bool have_internal{false};
};

/**
 * Export Descriptor writes human comments (# ...) plus checksummed descriptor
 * lines. importdescriptors treats '#' as the checksum marker, so pasting the
 * export verbatim used to fail with "Multiple '#' symbols".
 */
std::vector<ParsedImportDescriptor> ExtractImportDescriptors(const QString& text)
{
    std::vector<ParsedImportDescriptor> out;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith(QLatin1Char('#'))) {
            if (out.empty()) continue;
            if (line.contains(QStringLiteral("active="))) {
                out.back().have_active = true;
                out.back().active = line.contains(QStringLiteral("active=true"));
            }
            if (line.contains(QStringLiteral("internal="))) {
                out.back().have_internal = true;
                out.back().internal = line.contains(QStringLiteral("internal=true"));
            }
            continue;
        }
        QString desc = line;
        desc.remove(QRegularExpression(QStringLiteral("\\s+")));
        if (desc.isEmpty()) continue;
        ParsedImportDescriptor parsed;
        parsed.desc = desc;
        out.push_back(parsed);
    }
    return out;
}

bool DescriptorHasPrivateMaterial(const QString& desc)
{
    if (desc.contains(QLatin1String("xprv"), Qt::CaseInsensitive)) return true;
    static const QRegularExpression res_secret(QStringLiteral("res\\(([0-9a-fA-F]{128})\\)"));
    return res_secret.match(desc).hasMatch();
}

// Public ranged HD after the xpub uses hardened steps (/0'/*') — cannot expand.
bool DescriptorIsHardenedPublicRange(const QString& desc)
{
    if (desc.contains(QLatin1String("xprv"), Qt::CaseInsensitive)) return false;
    const int xpub_at = desc.indexOf(QLatin1String("xpub"), 0, Qt::CaseInsensitive);
    if (xpub_at < 0) return false;
    const QString after_xpub = desc.mid(xpub_at);
    return after_xpub.contains(QLatin1String("/*'")) ||
           after_xpub.contains(QRegularExpression(QStringLiteral("/\\d+'")));
}

QString RpcErrorMessage(const std::exception& e)
{
    return QString::fromStdString(e.what());
}

QString DetectAssetLabel(const QString& address)
{
    const auto asset = GetAssetTypeFromAddress(address.toStdString(), Params());
    if (!asset) return QStringLiteral("Unknown / invalid");
    if (*asset == ASSET_ORU) return QStringLiteral("ORU (P2WPKH / Bech32)");
    if (*asset == ASSET_RES) return QStringLiteral("RES (native v1 / Bech32m)");
    return QStringLiteral("Unsupported");
}

} // namespace

KeyManagementDialog::KeyManagementDialog(const PlatformStyle* platformStyle, QWidget* parent)
    : QDialog(parent, GUIUtil::dialog_flags),
      m_platform_style(platformStyle)
{
    setWindowTitle(tr("Key Management"));
    resize(640, 520);
    GUIUtil::ApplyRigorDialogChrome(this);

    auto* root = new QVBoxLayout(this);
    auto* intro = new QLabel(tr(
        "Descriptor-first key management for ORU (wpkh / Bech32) and RES (res / Bech32m). "
        "Private keys never leave this machine. "
        "To restore a spendable HD wallet, export with private descriptors and keep that file offline. "
        "Public ranged xpub lines use hardened paths and cannot be imported. "
        "Address-level export is under the Export private key tab (dumporuprivkey / dumpresprivkey)."));
    intro->setWordWrap(true);
    root->addWidget(intro);

    {
        auto* status_box = new QGroupBox(tr("Key Management Status"), this);
        auto* status_layout = new QVBoxLayout(status_box);
        m_key_model_status = new QLabel(status_box);
        m_key_model_status->setWordWrap(true);
        m_key_model_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
        status_layout->addWidget(m_key_model_status);
        root->addWidget(status_box);
    }

    m_tabs = new QTabWidget(this);
    root->addWidget(m_tabs, 1);

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
        m_tabs->addTab(page, tr("Import private key"));
    }

    // --- Export private key ---
    {
        auto* page = new QWidget(this);
        auto* layout = new QVBoxLayout(page);

        auto* warning = new QLabel(tr(
            "Warning:\n"
            "Anyone with this private key can control this address.\n\n"
            "Never share this key."));
        warning->setWordWrap(true);
        warning->setObjectName(QStringLiteral("exportPrivKeyWarning"));
        warning->setStyleSheet(QStringLiteral("QLabel#exportPrivKeyWarning { color: #FF6B6B; font-weight: 600; }"));
        layout->addWidget(warning);

        layout->addWidget(new QLabel(tr(
            "Address-level private key export (dumporuprivkey / dumpresprivkey). "
            "Not shown on Overview, Receive, Transactions, or Convergence.")));

        auto* form = new QFormLayout();
        m_export_address_edit = new QLineEdit(page);
        m_export_address_edit->setPlaceholderText(tr("Owned ORU or RES address"));
        connect(m_export_address_edit, &QLineEdit::textChanged, this, &KeyManagementDialog::onExportAddressEdited);
        form->addRow(tr("Address"), m_export_address_edit);

        m_export_asset_label = new QLabel(tr("—"), page);
        form->addRow(tr("Detected asset"), m_export_asset_label);
        layout->addLayout(form);

        auto* row = new QHBoxLayout();
        auto* export_btn = new QPushButton(tr("Export Private Key"), page);
        connect(export_btn, &QPushButton::clicked, this, &KeyManagementDialog::exportAddressPrivateKey);
        row->addWidget(export_btn);
        auto* copy_btn = new QPushButton(tr("Copy Key"), page);
        connect(copy_btn, &QPushButton::clicked, this, &KeyManagementDialog::copyAddressPrivateKey);
        row->addWidget(copy_btn);
        row->addStretch();
        layout->addLayout(row);

        m_export_key_edit = new QPlainTextEdit(page);
        m_export_key_edit->setReadOnly(true);
        m_export_key_edit->setPlaceholderText(tr("Exported key material appears here after confirmation."));
        layout->addWidget(m_export_key_edit, 1);

        m_export_key_status = new QLabel(page);
        m_export_key_status->setWordWrap(true);
        layout->addWidget(m_export_key_status);

        m_tabs->addTab(page, tr("Export private key"));
    }

    // --- Import descriptor ---
    {
        auto* page = new QWidget(this);
        auto* layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel(tr(
            "Paste a checksummed descriptor, or a Key Management export. "
            "Comment lines starting with # are ignored. "
            "A public export (# private: no) cannot restore this spendable HD wallet — "
            "re-export with private descriptors checked.")));
        m_desc_import_edit = new QPlainTextEdit(page);
        m_desc_import_edit->setPlaceholderText(tr(
            "Private restore: export with “Include private descriptors”, then paste here.\n"
            "Watch-only: res(<32-byte pubkey hex>)#checksum into a watch-only wallet."));
        layout->addWidget(m_desc_import_edit, 1);

        m_desc_active = new QCheckBox(tr("Mark as active receive descriptor (ranged ORU only; RES res() is fixed)"), page);
        layout->addWidget(m_desc_active);

        auto* btn = new QPushButton(tr("Import Descriptor"), page);
        connect(btn, &QPushButton::clicked, this, &KeyManagementDialog::importDescriptor);
        layout->addWidget(btn);

        m_desc_status = new QLabel(page);
        m_desc_status->setWordWrap(true);
        m_desc_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(m_desc_status);
        m_tabs->addTab(page, tr("Import descriptor"));
    }

    // --- Export descriptor ---
    {
        auto* page = new QWidget(this);
        auto* layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel(tr(
            "Private export restores a spendable HD wallet (keep offline). "
            "Public export is not a restore file: RIGOR HD paths are hardened (/0'/*'), "
            "so xpub lines cannot be imported. A watch-only wallet can import res(<pubkey hex>). "
            "Comment lines starting with # are ignored when importing.")));

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
        m_tabs->addTab(page, tr("Export descriptor"));
    }

    auto* close_btn = new QPushButton(tr("Close"), this);
    connect(close_btn, &QPushButton::clicked, this, &QDialog::accept);
    root->addWidget(close_btn, 0, Qt::AlignRight);
}

void KeyManagementDialog::setModel(WalletModel* model)
{
    m_model = model;
    refreshKeyModelStatus();
}

void KeyManagementDialog::refreshKeyModelStatus()
{
    if (!m_key_model_status) return;
    if (!m_model) {
        m_key_model_status->setText(tr("No wallet selected."));
        return;
    }

    auto& wallet = m_model->wallet();
    if (wallet.privateKeysDisabled()) {
        m_key_model_status->setText(tr(
            "Watch-only wallet (private keys disabled).\n"
            "ORU / RES key material is not available for export from this wallet."));
        return;
    }

    const QString oru_line = wallet.oruHdEnabled()
                                 ? tr("ORU:\nHD wallet enabled")
                                 : tr("ORU:\nHD wallet not available");
    QString res_line;
    if (wallet.resHdEnabled()) {
        res_line = tr("RES:\nHD wallet enabled");
    } else if (wallet.resDedicatedKeyMode()) {
        res_line = tr("RES:\nDedicated key mode");
    } else {
        res_line = tr("RES:\nNot configured");
    }
    m_key_model_status->setText(oru_line + QLatin1Char('\n') + QLatin1Char('\n') + res_line);
}

void KeyManagementDialog::setActiveTab(Tab tab)
{
    if (m_tabs) {
        m_tabs->setCurrentIndex(static_cast<int>(tab));
    }
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
    const QString pasted = m_desc_import_edit->toPlainText().trimmed();
    if (pasted.isEmpty()) {
        m_desc_status->setText(tr("Paste a descriptor first."));
        return;
    }

    const std::vector<ParsedImportDescriptor> parsed = ExtractImportDescriptors(pasted);
    if (parsed.empty()) {
        m_desc_status->setText(tr(
            "No descriptor line found. Comment lines starting with # are ignored. "
            "Paste a checksummed descriptor (wpkh(...)#… or res(...)#…)."));
        return;
    }

    const bool spendable = !m_model->wallet().privateKeysDisabled();
    bool has_private = false;
    for (const ParsedImportDescriptor& item : parsed) {
        if (DescriptorHasPrivateMaterial(item.desc)) has_private = true;
    }

    if (spendable && !has_private) {
        m_desc_status->setText(tr(
            "This is a public export. It cannot restore a spendable HD wallet.\n\n"
            "RIGOR HD derivation is hardened (/0'/*'). An xpub cannot create those child keys, "
            "so the ranged res(...) / wpkh(...) lines fail with "
            "“hardened derivations without private keys”. "
            "The single-key res(<pubkey hex>) line cannot be imported into a wallet that already has private keys.\n\n"
            "To restore spendability: open Export descriptor, check "
            "“Include private descriptors (keep offline only)”, then import that file "
            "into a new or blank wallet.\n"
            "To watch a public key only: create a watch-only wallet and import res(<32-byte pubkey hex>)."));
        return;
    }

    std::vector<ParsedImportDescriptor> to_import = parsed;
    if (!spendable && !has_private) {
        to_import.clear();
        for (const ParsedImportDescriptor& item : parsed) {
            if (!DescriptorIsHardenedPublicRange(item.desc)) {
                to_import.push_back(item);
            }
        }
        if (to_import.empty()) {
            m_desc_status->setText(tr(
                "Ranged public descriptors with hardened paths cannot be imported, even into a watch-only wallet. "
                "Import res(<32-byte pubkey hex>) or an address descriptor instead."));
            return;
        }
    }

    try {
        UniValue requests(UniValue::VARR);
        for (const ParsedImportDescriptor& item : to_import) {
            UniValue req(UniValue::VOBJ);
            req.pushKV("desc", item.desc.toStdString());
            req.pushKV("timestamp", "now");
            const bool active = item.have_active ? item.active : m_desc_active->isChecked();
            if (active) {
                req.pushKV("active", true);
            }
            if (item.have_internal) {
                req.pushKV("internal", item.internal);
            }
            requests.push_back(req);
        }
        UniValue params(UniValue::VARR);
        params.push_back(requests);

        const UniValue result = m_model->node().executeRpc("importdescriptors", params, WalletRpcUri(m_model));
        bool all_ok = result.isArray() && !result.empty();
        int ok_count = 0;
        QStringList failures;
        if (result.isArray()) {
            int i = 0;
            for (const UniValue& row : result.getValues()) {
                if (row.exists("success") && row["success"].get_bool()) {
                    ++ok_count;
                } else {
                    all_ok = false;
                    QString reason = QStringLiteral("unknown error");
                    if (row.exists("error") && row["error"].isObject() && row["error"].exists("message")) {
                        reason = QString::fromStdString(row["error"]["message"].get_str());
                    }
                    const QString desc = (i < static_cast<int>(to_import.size())) ? to_import[static_cast<size_t>(i)].desc : QString();
                    failures << QStringLiteral("%1: %2").arg(desc.left(48), reason);
                }
                ++i;
            }
        }
        QString extra;
        if (to_import.size() < parsed.size()) {
            extra = QLatin1Char('\n') + tr("Skipped %1 hardened public xpub line(s); they cannot be expanded.")
                                            .arg(static_cast<int>(parsed.size() - to_import.size()));
        }
        if (all_ok) {
            m_desc_status->setText(tr("Imported %1 descriptor(s) successfully. Back up the wallet if this added spendable keys.")
                                       .arg(ok_count) + extra);
        } else {
            m_desc_status->setText(tr("Imported %1 of %2 descriptor(s).\n%3")
                                       .arg(ok_count)
                                       .arg(static_cast<int>(to_import.size()))
                                       .arg(failures.join(QLatin1Char('\n'))) + extra);
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
        if (m_export_private->isChecked()) {
            lines << tr("# RIGOR cold-storage descriptor export");
            lines << tr("# wallet: %1").arg(m_model->getWalletName());
            lines << tr("# private: yes — keep offline");
        } else {
            lines << tr("# RIGOR public descriptor export");
            lines << tr("# wallet: %1").arg(m_model->getWalletName());
            lines << tr("# private: no");
            lines << tr("# not a restore file — hardened xpub cannot be imported");
        }
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

void KeyManagementDialog::onExportAddressEdited(const QString& text)
{
    m_export_asset_label->setText(DetectAssetLabel(text.trimmed()));
    m_export_key_edit->clear();
    m_export_key_status->clear();
}

void KeyManagementDialog::exportAddressPrivateKey()
{
    if (!m_model) return;

    const QString address = m_export_address_edit->text().trimmed();
    if (address.isEmpty()) {
        m_export_key_status->setText(tr("Enter an address."));
        return;
    }

    const auto asset = GetAssetTypeFromAddress(address.toStdString(), Params());
    if (!asset || (*asset != ASSET_ORU && *asset != ASSET_RES)) {
        m_export_key_status->setText(tr("Address must be a valid ORU or RES address."));
        return;
    }

    if (m_model->wallet().privateKeysDisabled()) {
        m_export_key_status->setText(tr("Private key unavailable for watch-only descriptor"));
        return;
    }

    const auto confirm = QMessageBox::warning(
        this,
        tr("Export Private Key"),
        tr("Warning:\nAnyone with this private key can control this address.\n\nNever share this key.\n\nContinue?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (confirm != QMessageBox::Yes) {
        return;
    }

    WalletModel::UnlockContext ctx(m_model->requestUnlock());
    if (!ctx.isValid()) {
        m_export_key_status->setText(tr("Wallet is locked"));
        return;
    }

    const char* rpc = (*asset == ASSET_ORU) ? "dumporuprivkey" : "dumpresprivkey";
    try {
        UniValue params(UniValue::VARR);
        params.push_back(address.toStdString());
        const UniValue result = m_model->node().executeRpc(rpc, params, WalletRpcUri(m_model));

        const QString out_addr = QString::fromStdString(result["address"].get_str());
        const QString out_asset = QString::fromStdString(result["asset"].get_str());
        const QString key = QString::fromStdString(result["private_key"].get_str());
        const QString format = QString::fromStdString(result["format"].get_str());

        m_export_key_edit->setPlainText(
            tr("address: %1\nasset: %2\nformat: %3\nprivate_key:\n%4")
                .arg(out_addr, out_asset, format, key));
        m_export_key_status->setText(tr("Export complete. Keep this key offline."));
    } catch (const UniValue& e) {
        m_export_key_status->setText(tr("Export failed: %1").arg(QString::fromStdString(e.write())));
        m_export_key_edit->clear();
    } catch (const std::exception& e) {
        m_export_key_status->setText(tr("Export failed: %1").arg(QString::fromStdString(e.what())));
        m_export_key_edit->clear();
    }
}

void KeyManagementDialog::copyAddressPrivateKey()
{
    const QString text = m_export_key_edit->toPlainText();
    if (text.isEmpty()) return;
    const QString marker = QStringLiteral("private_key:\n");
    const int idx = text.indexOf(marker);
    if (idx >= 0) {
        QApplication::clipboard()->setText(text.mid(idx + marker.size()).trimmed());
    } else {
        QApplication::clipboard()->setText(text);
    }
}
