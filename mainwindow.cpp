#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , mFilDownloader(this,this)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onDownloadCompleted(const QString& outputFilePath)
{
    ui->statusbar->showMessage(QStringLiteral("Download completed: %1").arg(outputFilePath));
    QFile::remove(outputFilePath);
}

void MainWindow::onProgressChanged(float progress)
{
    ui->progressBar->setValue(qBound(0, (int)progress, 100));
    ui->statusbar->showMessage(QString("Downloading... %1%").arg((int)progress));
}

void MainWindow::onError(const QString & error)
{
    ui->statusbar->showMessage(error);
}


void MainWindow::on_pushBtnBrowse_clicked()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        "Select download folder",
        ui->lineEditDownloadLocation->text().trimmed()
    );
    if (!dir.isEmpty())
        ui->lineEditDownloadLocation->setText(dir);
}


void MainWindow::on_pushBtnDownload_clicked()
{
    _sleep(3000);
    const QString url = ui->lineEditFileUrl->text().trimmed();
    const QString folder = ui->lineEditDownloadLocation->text().trimmed();

    if (url.isEmpty()) {
        ui->statusbar->showMessage("Please enter a download URL.");
        return;
    }
    const QUrl qurl(url);
    if (!qurl.isValid() || (qurl.scheme() != "http" && qurl.scheme() != "https")) {
        ui->statusbar->showMessage("Please enter a valid http/https URL.");
        return;
    }
    if (folder.isEmpty()) {
        ui->statusbar->showMessage("Please choose a download folder.");
        return;
    }

    ui->progressBar->setValue(0);
    ui->statusbar->showMessage("Starting download...");
    mFilDownloader.download(url, folder);
}

