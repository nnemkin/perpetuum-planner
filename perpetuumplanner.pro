#-------------------------------------------------
# Project created by QtCreator 2010-06-21T14:43:27
#-------------------------------------------------

QT += core gui network

TARGET = PerpetuumPlanner
TEMPLATE = app

CONFIG += static

# DEFINES += QT_NO_CAST_FROM_ASCII

SOURCES += src/main.cpp\
    src/mainwindow.cpp \
    src/levelchooser.cpp \
    src/titlebar.cpp \
    src/optionsdialog.cpp \
    src/gamedata.cpp \
    src/attributeeditor.cpp \
    src/extensionsview.cpp \
    src/styletweaks.cpp \
    src/extensionwindow.cpp \
    src/messagebox.cpp \
    src/itemsview.cpp \
    src/delegates.cpp \
    src/models.cpp \
    src/util.cpp \
    3rdparty/qjson/src/serializerrunnable.cpp \
    3rdparty/qjson/src/serializer.cpp \
    3rdparty/qjson/src/qobjecthelper.cpp \
    3rdparty/qjson/src/parserrunnable.cpp \
    3rdparty/qjson/src/parser.cpp \
    3rdparty/qjson/src/json_scanner.cpp \
    3rdparty/qjson/src/json_parser.cc \
    src/agent.cpp \
    3rdparty/qtsingleapplication/src/qtsinglecoreapplication.cpp \
    3rdparty/qtsingleapplication/src/qtsingleapplication.cpp \
    3rdparty/qtsingleapplication/src/qtlockedfile_win.cpp \
    3rdparty/qtsingleapplication/src/qtlockedfile.cpp \
    3rdparty/qtsingleapplication/src/qtlocalpeer.cpp \
    src/application.cpp \
    3rdparty/gdi++/override.cpp \
    3rdparty/gdi++/gdipp.cpp \
    src/gridtreeview.cpp \
    src/modeltest.cpp \
    src/qthacks.cpp \
    src/updater.cpp \
    3rdparty/quazip/src/zip.c \
    3rdparty/quazip/src/unzip.c \
    3rdparty/quazip/src/quazipnewinfo.cpp \
    3rdparty/quazip/src/quazipfile.cpp \
    3rdparty/quazip/src/quazip.cpp \
    3rdparty/quazip/src/ioapi.c \
    src/tierfilter.cpp

HEADERS += src/mainwindow.h \
    src/levelchooser.h \
    src/gamedata.h \
    src/titlebar.h \
    src/optionsdialog.h \
    src/attributeeditor.h \
    src/versions.h \
    src/extensionsview.h \
    src/styletweaks.h \
    src/extensionwindow.h \
    src/messagebox.h \
    src/itemsview.h \
    src/delegates.h \
    src/models.h \
    src/util.h \
    src/agent.h \
    3rdparty/qjson/src/serializerrunnable.h \
    3rdparty/qjson/src/serializer.h \
    3rdparty/qjson/src/qobjecthelper.h \
    3rdparty/qjson/src/qjson_export.h \
    3rdparty/qjson/src/qjson_debug.h \
    3rdparty/qjson/src/parserrunnable.h \
    3rdparty/qjson/src/parser_p.h \
    3rdparty/qjson/src/parser.h \
    3rdparty/qjson/src/json_scanner.h \
    3rdparty/qtsingleapplication/src/qtsinglecoreapplication.h \
    3rdparty/qtsingleapplication/src/qtsingleapplication.h \
    3rdparty/qtsingleapplication/src/qtlockedfile.h \
    3rdparty/qtsingleapplication/src/qtlocalpeer.h \
    src/application.h \
    3rdparty/gdi++/override.h \
    3rdparty/gdi++/gdipp.h \
    3rdparty/gdi++/hooklist.h \
    src/gridtreeview.h \
    src/modeltest.h \
    src/updater.h \
    3rdparty/quazip/src/zip.h \
    3rdparty/quazip/src/unzip.h \
    3rdparty/quazip/src/quazipnewinfo.h \
    3rdparty/quazip/src/quazipfileinfo.h \
    3rdparty/quazip/src/quazipfile.h \
    3rdparty/quazip/src/quazip.h \
    3rdparty/quazip/src/ioapi.h \
    3rdparty/quazip/src/crypt.h \
    src/tierfilter.h

FORMS += src/mainwindow.ui \
    src/titlebar.ui \
    src/optionsdialog.ui \
    src/attributeeditor.ui \
    src/extensionsview.ui \
    src/extensionwindow.ui \
    src/messagebox.ui \
    src/itemsview.ui \
    src/textwindow.ui \
    src/updater.ui \
    src/tierfilter.ui

OTHER_FILES += \
    res/winres.rc \
    res/about.html \
    res/3rdparty.html \
    res/lgpl-2.1-standalone.html \
    res/gpl-3.0-standalone.html \
    res/stylesheet.css \
    res/stylesheet-big.css \
    res/doc-stylesheet.css \
    res/doc-stylesheet-big.css \
    res/defaultSettings.ini

RESOURCES += \
    res/resources.qrc \
    i18n/i18n.qrc \
    data/data.qrc

TRANSLATIONS = \
    i18n/perpetuumplanner_en.ts \
    i18n/perpetuumplanner_hu.ts \
    i18n/perpetuumplanner_de.ts \
    i18n/perpetuumplanner_ru.ts \
    i18n/perpetuumplanner_it.ts \
    i18n/perpetuumplanner_fr.ts \
    i18n/perpetuumplanner_sl.ts \
    i18n/perpetuumplanner_nl.ts \
    i18n/perpetuumplanner_pl.ts

RC_FILE = res/winres.rc

INCLUDEPATH += \
    inc \
    3rdparty/qtsingleapplication/src \
    3rdparty/qjson/src \
    3rdparty/gdi++ \
    3rdparty/quazip/src \
    3rdparty/zlib

QMAKE_RESOURCE_FLAGS += -threshold 0 -compress 9

static:QMAKE_LFLAGS += -FORCE:MULTIPLE -MAP

# temporary workaround for debug builds
shared:LIBS += zdll.lib shell32.lib user32.lib gdi32.lib
