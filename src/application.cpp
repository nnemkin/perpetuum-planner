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
#include <QFontDatabase>
#include <QFile>
#include <QTemporaryFile>
#include <QDir>
#include <QUuid>
#include <QSettings>
#include <QTranslator>
#include <QPixmapCache>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QStringBuilder>
#include <QDateTime>
#include <QProgressDialog>
#include <QDesktopServices>

#include "mainwindow.h"
#include "optionsdialog.h"
#include "gamedata.h"
#include "styletweaks.h"
#include "messagebox.h"
#include "updater.h"
#include "versions.h"
#include "util.h"
#include "application.h"

// NB: windows.h has some nasty defines and is therefore included last
#include <private/qfont_p.h>  // QFontCache
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#undef MessageBox  // conflicts with our own MessageBox class


Application::Application(int &argc, char **argv)
    : QtSingleApplication(argc, argv), m_winVista(false), m_settingsChangedPosted(false), m_oldBigFonts(false), m_updateMode(false), m_updater(0)
{
    QApplication::setStyle(new StyleTweaks());

    extern Q_GUI_EXPORT bool qt_cleartype_enabled;
    qt_cleartype_enabled = true; // this var affects glyph cache buffer type, without it even grayscale AA won't work properly

    m_gameData = new GameData(this);

    OSVERSIONINFO osvi;
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osvi);

    m_winVista = osvi.dwMajorVersion >= 6;
}

int Application::exec()
{
    QStringList args = arguments();

    if (sendMessage(args.join(QLatin1String("\n"))))
        return 0;

    connect(this, SIGNAL(messageReceived(QString)), this, SLOT(handleMessage(QString)), Qt::QueuedConnection);

    QFontDatabase::addApplicationFont(":/uni05_53.ttf");
    QFontDatabase::addApplicationFont(":/MyriadPro-BoldCond.otf");

    setApplicationName(VER_PRODUCTNAME_STR);
    setApplicationVersion(VER_PRODUCTVERSION_STR);
    setOrganizationName(VER_PRODUCTNAME_STR); // for QSettings only
    QSettings::setDefaultFormat(QSettings::IniFormat);

    m_updateMode = (args.size() == 3) && (args.at(1) == QLatin1String("-update"));

    applySettings();

    if (m_updateMode) {
        m_updater = new Updater(args.at(2), QUrl(VER_DOWNLOAD_URL));
        m_updater->show();
    }
    else {
        QFile gameDataFile(":/compiled/game.json"), translationFile(":/compiled/lang_en.json");
        if (!m_gameData->load(&gameDataFile, &translationFile)) {
            MessageBox::exec(0, QApplication::translate("Main", "Error"), tr("Invalid data file."));
            return -1;
        }
        handleArguments(arguments());
    }

    return QtSingleApplication::exec();
}

void Application::checkForUpdates(QSettings &settings)
{
    if (settings.value("CheckForUpdates").toBool()) {
        QDateTime lastCheckTime = settings.value("LastUpdateCheck").toDateTime();

        if (settings.value("Debug").toBool() || lastCheckTime.secsTo(QDateTime::currentDateTime()) > 84600) {
            QNetworkAccessManager *manager = new QNetworkAccessManager(this);

            QUrl verCheckUrl;
            if (settings.value("Debug").toBool())
                verCheckUrl = settings.value("DebugVersionCheckUrl").toUrl();
            else
                verCheckUrl.setUrl(QLatin1String(VER_CHECK_URL));

            QNetworkRequest request(verCheckUrl);
            request.setRawHeader("User-Agent", VER_PRODUCTNAME_STR "/" VER_PRODUCTVERSION_STR);

            QNetworkReply *reply = manager->get(request);
            connect(reply, SIGNAL(finished()), this, SLOT(updateCheckFinished()));
        }
    }
    else {
        settings.remove("LastUpdateCheck");
    }
}

