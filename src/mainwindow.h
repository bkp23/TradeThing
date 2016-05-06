#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager> // Network stuff is needed
#include <QNetworkRequest>       // for pulling want from
#include <QNetworkReply>         // online.
#include "exec.h"

namespace Ui { class MainWindow; }
class Exec;

class MainWindow : public QMainWindow
{
  Q_OBJECT

  public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void runComplete(QString results);
    // UI Functions
    void displayTxt(QString str);
    void displayStats(QString status, QString users, QString real, QString viable, QString trades, QString iter);
    void setBarFormat(QString fmt, int max);
    void setBarVal(int val);

  private slots:
    void browseFile();
    void pullUrl();
    void downloaded(QNetworkReply* pReply);
    void runButtonPressed();
    void stopButtonPressed();
    void saveButtonPressed();
    void about();

  private:
    void readFile();
    void dropEvent(QDropEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);

    Ui::MainWindow *ui;
    Exec *ptrExec;
    QString input;
    QString filename;
    bool keepRunning, paused;

    // Used for downloading wants
    QNetworkAccessManager webCtrl;
};

#endif // MAINWINDOW_H
