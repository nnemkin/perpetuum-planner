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

#include <QSizeGrip>
#include <QSettings>
#include <QCloseEvent>
#include <QFile>

#include "mainwindow.h"
#include "optionsdialog.h"
#include "messagebox.h"
#include "agent.h"
#include "util.h"
#include "versions.h"
#include "application.h"


MainWindow::MainWindow(GameData *gameData, QString fileName, QWidget *parent)
    : QMainWindow(parent, Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint),
      m_gameData(gameData)
{
    setupUi(this);

    m_resizer = new QSizeGrip(this);
    m_resizer->resize(m_resizer->sizeHint());
    m_resizer->move(rect().bottomRight() -m_resizer->rect().bottomRight());
    m_resizer->raise();

    QSettings settings;

    connect(extensionsView, SIGNAL(closeRequest()), this, SLOT(close()));
    connect(extensionsView, SIGNAL(windowTitleChange(QString)), this, SLOT(setWindowTitle(QString)));
    extensionsView->initialize(settings, gameData, fileName);

    itemsView->initialize(settings, gameData);

    restoreGeometry(settings.value(QLatin1String("Geometry/MainWindow")).toByteArray());
    tabWidget->setCurrentIndex(settings.value(QLatin1String("ActiveTab")).toInt());

    addAction(actionHelp);

    connect(actionAbout, SIGNAL(triggered()), qApp, SLOT(showAbout()));
    connect(actionHelp, SIGNAL(triggered()), qApp, SLOT(showHelp()));
    connect(actionOptions, SIGNAL(triggered()), qApp, SLOT(showOptions()));

    updateLanguageSelector();
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);

    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi(this);

        updateLanguageSelector();
        break;
    }
}

void MainWindow::resizeEvent(QResizeEvent *)
{
    m_resizer->move(rect().bottomRight() -m_resizer->rect().bottomRight());
    m_resizer->raise();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (extensionsView->canClose()) {
        QSettings settings;
        settings.setValue(QLatin1String("Geometry/MainWindow"), saveGeometry());
        settings.setValue(QLatin1String("ActiveTab"), tabWidget->currentIndex());

        extensionsView->finalize(settings);
        itemsView->finalize(settings);
    }
    else {
        event->ignore();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString fileName = urls.at(0).toLocalFile();
        if (!fileName.isEmpty())
            extensionsView->agent()->load(fileName);
    }
}

void MainWindow::updateLanguageSelector()
{
    static const char *languageList[]= { "en", "hu", "de", "ru", "it" };
    for (int i = 0; i < listLanguage->count(); ++i)
        listLanguage->setItemData(i, languageList[i]);

    listLanguage->setCurrentIndex(listLanguage->findData(qApp->currentLanguage()));
}

void MainWindow::on_listLanguage_currentIndexChanged(int index)
{
    QString language = listLanguage->itemData(index).toString();
    if (language != qApp->currentLanguage()) { // avoid double fire from updateLanguageSelector()
        QSettings().setValue(QLatin1String("Language"), language);
        qApp->settingsChanged();
    }
}
