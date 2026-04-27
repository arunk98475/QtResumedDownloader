#include "filedownloader.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

FileDownloader::FileDownloader(QObject *parent,FileDownloadEvents * events)
    : QObject{parent},mFileDownloadEvents(events)
{
    mRetryTimer.setSingleShot(true);
    connect(&mRetryTimer, &QTimer::timeout, this, &FileDownloader::startOrResume);
}

void FileDownloader::download(const QString& url, const QString& outPutFolder)
{
    mUrl = url.trimmed();
    mOutputFolder = outPutFolder.trimmed();
    resetStateForNewDownload();
    startOrResume();
}

void FileDownloader::abort()
{
    mRetryTimer.stop();
    mAborted = true;

    cleanupReply();

    if (mFile) {
        if (mFile->isOpen())
            mFile->close();
        delete mFile;
        mFile = nullptr;
    }

    if (!mPartPath.isEmpty())
        QFile::remove(mPartPath);

    mUrl.clear();
    mOutputFolder.clear();
    mFinalPath.clear();
    mPartPath.clear();

    if (mFileDownloadEvents)
        mFileDownloadEvents->onError(QStringLiteral("Download aborted."));
}

void FileDownloader::resetStateForNewDownload()
{
    cleanupReply();
    if (mFile) {
        if (mFile->isOpen()) mFile->close();
        delete mFile;
        mFile = nullptr;
    }

    mAborted = false;

    mFinalPath = targetFilePath();
    mPartPath = partFilePath();

    mResumeOffset = 0;
    mTotalExpectedBytes = -1;
    mRetryCount = 0;
    mTriedRestartWithoutRange = false;
}

QString FileDownloader::derivedFileName() const
{
    QUrl qurl(mUrl);
    QString name = QFileInfo(qurl.path()).fileName();
    if (name.isEmpty()) name = "download.bin";
    return name;
}

QString FileDownloader::targetFilePath() const
{
    QDir dir(mOutputFolder);
    return dir.filePath(derivedFileName());
}

QString FileDownloader::partFilePath() const
{
    return targetFilePath() + ".part";
}

qint64 FileDownloader::existingPartSize() const
{
    QFileInfo fi(mPartPath);
    if (!fi.exists()) return 0;
    return fi.size();
}

void FileDownloader::cleanupReply()
{
    if (!mReply) return;
    mReply->disconnect(this);
    // Avoid aborting a reply that already finished/errored, which can trigger
    // Qt internal warnings on some versions/platforms.
    if (mReply->isRunning())
        mReply->abort();
    mReply->deleteLater();
    mReply = nullptr;
}

void FileDownloader::scheduleRetry(const QString& reason)
{
    if (mAborted)
        return;

    cleanupReply();
    // Close the output file between attempts so the next retry can reopen cleanly
    // and so we don't keep a stale handle around after network errors.
    if (mFile && mFile->isOpen()) {
        mFile->flush();
        mFile->close();
    }

    const int capped = qMin(mRetryCount, 6);
    const int delayMs = 1000 * (1 << capped); // 1s,2s,4s..64s
    mRetryCount++;

    mFileDownloadEvents->onError(QString("%1. Retrying in %2s...").arg(reason).arg(delayMs / 1000));
    mRetryTimer.start(delayMs);
}

