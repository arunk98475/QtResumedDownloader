#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "filedownloader.h"
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow, public FileDownloadEvents
{
    Q_OBJECT
    FileDownloader mFilDownloader;
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
public:
    void onDownloadCompleted(const QString& outputFilePath) override;
    void onProgressChanged(float progress) override;
    void onError(const QString & error) override;

private slots:
    void on_pushBtnBrowse_clicked();
    void on_pushBtnDownload_clicked();


private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
