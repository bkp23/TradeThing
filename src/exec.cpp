#include "exec.h"
#include <QTime>  // for seeding RNG
#include <QTimer> // for periodically updating threads

extern QHash<QString, OptionType> gOptions;

#define PROGRESS_PER_ITER 256  // the resultion of an individual thread for updating progress bar

Exec::Exec(MainWindow *parent)
{
  ptrParent       = parent;
  ptrBestCycles   = NULL;
  ptrBestGraph    = NULL;
  bestMetric      = 0;
  iterations      = 0;
  completions     = 0;
  bankedTime      = 0;
  parsedData.numItems      = 0;
  parsedData.numDummyItems = 0;
  parsedData.maxNameWidth  = 0;
}

Exec::~Exec()
{
  // Clean up
  if (ptrBestCycles)
  {
    while (!ptrBestCycles->isEmpty())
      delete ptrBestCycles->takeFirst();
    delete ptrBestCycles;
  }
  if (ptrBestGraph != NULL)
      delete ptrBestGraph;
  while (!graphList.isEmpty())
    delete graphList.takeFirst();
}


// Executes the complete sequence.
void Exec::go(QString &input, QString inputSrc, bool *ptrRunning, bool *ptrPaused)
{
  Q_ASSERT(iterations==0);

  OUTPURPLE(PROG_NAME + " " + PROG_VERSION);
  output += "Input from: ";
  OUTBLUE(inputSrc); // (1.4)
  updateStats("Parsing data");

  // Read in the want options, usernames, and want lists
  parseInput(ptrParent, input, parsedData);

  // Display custom options, if they exist
  bool customOptions = false;
  QHash<QString,OptionType>::iterator i;
  for (i = gOptions.begin(); i != gOptions.end(); ++i)
  {
    OptionType opt = i.value();
    if (opt.changed)
    {
      if (!customOptions) // this is the first
      {
        output += "Options:";
        customOptions = true;
      }
      output += " "+opt.altName.toUpper();
      if (opt.hasVal && i.key() != "metric")
        output += "="+QString::number(opt.value);
    }
  }
  if (customOptions)
    OUT();
  OUT();

  // (1.4) TODO: display MD5 checksum

  if (gOptions["iterations"].value > 1  &&  !gOptions["randSeed"].changed) // (1.4)
  {
    gOptions["randSeed"].value = QTime::currentTime().msec();
    OUTRED("No explicit SEED; using " + QString::number(gOptions["randSeed"].value));
  }

  if (gOptions["metric"].value != CHAIN_SIZES_SOS &&  // (1.4)
      gOptions["priorityScheme"].value != NO_PRIORITIES)
    OUTRED("Warning: using priorities with the non-default metric is normally worthless");

  // Create the graph by parsing the want lists and other input
  graph.ptrKeepRunning = ptrRunning;
  graph.ptrPaused = ptrPaused;
  buildGraph(ptrParent, parsedData, graph);
  jrand.setSeed(gOptions["randSeed"].value);

  // Display more info if requested by options
  if (gOptions["showMissing"].enabled && !parsedData.officialNames.isEmpty())
  {
    for (int idx=0; idx<parsedData.usedNames.size(); idx++)
      parsedData.officialNames.removeOne(parsedData.usedNames.at(idx));
    parsedData.officialNames.sort();
    for (int idx=0; idx<parsedData.officialNames.size(); idx++)
      OUTRED("**** Missing want list for official name " + parsedData.officialNames.at(idx));
    OUT();
  }
  if (gOptions["showErrors"].enabled && !parsedData.errors.isEmpty())
  {
    parsedData.errors.sort();
    OUT("ERRORS:");
    for (int idx=0; idx<parsedData.errors.size(); idx++)
      OUTRED(parsedData.errors.at(idx));
    OUT();
  }
  updateStats("Running");

  // Input parsing is completed. Before we start processing, we'll take a
  // timestamp so we can later report the total processing time
  elapsedTime.start();

  // Remove unusable entries and edges from the graph
  graph.removeImpossibleEdges();
  updateStats("Running", false);

  // Pre-processing complete. Kick off search function.
  ptrParent->setBarFormat("Finding matches", PROGRESS_PER_ITER*gOptions["iterations"].value );
  QTimer::singleShot(0, this, SLOT(running()) );
}


