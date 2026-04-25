#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , mFilDownloader(this)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(&mFilDownloader,&FileDownloader::onDownloadCompleted,this , &MainWindow::onDownloadCompleted);
    connect(&mFilDownloader,&FileDownloader::onProgressChanged,this , &MainWindow::onProgressChanged);
    connect(&mFilDownloader,&FileDownloader::onError,this , &MainWindow::onError);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onDownloadCompleted()
{

}

void MainWindow::onProgressChanged(float progress)
{
    ui->progressBar->valueChanged((int)progress);
}

void MainWindow::onError(const QString & error)
{
    ui->statusbar->showMessage(error);
}


void MainWindow::on_pushBtnBrowse_clicked()
{

}


void MainWindow::on_pushBtnDownload_clicked()
{

}

