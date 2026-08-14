// Copyright (c) 2026 The RIGOR developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_KEYMANAGEMENTDIALOG_H
#define BITCOIN_QT_KEYMANAGEMENTDIALOG_H

#include <QDialog>

class PlatformStyle;
class WalletModel;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTabWidget;

/** Wallet → Advanced → Key Management / Security: descriptor import/export and address key export. */
class KeyManagementDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Tab {
        ImportPrivateKey = 0,
        ImportDescriptor = 1,
        ExportDescriptor = 2,
        ExportPrivateKey = 3,
    };

    explicit KeyManagementDialog(const PlatformStyle* platformStyle, QWidget* parent = nullptr);
    void setModel(WalletModel* model);
    void setActiveTab(Tab tab);

private Q_SLOTS:
    void importPrivateKey();
    void importDescriptor();
    void exportDescriptors();
    void copyExport();
    void onExportAddressEdited(const QString& text);
    void exportAddressPrivateKey();
    void copyAddressPrivateKey();

private:
    void refreshKeyModelStatus();

    WalletModel* m_model{nullptr};
    const PlatformStyle* m_platform_style{nullptr};
    QTabWidget* m_tabs{nullptr};

    QLabel* m_key_model_status{nullptr};

    QLineEdit* m_privkey_edit{nullptr};
    QComboBox* m_asset_combo{nullptr};
    QLineEdit* m_privkey_label{nullptr};
    QCheckBox* m_privkey_rescan{nullptr};
    QLabel* m_privkey_status{nullptr};

    QPlainTextEdit* m_desc_import_edit{nullptr};
    QCheckBox* m_desc_active{nullptr};
    QLabel* m_desc_status{nullptr};

    QPlainTextEdit* m_desc_export_edit{nullptr};
    QCheckBox* m_export_private{nullptr};

    QLineEdit* m_export_address_edit{nullptr};
    QLabel* m_export_asset_label{nullptr};
    QPlainTextEdit* m_export_key_edit{nullptr};
    QLabel* m_export_key_status{nullptr};
};

#endif // BITCOIN_QT_KEYMANAGEMENTDIALOG_H