void Exec::running()
{
  static int numWaitingThreads = 2; // keep this many threads queued up
  Graph *ptrNewGraph;
  int progress = 0, threadsJustFinished = 0;

  // Check for completions
  for (int idx=0; idx<runningGraphs.size(); idx++)
  {
    if (!runningGraphs.at(idx).isRunning()) // it finished running
    {
      threadsJustFinished++;
      if (*graph.ptrKeepRunning)
        completions++;
      updateStats("Running", false);
      if (ptrBestGraph == NULL) // this is the first result!
      {
        ptrBestGraph = graphList.at(idx);
        ptrBestCycles = runningGraphs.at(idx).result();
        bestMetric = calculateMetric(ptrBestCycles, (METRIC_TYPE)gOptions["metric"].value);
        OUTGREEN(metricString((METRIC_TYPE)gOptions["metric"].value));
        updateStats("Running");
      }
      else
      {
        // Determine if we have a better solution this time
        CyclesType *ptrCycles = runningGraphs.at(idx).result();
        int newMetric = calculateMetric(ptrCycles, (METRIC_TYPE)gOptions["metric"].value);

        // Check if we have a new best (or a tie that should have occurred earlier)
        if (newMetric < bestMetric ||
            (newMetric == bestMetric  &&  graphList.at(idx)->numCopies < ptrBestGraph->numCopies) )
        {
          // Destroy old results
          deleteCycles(ptrBestCycles);
          delete ptrBestGraph;
          // Save off info on the new find
          bestMetric = newMetric;
          ptrBestCycles = ptrCycles;
          ptrBestGraph = graphList.at(idx);
          OUTGREEN(metricString((METRIC_TYPE)gOptions["metric"].value));
          updateStats("Running");
        }
        else
        {
          deleteCycles(ptrCycles);
          delete graphList[idx];
          if (gOptions["debug"].enabled)
          {
            OUT("# " + metricString((METRIC_TYPE)gOptions["metric"].value));
            updateStats("Running");
          }
        } // end if (better result)
      } // end if (ptrBestGraph)

      // Remove completed items from lists
      runningGraphs.removeAt(idx);
      graphList.removeAt(idx);
    } // end if (completed)
  } // end for(runningGraphs)

  // If iterations are finishing faster than we're spawning new threads,
  // then spawn threads faster.
  if (threadsJustFinished >= numWaitingThreads)
    numWaitingThreads = threadsJustFinished + 2;

  // Start new threads if
  //  * we haven't been canceled AND
  //  * there are available CPU cores AND
  //  * we haven't spawned enough to hit our iteration count
  while (*graph.ptrKeepRunning  &&
         runningGraphs.size() < QThread::idealThreadCount() + numWaitingThreads &&
         iterations < gOptions["iterations"].value)
  {
    ptrNewGraph = new Graph();
    // Don't shuffle the graph before the first iteration (because TradeMaximizer didn't)
    if (iterations++ > 0)
      graph.shuffle(jrand);
    // Copy the previous graph structure to the new one.
    graph.copy(ptrNewGraph);
    runningGraphs.append( QtConcurrent::run(ptrNewGraph, &Graph::findCycles) );
    graphList.append(ptrNewGraph);
  }

  // Update status bar
  if (*graph.ptrKeepRunning)
  {
    progress = PROGRESS_PER_ITER*completions;
    for (int idx=0; idx<graphList.size(); idx++)
      progress += graphList.at(idx)->progress;
    ptrParent->setBarVal(progress);
  }

  // Pause the timer if we're paused
  if (*graph.ptrPaused)
  {
    updateStats("Paused", false);
    if (elapsedTime.isValid())
    {
      bankedTime += elapsedTime.elapsed();
      elapsedTime.invalidate();
    }
  }
  else if (!*graph.ptrPaused && !elapsedTime.isValid()) // if we just resumed
  {
    elapsedTime.start();  // start the timer back up
    updateStats("Running", false);
  }

  if (runningGraphs.isEmpty())
    allDone();
  else
    QTimer::singleShot(500, this, SLOT(running()) ); // perform a couple times per second
}


void Exec::allDone()
{
  OUT();

  if (*graph.ptrKeepRunning)
  {
    quint64 totalTime = elapsedTime.elapsed() + bankedTime;
    output += displayMatches(ptrBestCycles, parsedData, ptrBestGraph);

    if (gOptions["showElapsedTime"].enabled)
    {
      output += "Elapsed time = ";
      OUTBLUE(QString::number(totalTime) + "ms ("+timeToStr(totalTime)+")");
    }
    updateStats("Completed");
    ptrParent->setBarFormat("Completed", PROGRESS_PER_ITER*gOptions["iterations"].value );
  }
  else
  {
    OUTRED("OPERATION CANCELED.");
    updateStats("Canceled");
  }

  ptrParent->runComplete(output);
}


void Exec::deleteCycles(CyclesType *ptrCycles)
{
  if (ptrCycles != NULL)
  {
    while (!ptrCycles->isEmpty())
      delete ptrCycles->takeFirst();
    delete ptrCycles;
  }
}


