#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "parser.h"
#include "exec.h"
#include <QDropEvent>   // for drag-drop
#include <QMimeData>    // for drag-drop
#include <QFileDialog>  // for file browsing
#include <QFile>        // for file I/O
#include <QMessageBox>  // for warning/error dialog
#include <QInputDialog> // for URL dialog
#include <QScrollBar>   // for scrolling disp

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{  
    ui->setupUi(this);

    connect(ui->browseButton, &QPushButton::pressed, this, &MainWindow::browseFile);
    connect(ui->runButton,    &QPushButton::pressed, this, &MainWindow::runButtonPressed);
    connect(ui->stopButton,   &QPushButton::pressed, this, &MainWindow::stopButtonPressed);
    connect(ui->saveButton,   &QPushButton::pressed, this, &MainWindow::saveButtonPressed);
    connect(ui->urlButton,    &QPushButton::pressed, this, &MainWindow::pullUrl);
    connect(&webCtrl, &QNetworkAccessManager::finished, this, &MainWindow::downloaded);

    keepRunning = false;
    paused = false;
    ptrExec = NULL;

    setAcceptDrops(true);
    this->setWindowTitle(PROG_NAME+" "+PROG_VERSION);
}

MainWindow::~MainWindow()
{
    keepRunning = false;
    paused = false;
    delete ui;
}

/////////////////////////////////////////////////////////
///// UI STUFF

void MainWindow::displayTxt(QString str)
{
  ui->disp->setHtml(str);
  ui->disp->verticalScrollBar()->setValue(ui->disp->verticalScrollBar()->maximum());
  ui->disp->repaint();
  QApplication::processEvents();
}

void MainWindow::displayStats(QString status, QString users, QString real, QString viable, QString trades, QString iter)
{
  ui->statusLabel->setText(status);

  ui->usersLabel->setText(users);

  ui->itemsLabel->setText(real);
  ui->viableLabel->setText(viable);
  ui->tradesLabel->setText(trades);

  ui->iterLabel->setText(iter);
}

void MainWindow::setBarFormat(QString fmt, int max)
{
  ui->progressBar->setFormat(fmt);
  ui->progressBar->setMaximum(max);
  ui->progressBar->setValue(0);
}
void MainWindow::setBarVal(int val)
{
  ui->progressBar->setValue(val);
}

void MainWindow::runComplete(QString results)
{
  ui->progressBar->setEnabled(false);
  ui->runButton->setText("Run");
  ui->runButton->setEnabled(true);
  ui->stopButton->setEnabled(false);
  ui->saveButton->setEnabled(true);
  ui->browseButton->setEnabled(true);
  ui->urlButton->setEnabled(true);
  displayTxt(results);
  delete ptrExec;
  ptrExec = NULL;
  if (!keepRunning)
    ui->progressBar->reset();
  keepRunning = false;
}


/////////////////////////////////////////////////////////
///// SLOTS

void MainWindow::runButtonPressed()
{
  if (ui->runButton->text() == "Pause")
  {
    paused = true;
    ui->progressBar->setEnabled(false);
    ui->runButton->setText("Resume");
    return;
  }
  if (ui->runButton->text() == "Resume")
  {
    paused = false;
    ui->progressBar->setEnabled(true);
    ui->runButton->setText("Pause");
    return;
  }

  keepRunning = true;
  paused = false;
  ui->runButton->setText("Pause");
  ui->stopButton->setEnabled(true);
  ui->progressBar->setEnabled(true);
  ui->browseButton->setEnabled(false);
  ui->urlButton->setEnabled(false);
  ui->saveButton->setEnabled(false);

  ptrExec = new Exec(this);
  ptrExec->go(input, filename, &keepRunning, &paused);
}

void MainWindow::stopButtonPressed()
{
  // are you sure??
  ui->progressBar->reset();
  ui->runButton->setEnabled(false);
  ui->stopButton->setEnabled(false);
  paused = false; // unpause threads so they can exit
  keepRunning = false;
}

void MainWindow::saveButtonPressed()
{
  QString filename = QFileDialog::getSaveFileName(this,"Save trade results", "", "Text files (*.txt)");
  if (filename == "")
    return;
  QFile file(filename);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    QMessageBox::critical(this, "Error", "Could not save file");
    return;
  }
  file.write(ui->disp->toPlainText().toLatin1());
  file.close();
}


void MainWindow::about()
{
  //aboutDialog abt(this);
  //abt.display();
}

void MainWindow::browseFile()
{
  filename = QFileDialog::getOpenFileName(this, "Open File", "", "Text files (*.txt);;All Files (*.*)");
  readFile();
}

void MainWindow::pullUrl()
{
  static QString lastUrl;
  QInputDialog dialog;
  bool ok;

  // Get the URL from the user
  lastUrl = dialog.getText(this,"Open URL", "Enter URL starting with http://...      ", QLineEdit::Normal, lastUrl, &ok);
  if (!ok || lastUrl.isEmpty())
    return;
  filename=lastUrl;

  // Request the page
  QUrl u(filename);
  QNetworkRequest request(u);
  webCtrl.get(request);
}
void MainWindow::downloaded(QNetworkReply* ptrReply)
{
  ptrReply->deleteLater(); // mark for deletion after signal completes
  input = QString(ptrReply->readAll());
  if (!input.isEmpty())
  {
    ui->runButton->setEnabled(true);
    ui->statusLabel->setText("URL Loaded");
  }
  else
  {
    ui->runButton->setEnabled(false);
    ui->statusLabel->setText("---");
    QMessageBox::critical(this, "Error", "Could not open URL");
  }
}

/////////////////////////////////////////////////////////
//// FILE STUFF

// read a file from either browse or drag-n-drop
void MainWindow::readFile()
{
  QFile file(filename);

  if (filename == "")
    return;

  input = "";
  ui->runButton->setEnabled(false);
  if (!file.open(QIODevice::ReadOnly))
  {
    ui->runButton->setEnabled(false);
    ui->statusLabel->setText("---");
    QMessageBox::critical(this, "Error", "Could not open file");
    return;
  }
  input = QString(file.readAll());
  file.close();
  ui->runButton->setEnabled(true);
  ui->statusLabel->setText("File Loaded");

  // Chop off the path
  int chop = filename.lastIndexOf(QRegExp("[\\/]"));
  if (chop>0)
    filename = filename.right(filename.length()-chop-1);
}

void MainWindow::dropEvent(QDropEvent* event)
{
  filename = event->mimeData()->text().right(event->mimeData()->text().length()-8);
  //  QString filename = event->mimeData()->urls().at(0).toString();  filename = filename.right(filename.size()-8);  // chop off leading "file:///" part
  readFile();
}


void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
  QString tmpFilename;

  if (!ui->browseButton->isEnabled())
    return;  // don't accept file loads while we're running
  if (!event->mimeData()->hasText())  //   if (!event->mimeData()->hasUrls())
    return;
  // filename = event->mimeData()->urls().at(0).toString();
  tmpFilename = event->mimeData()->text();           // "file:///C:/Qt/example.txt"
  tmpFilename = tmpFilename.right(tmpFilename.length()-8); //   "C:/Qt/example.txt
  if (!QFile(tmpFilename).exists())
    return; // bad filename
  if (tmpFilename.right(3) != "txt")
    return; // only text files
  //if (QFile(filename).size() > MAX_FILESIZE)
  //  return; // file too large
  event->acceptProposedAction();
}