void Application::updateCheckFinished()
{
    QNetworkReply *reply = static_cast<QNetworkReply *>(sender());
    if (reply->error() == QNetworkReply::NoError) {
        QString message = QString::fromUtf8(reply->readAll());

        QString latestVersion = message.section(QLatin1Char('\n'), 0, 0).trimmed();
        QString latestDate = message.section(QLatin1Char('\n'), 1, 1).trimmed();

        if (compareVersion(latestVersion, VER_PRODUCTVERSION_STR) > 0) {
            TextWindow *window = TextWindow::show(0, tr("New Version Available"),
                tr("<h3>New version available: %1 (%2)</h3>"
                   "<p>Press <b>Update</b> button to download and update automatically.</p>"
                   "<p>Visit the <a href=\"http://www.perpetuum-planner.com/\">home page</a> to read about the latest changes."
                   "<p>Note: you can disable automatic update checks in the options.</p>")
                    .arg(latestVersion, latestDate), TextWindow::Small, "NewVersionWindow").data();
            window->buttons()->setStandardButtons(QDialogButtonBox::Cancel);
            window->buttons()->addButton(tr("Update"), QDialogButtonBox::AcceptRole);
            connect(window, SIGNAL(accepted()), this, SLOT(launchUpdater()));
        }

        QSettings settings;
        settings.setValue("LastUpdateCheck", QDateTime::currentDateTime());
    }
    reply->deleteLater();
    reply->manager()->deleteLater();
}

void Application::handleMessage(const QString &message)
{
    if (m_updater) {
        m_updater->activateWindow();
        m_updater->raise();
    }
    else {
        handleArguments(message.split(QLatin1Char('\n')));
    }
}

void Application::handleArguments(const QStringList &args)
{
    QString fileName;
    if (args.size() > 1)
        fileName = args.at(1);

    MainWindow *mainWindow = new MainWindow(m_gameData, fileName);
    mainWindow->show();
    mainWindow->activateWindow();
    mainWindow->raise();
}

void Application::settingsChanged()
{
    if (!m_settingsChangedPosted) {
        m_settingsChangedPosted = true;
        postEvent(this, new QEvent(static_cast<QEvent::Type>(SettingsChanged)));
    }
}

static void broadcastEvent(QWidget *root, QEvent *event)
{
    QApplication::sendEvent(root, event);

    foreach (QObject *child, root->children()) {
        QWidget *w = qobject_cast<QWidget *>(child);
        if (w && !w->isWindow())
            broadcastEvent(w, event);
    }
}

void Application::customEvent(QEvent *event)
{
    if (event->type() == SettingsChanged) {
        m_settingsChangedPosted = false;

        applySettings();

        foreach (QWidget *widget, QApplication::topLevelWidgets())
            broadcastEvent(widget, event);
    }
}

QString Application::resourceString(const QString &fileName)
{
    QString data;
    QFile file(fileName);
    if (file.open(QFile::ReadOnly))
        data = file.readAll();
    return data;
}

void Application::ensureDefaultSettings(QSettings &settings)
{
    // Make sure some principal settigns have sane default value.

    QSettings defaultSettings(":/defaultSettings.ini", QSettings::IniFormat);
    foreach (const QString &key, defaultSettings.allKeys())
        if (!settings.contains(key))
            settings.setValue(key, defaultSettings.value(key));
}

void Application::applySettings()
{
    QSettings settings;

    ensureDefaultSettings(settings);

    bool bigFonts = settings.value("BigFonts").toBool();
    bool updateStyleSheet = styleSheet().isEmpty() || m_oldBigFonts != bigFonts;
    m_oldBigFonts = bigFonts;

    QString langCode = settings.value(QLatin1String("Language")).toString();
    if (m_currentLanguage != langCode) {
        m_currentLanguage = langCode;

        removeTranslator(&m_translator);
        removeTranslator(&m_qtTranslator);
        if (m_qtTranslator.load(QLatin1Literal("qt_") % langCode, QLatin1String(":/i18n")))
            installTranslator(&m_qtTranslator);
        if (m_translator.load(QLatin1Literal("perpetuumplanner_") % langCode, QLatin1String(":/i18n")))
            installTranslator(&m_translator);

        QFile transFile(QString(":/compiled/lang_%1.json").arg(langCode));
        m_gameData->loadTranslation(langCode, &transFile);

        QPixmapCache::clear();
    }

    bool enableClearType = settings.value("EnableClearType").toBool();
    if (enableClearType != m_gdipp.enableClearType()) {
        m_gdipp.setEnableClearType(enableClearType);
#ifndef QT_DLL
        QFontCache::instance()->clear(); // XXX: Qt private
#endif
        updateStyleSheet = true;
    }

    if (updateStyleSheet) {
        QPixmapCache::clear();

        QString styleSheet = resourceString(QLatin1String(":/stylesheet.css"));
        if (bigFonts)
            styleSheet += resourceString(QLatin1String(":/stylesheet-big.css"));
        setStyleSheet(styleSheet);

        m_docStyleSheet = resourceString(QLatin1String(":/doc-stylesheet.css"));
        if (bigFonts)
            m_docStyleSheet += resourceString(QLatin1String(":/doc-stylesheet-big.css"));
    }

    QNetworkProxy proxy(QNetworkProxy::NoProxy);
    if (settings.value("UseProxy").toBool()) {
        QUrl proxyUrl("http://" + settings.value("ProxyAddress").toString());
        proxy.setType(settings.value("SocksProxy").toBool() ? QNetworkProxy::Socks5Proxy : QNetworkProxy::HttpProxy);
        proxy.setUser(proxyUrl.userName());
        proxy.setPassword(proxyUrl.password());
        proxy.setHostName(proxyUrl.host());
        proxy.setPort(proxyUrl.port());
    }
    QNetworkProxy::setApplicationProxy(proxy);

    if (!m_updateMode) {
        setFileAssociations(settings);
        checkForUpdates(settings);
    }
}

