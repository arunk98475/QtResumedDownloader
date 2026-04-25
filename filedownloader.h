#ifndef FILEDOWNLOADER_H
#define FILEDOWNLOADER_H

#include <QObject>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>

class FileDownloader : public QObject
{
    Q_OBJECT
public:
    explicit FileDownloader(QObject *parent = nullptr);
    void download(const QString& url, const QString& outPutFolder);

private:
    void startOrResume();
    void cleanupReply();
    void scheduleRetry(const QString& reason);
    void resetStateForNewDownload();

    QString derivedFileName() const;
    QString targetFilePath() const;
    QString partFilePath() const;
    qint64 existingPartSize() const;

signals:

    void onProgressChanged(float val);
    void onDownloadCompleted();
    void onError(const QString& error);

private:
    QNetworkAccessManager mNam;
    QPointer<QNetworkReply> mReply;
    QTimer mRetryTimer;

    QString mUrl;
    QString mOutputFolder;
    QString mFinalPath;
    QString mPartPath;

    QFile* mFile = nullptr;
    qint64 mResumeOffset = 0;
    qint64 mTotalExpectedBytes = -1; // full file size if known
    int mRetryCount = 0;
    bool mTriedRestartWithoutRange = false;

};

#endif // FILEDOWNLOADER_H
