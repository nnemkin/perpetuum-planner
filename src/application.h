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

#ifndef APPLICATION_H
#define APPLICATION_H

#include <QEvent>
#include <QWeakPointer>
#include <QTranslator>
#include "QtSingleApplication"
#include "gdipp.h"

class QSettings;
class GameData;
class OptionsDialog;
class TextWindow;
class Updater;


class Application : public QtSingleApplication
{
    Q_OBJECT

public:
    enum { SettingsChanged = QEvent::User + 1 };

    Application(int &argc, char **argv);

    int exec();

    QString currentLanguage() { return m_currentLanguage; }
    void settingsChanged();

    QString docStyleSheet() { return m_docStyleSheet; }

public slots:
    void showOptions();
    void showHelp();
    void showAbout();

protected:
    void customEvent(QEvent *event);

private slots:
    void handleMessage(const QString &message);
    void updateCheckFinished();
    void launchUpdater();

private:
    bool m_updateMode;
    Updater *m_updater;
    GameData *m_gameData;

    bool m_winVista;

    Gdipp m_gdipp;
    bool m_oldBigFonts;

    QString m_docStyleSheet;

    QString m_currentLanguage;
    QTranslator m_translator, m_qtTranslator;

    void handleArguments(const QStringList &args);
    void checkForUpdates(QSettings &settings);
    void applySettings();

    bool m_settingsChangedPosted;

    QWeakPointer<OptionsDialog> m_optionsDialog;
    QWeakPointer<TextWindow> m_aboutWindow, m_helpWindow;

    QString resourceString(const QString &fileName);
};

#if defined(qApp)
#undef qApp
#endif
#define qApp (static_cast<Application *>(QCoreApplication::instance()))

#endif // APPLICATION_H
