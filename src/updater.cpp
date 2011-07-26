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

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTemporaryFile>
#include <QVarLengthArray>
#include <QDir>
#include <QUuid>
#include <QStringBuilder>
#include <QTimer>
#include <QProcess>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef MessageBox

#include "messagebox.h"
#include "updater.h"
#include "versions.h"
#include "quazip.h"
#include "quazipfile.h"


Updater::Updater(QString fileName, QUrl url, QWidget *parent) :
    QDialog(parent, Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::MSWindowsFixedSizeDialogHint),
    m_reply(0), m_tempFile(0), m_aborted(false)
{
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, true);

    m_downloadUrl = url;
    m_targetFileName = fileName;

    labelMessage->setText(tr("Waiting for application to close..."));

    connect(&m_waitClose, SIGNAL(timeout()), this, SLOT(waitApplicationClose()));
    m_waitClose.start(333);
}

void Updater::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);

    if (e->type() == QEvent::LanguageChange)
        retranslateUi(this);
}

void Updater::waitApplicationClose()
{
    QFile targetFile(m_targetFileName);
    if (targetFile.open(QIODevice::ReadWrite)) {
        targetFile.close();
        m_waitClose.stop();

        startDownload(m_downloadUrl);
    }
}

void Updater::startDownload(QUrl url)
{
    labelMessage->setText(tr("Downloading %1 MB...").arg(0));

    m_tempFile = new QTemporaryFile();
    m_tempFile->open();

    m_reply = m_networkAccess.get(QNetworkRequest(url));
    connect(m_reply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(m_reply, SIGNAL(readyRead()), this, SLOT(replyReadyRead()));
    connect(m_reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(replyProgress(qint64,qint64)));
}

void Updater::reject()
{
    if (m_reply) {
        m_aborted = true;
        m_reply->abort();
    }

    QDialog::reject();
}

void Updater::replyFinished()
{
    if (!m_aborted) {
        if (m_reply->error()) {
            MessageBox::exec(this, tr("Error"), tr("Download failed:\n%1").arg(m_reply->errorString()));
        }
        else {
            QVariant redirectionTarget = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
            if (!redirectionTarget.isNull()) {
                m_tempFile->resize(0);
                startDownload(m_reply->url().resolved(redirectionTarget.toUrl()));
                return;
            }
            else {
                // success, replace the file
                labelMessage->setText(tr("Updating..."));

                m_tempFile->close();
                if (unpack(m_tempFile->fileName()))
                    QProcess::startDetached(m_targetFileName, QStringList());
                else
                    MessageBox::exec(this, tr("Error"), tr("Unable to replace program file."));
            }
        }
    }

    m_reply->deleteLater();
    m_reply = 0;
    delete m_tempFile;
    m_tempFile = 0;
    reject();
}

void Updater::replyReadyRead()
{
    if (m_tempFile)
        m_tempFile->write(m_reply->readAll());
}

void Updater::replyProgress(qint64 bytesRead, qint64 totalBytes)
{
    const double MB = 1. / (1024 * 1024);

    if (totalBytes > 0) {
        labelMessage->setText(tr("Downloading %1 MB of %2 MB...").arg(bytesRead * MB, 0, 'f', 2).arg(totalBytes * MB, 0, 'f', 2));
        progressBar->setMaximum(totalBytes);
        progressBar->setValue(bytesRead);
    }
    else {
        labelMessage->setText(tr("Downloading %1 MB...").arg(bytesRead * MB, 0, 'f', 2));
        progressBar->setMaximum(0);
    }
}

bool Updater::unpack(const QString& zipFileName)
{
    QuaZipFile inFile(zipFileName, QLatin1String(VER_ORIGINALFILENAME_STR));
    if (!inFile.open(QIODevice::ReadOnly))
        return false;

    QFile outFile(m_targetFileName);
    if (!outFile.open(QIODevice::WriteOnly))
        return false;

    char buf[4096];
    int read = 0;
    while ((read = inFile.read(buf, sizeof(buf))) > 0)
        outFile.write(buf, read);

    if (read == -1)
        return false;

    return true;
}
