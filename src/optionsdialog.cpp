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

#include <QSettings>
#include <QDir>
#include <QTranslator>

#include "optionsdialog.h"
#include "messagebox.h"
#include "application.h"


OptionsDialog::OptionsDialog(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::MSWindowsFixedSizeDialogHint)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setupUi(this);

    connect(buttonApply, SIGNAL(clicked()), this, SLOT(apply()));

    QSettings settings;
    restoreGeometry(settings.value(QLatin1String("Geometry/OptionsDialog")).toByteArray());
}

OptionsDialog::~OptionsDialog()
{
    QSettings().setValue(QLatin1String("Geometry/OptionsDialog"), saveGeometry());
}

void OptionsDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);

    if (e->type() == QEvent::LanguageChange)
        retranslateUi(this);
}

void OptionsDialog::showEvent(QShowEvent *event)
{
    QSettings settings;
    foreach (QAbstractButton *widget, findChildren<QAbstractButton *>()) {
        QString option = widget->property("option").toString();
        if (!option.isEmpty())
            widget->setChecked(settings.value(option).toBool());
    }
    textProxy->setText(settings.value(QLatin1String("ProxyAddress")).toString());

    QDialog::showEvent(event);
}

void OptionsDialog::accept()
{
    if (apply())
        QDialog::accept();
}

bool OptionsDialog::apply()
{
    if (checkUseProxy->isChecked())    {
        QUrl proxyUrl(QLatin1String("http://") + textProxy->text());
        if (!proxyUrl.isValid() || proxyUrl.host().isEmpty() || proxyUrl.port(-1) == -1
                || !proxyUrl.path().isEmpty() || proxyUrl.hasQuery() || proxyUrl.hasFragment()) {
            //: combatlog_error
            MessageBox::exec(this, tr("Error"), tr("Invalid proxy address."));
            return false;
        }
    }

    QSettings settings;
    foreach (QAbstractButton *widget, findChildren<QAbstractButton *>()) {
        QString option = widget->property("option").toString();
        if (!option.isEmpty())
            settings.setValue(option, widget->isChecked());
    }
    settings.setValue(QLatin1String("ProxyAddress"), textProxy->text());

    qApp->settingsChanged();
    return true;
}
