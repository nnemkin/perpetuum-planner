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

#include "extensionwindow.h"
#include "gamedata.h"
#include "models.h"
#include "application.h"


ExtensionWindow::ExtensionWindow(Extension *extension, QWidget *parent)
    : QDialog(parent, Qt::Tool | Qt::WindowSystemMenuHint | Qt::FramelessWindowHint), m_extension(extension)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setupUi(this);

    if (!extension->requirements().isEmpty()) {
        RequirementsModel *model = new RequirementsModel(this);
        model->setRequirements(extension->requirements());
        treeRequirements->setModel(model);
        treeRequirements->expandAll();
    }
    else {
        tabWidget->removeTab(tabWidget->indexOf(tabRequirements));
    }

    updateExtensionInfo();

    QSettings settings;
    restoreGeometry(settings.value(QLatin1String("Geometry/ExtensionWindow")).toByteArray());
}

ExtensionWindow::~ExtensionWindow()
{
    QSettings().setValue(QLatin1String("Geometry/ExtensionWindow"), saveGeometry());
}

void ExtensionWindow::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi(this);

        updateExtensionInfo();
    }
}

void ExtensionWindow::customEvent(QEvent *event)
{
    if (event->type() == Application::SettingsChanged) {
        textDescription->document()->setDefaultStyleSheet(qApp->property("docStyleSheet").toString());
        textDescription->setHtml(textDescription->toHtml());
    }
}

void ExtensionWindow::updateExtensionInfo()
{
    //: window_extensioninfo
    setWindowTitle(tr("Extension: %1").arg(m_extension->name()));

    textDescription->document()->setDefaultStyleSheet(qApp->docStyleSheet());
    textDescription->document()->setDocumentMargin(10);

    textDescription->setText(QString("<html><p>%1</p></html>").arg(m_extension->description()));
}
