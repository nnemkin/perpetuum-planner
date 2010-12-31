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

#ifndef MESSAGEBOX_H
#define MESSAGEBOX_H

#include <QDialogButtonBox>
#include <QWeakPointer>

#include "ui_messagebox.h"
#include "ui_textwindow.h"


class MessageBox : public QDialog, private Ui::MessageBox
{
    Q_OBJECT

public:
    explicit MessageBox(QWidget *parent = 0);

    void setText(const QString& text) { labelMessage->setText(text); }
    void setStandardButtons(QDialogButtonBox::StandardButtons buttons);

    int result() { return m_result; }

    static void show(QWidget *parent, const QString &title, const QString &text,
                     QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok);

    static int exec(QWidget *parent, const QString &title, const QString &text,
                    QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok);

protected:
    void changeEvent(QEvent *e);

private slots:
    void on_buttonBox_clicked(QAbstractButton* button);

private:
    int m_result;
};


class TextWindow : public QDialog, private Ui::TextWindow
{
    Q_OBJECT

public:
    enum Size { Small, Medium, Large };

    explicit TextWindow(QWidget *parent = 0);
    ~TextWindow();

    void setText(const QString &text) { textMessage->setText(text); }

    QDialogButtonBox *buttons() { return buttonBox; }

    static QWeakPointer<TextWindow> show(QWidget *parent, const QString &title, const QString &text, Size size = Medium, const QString &id = QString());

protected:
    void changeEvent(QEvent *);
    void customEvent(QEvent *);

};

#endif // MESSAGEBOX_H