QString Exec::displayMatches(CyclesType *ptrCycles,
                             ParseDataType parsed,
                             Graph *ptrGraph)
{
  int numTrades = 0;
  int numGroups = ptrCycles->size();
  int totalCost = 0;
  int sumOfSquares = 0;
  bool sort = gOptions["sortByItem"].enabled;
  int width = parsed.maxNameWidth;
  QList<int> groupSizes;
  QStringList summary, loops;
  QString output, str;

  for(int idx=0; idx<ptrCycles->size(); idx++)
  {
    QList<Node> *ptrCycle = ptrCycles->at(idx);
    int size = ptrCycle->size();
    numTrades += size;
    sumOfSquares += size*size;
    groupSizes.append(size);
    for(int i=0; i<ptrCycle->size(); i++)
    {
      Node n = ptrCycle->at(i);
      Q_ASSERT(n.ptrMatch != n.ptrTwin);
      loops.append(pad(n.show(sort),width) + " receives " + n.ptrMatch->ptrTwin->show(sort));
      summary.append(pad(n.show(sort),width) + " receives " + pad(n.ptrMatch->ptrTwin->show(sort),width) + " and sends to " + n.ptrTwin->ptrMatch->show(sort));
      totalCost += n.matchCost;
    }
    loops.append("");
  }
  if (gOptions["showNonTrades"].enabled)
  {
    for(int i=0; i<ptrGraph->wanters.size(); i++)
    {
      Node *n = ptrGraph->wanters.at(i);
      if (n->ptrMatch == n->ptrTwin && !n->isDummy)
        summary.append(pad(n->show(sort),width) + "             does not trade");
    }
    for(int i=0; i<ptrGraph->orphans.size(); i++)
    {
      Node *n = ptrGraph->orphans.at(i);
      if (!n->isDummy)
        summary.append(pad(n->show(sort),width) + "             does not trade");
    }
  }

  if (gOptions["showLoops"].enabled)
  {
    output += "<pre>";
    output += "TRADE LOOPS (" + QString::number(numTrades) + " total trades):\n";
    output += "\n";
    for (int idx=0; idx<loops.size(); idx++)
      output += loops.at(idx) + "\n";
    output += "</pre>\n";
  }

  if (gOptions["showSummary"].enabled)
  {
    output += "<pre>";
    summary.sort();
    output += "ITEM SUMMARY (" + QString::number(numTrades) + " total trades):\n";
    output += "\n";
    for (int idx=0; idx<summary.size(); idx++)
      output += summary.at(idx) + "\n";
    output += "\n";
    output += "</pre>\n";
  }

  // Display number of trades (and percentage)
  int realItems = parsed.numItems - parsed.numDummyItems;
  output += "Num trades  = ";
  str = QString::number(numTrades) + " of " + QString::number(realItems) + " items";
  if (realItems > 0)
    str += " (" + QString::number(100.0*(double)numTrades/(double)realItems,'f',1) + "%)";
  OUTGREEN(str);

  if (gOptions["showStats"].enabled)
  {
    // Display total cost
    output += "Total cost  = ";
    str = QString::number(totalCost);
    if (numTrades > 0)
      str += " (avg "+ QString::number((double)totalCost/(double)numTrades,'f',2)+")";
    OUTBLUE(str);
    // Display number of groups
    output += "Num groups  = ";
    OUTBLUE(QString::number(numGroups));
    // Display group sizes
    output += "Group sizes =";
    str = "";
    std::sort(groupSizes.begin(), groupSizes.end());
    for(int idx=groupSizes.size()-1; idx>=0; idx--)
      str += " " + QString::number(groupSizes.at(idx));
    OUTBLUE(str);
    // Display sum of squares, only because TradeMaximizer did it
    output += "Sum squares = ";
    OUTBLUE(QString::number(sumOfSquares));
  }
  return output;
}

void Exec::updateStats(QString status, bool newOutput)
{
  QString users = statNum(parsedData.usernames.size());
  QString trades = QString("---");
  if (ptrBestCycles != NULL)
  {
    int numTrades = 0;
    for(int idx=0; idx<ptrBestCycles->size(); idx++)
    {
      QList<Node> *ptrCycle = ptrBestCycles->at(idx);
      numTrades += ptrCycle->size();
    }
    trades = QString::number(numTrades);
  }
  QString real = statNum(parsedData.numItems - parsedData.numDummyItems);
  QString viable = statNum(graph.viableRealItems);
  QString iter = QString::number(completions) + " of " +
                 QString::number(gOptions["iterations"].value);
  ptrParent->displayStats(status,users,real,viable,trades,iter);
  if (newOutput)
    ptrParent->displayTxt(output);
}
QString Exec::statNum(int x)
{
  if (x<=0)
    return QString("---");
  else
    return QString::number(x);
}

QString Exec::pad(QString name, int width)
{
  while (name.length() < width)
    name += " ";
  return name;
}

QString Exec::timeToStr(quint64 t)
{
  QString str;

  str = QString::number(t%1000)+"ms";
  t /= 1000;
  if (t == 0)
    return str;

  str = QString::number(t%60)+"sec " + str;
  t /= 60;
  if (t == 0)
    return str;

  str = QString::number(t%60)+"min " + str;
  t /= 60;
  if (t == 0)
    return str;

  str = QString::number(t%24)+"hr " + str;
  t /= 24;
  if (t == 0)
    return str;

  str = QString::number(t)+"days " + str; // hopefully this never happens
  return str;
}

