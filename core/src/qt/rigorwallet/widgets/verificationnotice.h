// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_VERIFICATIONNOTICE_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_VERIFICATIONNOTICE_H

#include <qt/rigorwallet/widgets/warningbanner.h>

#include <QWidget>

class QVBoxLayout;

namespace RigorWallet {

/** Overview compliance strip — user-facing only; technical detail in Diagnostics. */
class VerificationNotice : public QWidget
{
    Q_OBJECT

public:
    explicit VerificationNotice(QWidget* parent = nullptr);

    void refresh();

private:
    void buildNotices();

    QVBoxLayout* m_layout{nullptr};
    WarningBanner* m_experimentalBanner{nullptr};
};

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_WIDGETS_VERIFICATIONNOTICE_H
