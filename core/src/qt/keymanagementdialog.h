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

/** Wallet → Advanced → Key Management: import keys/descriptors and export cold-storage descriptors. */
class KeyManagementDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KeyManagementDialog(const PlatformStyle* platformStyle, QWidget* parent = nullptr);
    void setModel(WalletModel* model);

private Q_SLOTS:
    void importPrivateKey();
    void importDescriptor();
    void exportDescriptors();
    void copyExport();

private:
    WalletModel* m_model{nullptr};
    const PlatformStyle* m_platform_style{nullptr};

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
};

#endif // BITCOIN_QT_KEYMANAGEMENTDIALOG_H