void FileDownloader::startOrResume()
{
    if (mAborted)
        return;

    if (mUrl.isEmpty()) {
        mFileDownloadEvents-> onError("URL is empty.");
        return;
    }
    if (mOutputFolder.isEmpty()) {
        mFileDownloadEvents-> onError("Output folder is empty.");
        return;
    }

    QDir outDir(mOutputFolder);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        mFileDownloadEvents-> onError("Cannot create output folder.");
        return;
    }

    mFinalPath = targetFilePath();
    mPartPath = partFilePath();

    mResumeOffset = existingPartSize();

    if (!mFile) mFile = new QFile(mPartPath);
    if (mFile->isOpen())
        mFile->close();
    if (!mFile->open(QIODevice::ReadWrite)) {
        const QString detail = mFile ? mFile->errorString() : QString("unknown error");
        // If this failure is transient (e.g., folder temporarily unavailable), keep retrying.
        // Also reset the QFile so the next attempt re-creates it cleanly.
        if (mFile) {
            if (mFile->isOpen()) mFile->close();
            delete mFile;
            mFile = nullptr;
        }
        scheduleRetry(QString("Cannot open output file for writing (%1)").arg(detail));
        return;
    }
    if (!mFile->seek(mResumeOffset)) {
        const QString detail = mFile ? mFile->errorString() : QString("unknown error");
        // Treat as potentially transient (file system hiccup) and retry.
        scheduleRetry(QString("Cannot seek output file (%1)").arg(detail));
        return;
    }

    // Avoid MSVC "most vexing parse" (treating this as a function declaration)
    QNetworkRequest req{QUrl(mUrl)};
    // Qt5/Qt6 compatible redirect handling
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    if (mResumeOffset > 0 && !mTriedRestartWithoutRange) {
        req.setRawHeader("Range", QByteArray("bytes=") + QByteArray::number(mResumeOffset) + "-");
    }

    cleanupReply();
    mReply = mNam.get(req);

    connect(mReply, &QNetworkReply::readyRead, this, [this]() {
        if (mAborted || !mReply || !mFile) return;
        const QByteArray chunk = mReply->readAll();
        if (!chunk.isEmpty()) {
            const qint64 written = mFile->write(chunk);
            if (written != chunk.size()) {
                scheduleRetry("Disk write failed");
            }
        }
    });

    connect(mReply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (mAborted)
            return;
        // received/total are for the current reply; adjust with resume offset if resuming
        qint64 fullTotal = -1;
        if (total > 0) fullTotal = mResumeOffset + total;
        if (mTotalExpectedBytes > 0) fullTotal = mTotalExpectedBytes;

        if (fullTotal > 0) {
            const qint64 fullReceived = mResumeOffset + received;
            const float pct = (fullReceived * 100.0f) / (float)fullTotal;
            mFileDownloadEvents-> onProgressChanged(pct);
        }
    });

    connect(mReply, &QNetworkReply::errorOccurred, this, [this](QNetworkReply::NetworkError) {
        if (mAborted || !mReply)
            return;
        scheduleRetry(mReply->errorString());
    });

    connect(mReply, &QNetworkReply::finished, this, [this]() {
        if (mAborted || !mReply || !mFile)
            return;

        // Ensure last bytes are written.
        const QByteArray chunk = mReply->readAll();
        if (!chunk.isEmpty()) mFile->write(chunk);
        mFile->flush();

        const QVariant statusV = mReply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const int status = statusV.isValid() ? statusV.toInt() : 0;

        // If file is already fully downloaded, some servers respond with 416 on Range requests.
        if (status == 416 && mResumeOffset > 0) {
            mFile->close();
            QFile::remove(mFinalPath);
            if (QFile::rename(mPartPath, mFinalPath)) {
                mFileDownloadEvents-> onProgressChanged(100.0f);
                mFileDownloadEvents-> onDownloadCompleted(mFinalPath);
            } else {
                mFileDownloadEvents-> onError("Download appears complete (416), but could not rename .part file.");
            }
            cleanupReply();
            return;
        }

        // Detect server ignoring Range (status 200 when we requested resume).
        if (mResumeOffset > 0 && !mTriedRestartWithoutRange && status == 200) {
            // Server didn't resume; restart cleanly.
            mTriedRestartWithoutRange = true;
            mFile->close();
            mFile->open(QIODevice::WriteOnly | QIODevice::Truncate);
            mFile->close();
            mFileDownloadEvents-> onError("Server does not support resume; restarting download from beginning.");
            mRetryCount = 0;
            startOrResume();
            return;
        }

        if (mReply->error() != QNetworkReply::NoError) {
            // errorOccurred already scheduled retry; just return
            cleanupReply();
            return;
        }

        // If Content-Range exists, parse total size.
        const QByteArray contentRange = mReply->rawHeader("Content-Range"); // e.g. bytes 100-199/1000
        if (!contentRange.isEmpty()) {
            QRegularExpression re(R"(bytes\s+(\d+)-(\d+)/(\d+|\*))");
            const auto m = re.match(QString::fromLatin1(contentRange));
            if (m.hasMatch() && m.captured(3) != "*") {
                mTotalExpectedBytes = m.captured(3).toLongLong();
            }
        } else {
            const qint64 len = mReply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
            if (len > 0 && status == 200) mTotalExpectedBytes = len;
            if (len > 0 && status == 206) mTotalExpectedBytes = mResumeOffset + len;
        }

        mFile->close();

        // Move .part to final.
        QFile::remove(mFinalPath); // overwrite if exists
        if (!QFile::rename(mPartPath, mFinalPath)) {
            mFileDownloadEvents-> onError("Download finished but could not rename .part file.");
            cleanupReply();
            return;
        }

        mFileDownloadEvents-> onProgressChanged(100.0f);
        mFileDownloadEvents-> onDownloadCompleted(mFinalPath);
        cleanupReply();
    });
}

