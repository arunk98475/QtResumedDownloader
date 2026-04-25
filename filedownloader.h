#ifndef FILEDOWNLOADER_H
#define FILEDOWNLOADER_H

#include <QObject>

class FileDownloader : public QObject
{
    Q_OBJECT
public:
    explicit FileDownloader(QObject *parent = nullptr);
    void download(QString & url, QString & outPutFolder);
signals:

    void onProgressChanged(float val);
    void onDownloadCompleted();
    void onError(const QString& error);

};

#endif // FILEDOWNLOADER_H