void Application::setFileAssociations(QSettings &settings)
{
    QSettings hkcr(m_winVista ? "HKEY_CURRENT_USER\\Software\\Classes" : "HKEY_CLASSES_ROOT", QSettings::NativeFormat);
    if (settings.value("AssociateFiles").toBool()) {
        if (hkcr.value(VER_PROGID "/.") != QLatin1String(VER_PRODUCTNAME_STR " " VER_PRODUCTVERSION_STR)) {
            hkcr.setValue(VER_PROGID "/.", VER_PRODUCTNAME_STR " " VER_PRODUCTVERSION_STR);
            hkcr.setValue(VER_PROGID "/FriendlyTypeName/.", tr("Perpetuum Agent"));

            QString binaryPath = QDir::toNativeSeparators(QApplication::applicationFilePath());
            hkcr.setValue(VER_PROGID "/DefaultIcon/.", binaryPath + ",0");
            hkcr.setValue(VER_PROGID "/shell/open/command/.", QVariant(QLatin1Literal("\"") % binaryPath % QLatin1Literal("\" \"%1\"")));
        }
        if (!hkcr.value(".agent/.").isValid()) {
            hkcr.setValue(".agent/.", VER_PROGID);

            if (hkcr.status() == QSettings::NoError)
                SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
        }
    }
    else {
        if (hkcr.value(".agent/.") == QLatin1String(VER_PROGID)) {
            hkcr.remove(".agent/.");
            if (hkcr.status() == QSettings::NoError) {
                hkcr.remove(VER_PROGID);
                SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
            }
        }
    }
}

void Application::showOptions()
{
    if (m_optionsDialog.isNull()) {
        m_optionsDialog = new OptionsDialog(activeWindow());
        m_optionsDialog.data()->show();
    }
    else {
        m_optionsDialog.data()->activateWindow();
        m_optionsDialog.data()->raise();
    }
}

void Application::showHelp()
{
    QDesktopServices::openUrl(QUrl(VER_HELP_URL));
}

void Application::showAbout()
{
    if (m_aboutWindow.isNull()) {
        QString about = resourceString(":/about.html")
            .arg(VER_PRODUCTNAME_STR, VER_PRODUCTVERSION_STR, VER_PRODUCTVERSION_DATE, VER_LEGALCOPYRIGHT_STR, qVersion());
        m_aboutWindow = TextWindow::show(activeWindow(), tr("About"), about, TextWindow::Medium, "AboutWindow");
    }
    else {
        m_aboutWindow.data()->activateWindow();
        m_aboutWindow.data()->raise();
    }
}

void Application::launchUpdater()
{
    closeAllWindows();

    QString tmpExeName = QDir::tempPath() % QDir::separator() % QLatin1Literal("PerpetuumPlannerUpdate.exe");
    QString tmpExeArgs = QLatin1Literal("-update \"") % QApplication::applicationFilePath() % QLatin1Literal("\"");

    LPCWSTR pszVerb = 0;
    if (m_winVista) {
        // try to elevate if we do not have write permission in the application dir
        QTemporaryFile tmpFile(QApplication::applicationDirPath() % QDir::separator());
        if (!tmpFile.open())
            pszVerb = L"runas";
    }

    // copy oneself to temp dir and launch with "-update <exepath>" arguments.
    QFile::remove(tmpExeName);
    if (QFile::copy(QApplication::applicationFilePath(), tmpExeName)) {
        HINSTANCE result = ShellExecute(0, pszVerb, (LPCWSTR) tmpExeName.utf16(), (LPCWSTR) tmpExeArgs.utf16(), 0, SW_SHOWNORMAL);
        if ((int) result > 32) {
            quit();
            return;
        }
    }

    MessageBox::show(activeWindow(), tr("Error"), tr("Failed to launch automatic updater."));
}
