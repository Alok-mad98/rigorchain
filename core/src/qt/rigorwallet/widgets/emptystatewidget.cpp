// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/emptystatewidget.h>

#include <qt/rigorwallet/rigortheme.h>

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace RigorWallet {

EmptyStateWidget::EmptyStateWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("RigorEmptyState"));
    const ColorPalette p = RigorTheme::colors();
    const Spacing s = RigorTheme::spacing();
    setStyleSheet(QStringLiteral(
                      "QWidget#RigorEmptyState { background: transparent; }"
                      "QLabel#RigorEmptyHeading { color: %1; font-size: %2px; font-weight: 600; }"
                      "QLabel#RigorEmptyBody { color: %3; font-size: %4px; }")
                      .arg(RigorTheme::colorHex(p.textPrimary))
                      .arg(RigorTheme::typography().uiBodyLargePx)
                      .arg(RigorTheme::colorHex(p.textMuted))
                      .arg(RigorTheme::typography().uiBodyPx));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(s.cardPadding, s.sectionGap, s.cardPadding, s.sectionGap);
    layout->setSpacing(s.unit);
    layout->setAlignment(Qt::AlignCenter);

    m_heading = new QLabel(this);
    m_heading->setObjectName(QStringLiteral("RigorEmptyHeading"));
    m_heading->setAlignment(Qt::AlignCenter);
    m_heading->setWordWrap(true);

    m_body = new QLabel(this);
    m_body->setObjectName(QStringLiteral("RigorEmptyBody"));
    m_body->setAlignment(Qt::AlignCenter);
    m_body->setWordWrap(true);

    m_action = new QPushButton(this);
    m_action->setVisible(false);
    connect(m_action, &QPushButton::clicked, this, &EmptyStateWidget::actionTriggered);

    layout->addWidget(m_heading);
    layout->addWidget(m_body);
    layout->addWidget(m_action, 0, Qt::AlignHCenter);
}

void EmptyStateWidget::setHeading(const QString& heading)
{
    m_heading->setText(heading);
}

void EmptyStateWidget::setBody(const QString& body)
{
    m_body->setText(body);
}

void EmptyStateWidget::setActionText(const QString& text)
{
    m_action->setText(text);
    m_action->setVisible(!text.isEmpty());
}

void EmptyStateWidget::clearAction()
{
    m_action->setVisible(false);
    m_action->setText(QString());
}

} // namespace RigorWallet
