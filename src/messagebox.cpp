/*
    Copyright (C) 2010 Nikita Nemkin <nikita@nemkin.ru>

    This file is part of PerpetuumPlanner.

    PerpetuumPlanner is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PerpetuumPlanner is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PerpetuumPlanner. If not, see <http://www.gnu.org/licenses/>.
*/

#include <QApplication>
#include <QSettings>

#include "messagebox.h"
#include "application.h"


// MessageBox

MessageBox::MessageBox(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::WindowSystemMenuHint), m_result(QDialogButtonBox::NoButton)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setupUi(this);
}

void MessageBox::setStandardButtons(QDialogButtonBox::StandardButtons buttons)
{
    buttonBox->setStandardButtons(buttons);

    foreach (QAbstractButton *button, buttonBox->buttons())
        button->setFocusPolicy(Qt::NoFocus);
}

void MessageBox::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);

    if (e->type() == QEvent::LanguageChange)
        retranslateUi(this);
}

void MessageBox::show(QWidget *parent, const QString &title, const QString &text,
                         QDialogButtonBox::StandardButtons buttons)
{
    MessageBox *messageBox = new MessageBox(parent);
    messageBox->setWindowTitle(title);
    messageBox->setText(text);
    messageBox->setStandardButtons(buttons);
    messageBox->QDialog::show();
}

int MessageBox::exec(QWidget *parent, const QString &title, const QString &text,
                              QDialogButtonBox::StandardButtons buttons)
{
    MessageBox *messageBox = new MessageBox(parent);
    messageBox->setAttribute(Qt::WA_DeleteOnClose, false);
    messageBox->setWindowTitle(title);
    messageBox->setText(text);
    messageBox->setStandardButtons(buttons);
    messageBox->QDialog::exec();
    int result = messageBox->result();
    delete messageBox;
    return result;
}

void MessageBox::on_buttonBox_clicked(QAbstractButton* button)
{
    m_result = buttonBox->standardButton(button);
}


// TextWindow

TextWindow::TextWindow(QWidget *parent) : QDialog(parent, Qt::FramelessWindowHint | Qt::WindowSystemMenuHint)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setupUi(this);

    textMessage->document()->setDefaultStyleSheet(qApp->docStyleSheet());
    textMessage->document()->setDocumentMargin(10);
}

TextWindow::~TextWindow()
{
    if (!objectName().isEmpty())
        QSettings().setValue(QLatin1String("Geometry/") + objectName(), saveGeometry());
}

QWeakPointer<TextWindow> TextWindow::show(QWidget *parent, const QString &title, const QString &text, Size size, const QString &id)
{
    TextWindow* textWindow = new TextWindow(parent);
    textWindow->setObjectName(id);
    textWindow->setWindowTitle(title);
    textWindow->setText(text);
    switch (size) {
    case Small:
        textWindow->resize(300, 200);
        break;
    case Medium:
        textWindow->resize(400, 300);
        break;
    case Large:
        textWindow->resize(450, 400);
        break;
    }

    if (!id.isEmpty())
        textWindow->restoreGeometry(QSettings().value(QLatin1String("Geometry/") + id).toByteArray());

    textWindow->QDialog::show();
    return textWindow;
}

void TextWindow::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);

    if (e->type() == QEvent::LanguageChange)
        retranslateUi(this);
}

void TextWindow::customEvent(QEvent *event)
{
    if (event->type() == Application::SettingsChanged) {
        textMessage->document()->setDefaultStyleSheet(qApp->docStyleSheet());
        textMessage->setHtml(textMessage->toHtml());
    }
}
