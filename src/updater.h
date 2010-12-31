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

#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <QNetworkAccessManager>
#include <QTimer>
#include <QUrl>
#include "ui_updater.h"

class QTemporaryFile;


class Updater : public QDialog, private Ui::Updater
{
    Q_OBJECT

public:
    explicit Updater(QString fileName, QUrl url, QWidget *parent = 0);

    void reject();

protected:
    void changeEvent(QEvent *e);

private slots:
    void waitApplicationClose();
    void replyFinished();
    void replyReadyRead();
    void replyProgress(qint64 bytesRead, qint64 totalBytes);

private:
    QTimer m_waitClose;
    QNetworkAccessManager m_networkAccess;

    QUrl m_downloadUrl;
    QString m_targetFileName;
    QTemporaryFile *m_tempFile;
    QNetworkReply *m_reply;
    bool m_aborted;

    void startDownload(QUrl url);
    bool unpack(const QString& zipFileName);
};

#endif // DOWNLOADER_H
