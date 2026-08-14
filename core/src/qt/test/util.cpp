// Copyright (c) 2018-2022 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chrono>

#include <QAbstractButton>
#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QWidget>

void ConfirmMessage(QString* text, std::chrono::milliseconds msec)
{
    QTimer::singleShot(msec, [text]() {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            // SendConfirmationDialog subclasses QMessageBox; clicking a closing
            // bump/send dialog from a later ConfirmMessage is a use-after-free on offscreen.
            if (widget->inherits("SendConfirmationDialog")) continue;
            if (!widget->inherits("QMessageBox")) continue;
            QMessageBox* messageBox = qobject_cast<QMessageBox*>(widget);
            if (!messageBox) continue;
            if (text) *text = messageBox->text();
            QAbstractButton* button = messageBox->defaultButton();
            if (!button) button = messageBox->button(QMessageBox::Ok);
            if (!button) button = messageBox->button(QMessageBox::Yes);
            if (button) button->click();
        }
    });
}
