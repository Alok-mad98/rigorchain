// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_EMPTYSTATEWIDGET_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_EMPTYSTATEWIDGET_H

#include <QWidget>

class QLabel;
class QPushButton;

namespace RigorWallet {

/** Consistent empty-state presentation for wallet panels. */
class EmptyStateWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EmptyStateWidget(QWidget* parent = nullptr);

    void setHeading(const QString& heading);
    void setBody(const QString& body);
    void setActionText(const QString& text);
    void clearAction();

Q_SIGNALS:
    void actionTriggered();

private:
    QLabel* m_heading{nullptr};
    QLabel* m_body{nullptr};
    QPushButton* m_action{nullptr};
};

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_WIDGETS_EMPTYSTATEWIDGET_H
