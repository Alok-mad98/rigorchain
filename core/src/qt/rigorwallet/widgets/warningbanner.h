// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_WARNINGBANNER_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_WARNINGBANNER_H

#include <QFrame>
#include <QString>

class QLabel;
class QPushButton;

namespace RigorWallet {

/** Severity level for reusable dashboard banners. */
enum class WarningLevel {
    Info,
    Warning,
    Critical,
    Verification,
};

/** Reusable warning / notice banner (BUILD-04). */
class WarningBanner : public QFrame
{
    Q_OBJECT

public:
    explicit WarningBanner(QWidget* parent = nullptr);

    WarningLevel level() const { return m_level; }
    QString message() const;

    void setLevel(WarningLevel level);
    void setMessage(const QString& message);
    void setDismissible(bool dismissible);

public Q_SLOTS:
    void showBanner(WarningLevel level, const QString& message);
    void hideBanner();

Q_SIGNALS:
    void dismissed();

private:
    void applyLevelStyle();
    void onDismissClicked();

    WarningLevel m_level{WarningLevel::Info};
    QLabel* m_label{nullptr};
    QPushButton* m_dismissButton{nullptr};
};

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_WIDGETS_WARNINGBANNER_H
