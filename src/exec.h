#ifndef EXEC_H
#define EXEC_H

#include <QString>       // for all the string stuff
#include <QElapsedTimer> // for displaying execution time
#include <QtConcurrent>  // for multiple thread execution
#include "mainwindow.h"
#include "parser.h"
#include "graph.h"
#include "javarand.h"
#include "metric.h"

#define PROG_NAME     QString("TradeThing")
#define PROG_VERSION  QString("v1.4")
#define NEWLINE       QString("<br>\n")
#define OUT(str)        output += QString(str) + NEWLINE
#define OUTRED(str)     output += QString("<font color=\"#880000\">")+ QString(str) + "</font>" + NEWLINE
#define OUTBLUE(str)    output += QString("<font color=\"#0000B8\">")+ QString(str) + "</font>" + NEWLINE
#define OUTGREEN(str)   output += QString("<font color=\"#007000\">")+ QString(str) + "</font>" + NEWLINE
#define OUTPURPLE(str)  output += QString("<font color=\"#900090\">")+ QString(str) + "</font>" + NEWLINE

class Exec: public QObject
{
  Q_OBJECT  // allow slots

  public:
    Exec(MainWindow *parent);
    ~Exec();
    void go(QString &input, QString inputSrc, bool *ptrRunning, bool *ptrPaused);

  private slots:
    void running();


  private:
    void allDone();
    void deleteCycles(CyclesType *ptrCycles);
    QString displayMatches(CyclesType *ptrCycles, ParseDataType parsed, Graph *ptrGraph);
    void updateStats(QString status, bool newOutput=true);
    QString statNum(int x);
    QString pad(QString name, int width);
    QString timeToStr(quint64 t);

    MainWindow *ptrParent;
    ParseDataType parsedData;
    QString output;
    Graph graph;
    JavaRand jrand;
    QElapsedTimer elapsedTime;
    quint64 bankedTime; // accounts for pause-unpause cycles
    CyclesType *ptrBestCycles;
    Graph *ptrBestGraph;
    int bestMetric;
    unsigned int iterations; // how many iterations have been started
    unsigned int completions; // how many iterations have been completed

    QList< QFuture<CyclesType*> > runningGraphs;
    QList< Graph* > graphList;
};



#endif // EXEC_H
