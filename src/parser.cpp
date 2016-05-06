#include "parser.h"
#include <QMessageBox>  // for displaying critical errors
#include <QApplication> // for updating the display

static void setDefaultOptions( QHash<QString, OptionType> &options );
static void setOption(QString optName, bool enabled, QString name, int val=0);
static bool fatalError(QWidget *parent, QString str, int line);

QHash<QString, OptionType> gOptions;

bool parseInput(MainWindow *parent, QString input, ParseDataType &parsed)
{
  QString line;
  int lineNumber=0;
  bool readingOfficialNames = false;
  QStringList inputLines = input.split("\n");

  parent->setBarFormat("Parsing input %v / %m", inputLines.size());
  setDefaultOptions( gOptions ); // set options to their default values

  while (lineNumber < inputLines.size())
  {
    line = inputLines[lineNumber++].trimmed();
    if ((lineNumber & 0x3FF) == 0) // update progress bar occasionally
    {
      parent->setBarVal(lineNumber);
      QApplication::processEvents(); // update display every so often
    }
    if (line == "")
    { } // do nothing
    // Check for options ------------------------------------------------------------
    else if (line.startsWith("#!"))
    {
      if (!parsed.wantLists.isEmpty())
        return fatalError(parent, "Options (#!...) cannot be declared after first real want list", lineNumber);
      if (!parsed.officialNames.isEmpty())
        return fatalError(parent, "Options (#!...) cannot be declared after official names", lineNumber);

      // Handle options
      QStringList options = line.toUpper().right(line.length()-2).trimmed().split(QRegExp("\\s+"));
      for (int a=0; a<options.size(); a++)
      {
        QString opt = options.at(a);

        if (opt == "CASE-SENSITIVE")
          setOption("caseSensitive", true, "CASE-SENSITIVE");
        else if (opt == "REQUIRE-COLONS")
          setOption("requireColons", true, "REQUIRE-COLONS");
        else if (opt == "REQUIRE-USERNAMES")
          setOption("requireUsernames", true, "REQUIRE-USERNAMES");
        else if (opt == "HIDE-ERRORS")
          setOption("showErrors", false, "HIDE-ERRORS");
        else if (opt == "HIDE-REPEATS")
          setOption("showRepeats", false, "HIDE-REPEATS");
        else if (opt == "HIDE-LOOPS")
          setOption("showLoops", false, "HIDE-LOOPS");
        else if (opt == "HIDE-SUMMARY")
          setOption("showSummary", false, "HIDE-SUMMARY");
        else if (opt == "HIDE-NONTRADES")
          setOption("showNonTrades", false, "HIDE-NONTRADES");
        else if (opt == "HIDE-STATS")
          setOption("showStats", false, "HIDE-STATS");
        else if (opt == "SHOW-MISSING")
          setOption("showMissing", true, "SHOW-MISSING");
        else if (opt == "SORT-BY-ITEM")
          setOption("sortByItem", true, "SORT-BY-ITEM");
        else if (opt == "ALLOW-DUMMIES")
          setOption("allowDummies", true, "ALLOW-DUMMIES");
        else if (opt == "SHOW-ELAPSED-TIME")
          setOption("showElapsedTime", true, "SHOW-ELAPSED-TIME");
        else if (opt == "LINEAR-PRIORITIES")
          setOption("priorityScheme", true, "LINEAR-PRIORITIES", LINEAR_PRIORITIES);
        else if (opt == "TRIANGLE-PRIORITIES")
          setOption("priorityScheme", true, "TRIANGLE-PRIORITIES", TRIANGLE_PRIORITIES);
        else if (opt == "SQUARE-PRIORITIES")
          setOption("priorityScheme", true, "SQUARE-PRIORITIES", SQUARE_PRIORITIES);
        else if (opt == "SCALED-PRIORITIES")
          setOption("priorityScheme", true, "SCALED-PRIORITIES", SCALED_PRIORITIES);
        else if (opt == "EXPLICIT-PRIORITIES")
          setOption("priorityScheme", true, "EXPLICIT-PRIORITIES", EXPLICIT_PRIORITIES);
        else if (opt.startsWith("SMALL-STEP="))
        {
          bool ok;
          int val = opt.right(opt.length()-11).toInt(&ok);
          if (!ok || val<0)
            return fatalError(parent, "SMALL-STEP argument must be a non-negative integer",lineNumber);
          setOption("smallStep", true, "SMALL-STEP", val);
        }
        else if (opt.startsWith("BIG-STEP="))
        {
          bool ok;
          int val = opt.right(opt.length()-9).toInt(&ok);
          if (!ok || val<0)
            return fatalError(parent, "BIG-STEP argument must be a non-negative integer",lineNumber);
          setOption("bigStep", true, "BIG-STEP", val);
        }
        else if (opt.startsWith("NONTRADE-COST="))
        {
          bool ok;
          int val = opt.right(opt.length()-9).toInt(&ok);
          if (!ok || val<=0)
            return fatalError(parent, "NONTRADE-COST argument must be a positive integer",lineNumber);
          setOption("nonTradeCost", true, "NONTRADE-COST", val);
        }
        else if (opt.startsWith("ITERATIONS="))
        {
          bool ok;
          int val = opt.right(opt.length()-11).toInt(&ok);
          if (!ok || val<=0)
            return fatalError(parent, "ITERATIONS argument must be a positive integer",lineNumber);
          setOption("iterations", true, "ITERATIONS", val);
        }
        else if (opt.startsWith("SEED="))
        {
          bool ok;
          int val = opt.right(opt.length()-5).toInt(&ok);
          if (!ok || val<=0)
            return fatalError(parent, "SEED argument must be a positive integer",lineNumber);
          setOption("randSeed", true, "SEED", val);
        }
        else if (opt == "VERBOSE") // (1.4)
          setOption("verbose", true, "VERBOSE");
        else if (opt.startsWith("METRIC=")) // (1.4)
        {
          QString met = opt.right(opt.length()-7);
          if (met == "USERS-TRADING")
            setOption("metric", true, "METRIC=USERS-TRADING", USERS_TRADING);
          else if (met == "USERS-SOS")
            setOption("metric", true, "METRIC=USERS-SOS", USERS_SOS);
          else if (met == "COMBINE-SHIPPING")
            setOption("metric", true, "METRIC=COMBINE-SHIPPING", COMBINE_SHIPPING);
          else if (met == "CHAIN-SIZES-SOS") // default
            setOption("metric", true, "METRIC=CHAIN-SIZES-SOS", CHAIN_SIZES_SOS);
          else
            return fatalError(parent, "Unknown metric option \""+met+"\"",lineNumber);
        }
        else
          return fatalError(parent, "Unknown option \""+opt+"\"",lineNumber);
      } // end for(options)
    } // end if(option)
    else if (line.startsWith("#"))
    {
      // skip commented lines
    }

    // Handle official names --------------------------------------------------------
    else if (line.toUpper() == "!BEGIN-OFFICIAL-NAMES")
    {
      if (!parsed.officialNames.isEmpty())
        return fatalError(parent, "Cannot begin official names more than once", lineNumber);
      if (!parsed.wantLists.isEmpty())
        return fatalError(parent, "Official names cannot be declared after first real want list", lineNumber);

      readingOfficialNames = true;
    }
    else if (line.toUpper() == "!END-OFFICIAL-NAMES")
    {
      if (!readingOfficialNames)
        return fatalError(parent, "!END-OFFICIAL-NAMES without matching !BEGIN-OFFICIAL-NAMES", lineNumber);
      readingOfficialNames = false;
    }
    else if (readingOfficialNames)
    {
      if (line.left(1) == ":")
        return fatalError(parent, "Line cannot begin with colon",lineNumber);
      if (line.left(1) == "%")
        return fatalError(parent, "Cannot give official names for dummy items",lineNumber);

      QString name = line.split(QRegExp("[:\\s]"))[0];
      if (!gOptions["caseSensitive"].enabled)
        name = name.toUpper();
      if (parsed.officialNames.contains(name))
        return fatalError(parent, "Official name "+name+"+ already defined",lineNumber);
      parsed.officialNames.append(name);
    }

    // Handle wants -----------------------------------------------------------------
    else
    {
      // Check parens for username preceding wants
      if (line.indexOf("(") == -1 && gOptions["requireUsernames"].enabled)
        return fatalError(parent, "Missing username with REQUIRE-USERNAMES selected",lineNumber);
      if (line.left(1) == "(")
      {
        if (line.lastIndexOf("(") > 0)
          return fatalError(parent, "Cannot have more than one '(' per line",lineNumber);
        int close = line.indexOf(")");
        if (close == -1)
          return fatalError(parent, "Missing ')' in username",lineNumber);
        if (close == line.length()-1)
          return fatalError(parent, "Username cannot appear on a line by itself",lineNumber);
        if (line.lastIndexOf(")") > close)
          return fatalError(parent, "Cannot have more than one ')' per line",lineNumber);
        if (close == 1)
          return fatalError(parent, "Cannot have empty parentheses",lineNumber);

        // Temporarily replace spaces in username with #'s
        if (line.indexOf(" ") < close)
          line = line.left(close+1).replace(" ","#")+" " + line.mid(close+1);
      }
      else if (line.contains("("))
        return fatalError(parent, "Username (parentheses) can only be used at the front of a want list",lineNumber);
      else if (line.contains(")"))
        return fatalError(parent, "Bad ')' on a line that does not have a '('",lineNumber);

      // Check wants for semicolons, which indicate a large step in rank
      // value between two wants
      line = line.replace(";"," ; "); // place extra space around semicolons
      if (line.contains(";"))
      {
        int semiPos = line.indexOf(";");
        if (semiPos < line.indexOf(":")) // Semicolons can only appear between wants
          return fatalError(parent, "Semicolon cannot appear before colon",lineNumber);
        QString before = line.left(semiPos).trimmed();
        if (before.length() == 0 || before.right(1)==")")
          return fatalError(parent, "Semicolon cannot appear before first item on line", lineNumber);
      }

      // Check and remove colon, which should occur just after the item whose
      // wants are being specified
      if (line.contains(":"))
      {
        int colonPos = line.indexOf(":");
        if (line.lastIndexOf(":") != colonPos)
          return fatalError(parent, "Cannot have more that one colon on a line",lineNumber);
        QString header = line.left(colonPos).trimmed();
        if (!header.contains(QRegExp("(.*\\)\\s+)?[^(\\s)]\\S*")))
          return fatalError(parent, "Must have exactly one item before a colon (:)",lineNumber);
        line = line.replace(colonPos, 1, " "); // remove colon
      }
      else if (gOptions["requireColons"].enabled)
        return fatalError(parent, "Missing colon with REQUIRE-COLONS selected",lineNumber);

      if (!gOptions["caseSensitive"].enabled)
        line = line.toUpper();
      // Add an array of each item on the list to wantLists. The first item is
      // the username, if present. The next item (or the first, if no username)
      // is the item whose wants are specified in the remaining items.
      parsed.wantLists.append(line.trimmed().split(QRegExp("\\s+")));

    } // end if (what to handle)
  } // end while(lines)
  parent->setBarVal(inputLines.size());

  if (parsed.wantLists.isEmpty())
  {
    QMessageBox::critical(parent, "Input Error", "No want lists found in input; nothing to process");
    return false;
  }

  return true;
}


void buildGraph(MainWindow *parent, ParseDataType &parsed, Graph &graph)
{
  QHash<QString,int> unknownNames;
  parsed.numItems = 0;
  parsed.numDummyItems = 0;
  parsed.maxNameWidth = 0;

  // Create the nodes -------------------------------------------------------------
  parent->setBarFormat("Constructing graph", parsed.wantLists.size());
  for (int i = 0; i < parsed.wantLists.size()  &&  *graph.ptrKeepRunning; i++)
  {
    if ((i & 0x1FF) == 0) // check once in a while
    {
      parent->setBarVal(i+1);
      QApplication::processEvents(); // update display every so often
    }

    QStringList list = parsed.wantLists.at(i);
    Q_ASSERT(!list.isEmpty()); // Every array of strings should be a formatted want list
    QString name = list[0]; // The item to be traded (whose wants are specified here)
    QString owner = "";    // The user owning the item

    // Check whether a name is present as the first string
    if (name.left(1) == "(")
    {
      owner = name.replace("#"," "); // restore spaces in username
      parsed.wantLists[i].removeFirst(); // remove the username from the list
      list = parsed.wantLists.at(i);
      name = list[0]; // set the current item to be traded to the next string
      if (!parsed.usernames.contains(owner)) // track unique users
        parsed.usernames.append(owner);
    }

    // Check whether this item is a dummy item
    bool isDummy = (name.left(1) == "%");
    if (isDummy)
    {
      if (owner.isEmpty())
        parsed.errors.append("**** Dummy item " + name + " declared without a username.");
      else if (!gOptions["allowDummies"].enabled)
        parsed.errors.append("**** Dummy items not allowed. ("+name+")");
      else
      {
        name += " for user " + owner;
        list[0] = name;
        parsed.wantLists[i] = list; // update wantlists
      }
    }

    // Check whether the item exists in the list of official item names, if present
    if (!parsed.officialNames.isEmpty() && !parsed.officialNames.contains(name) && !isDummy)
    {
      parsed.errors.append("**** Cannot define want list for "+name+" because it is not an official name.  (Usually indicates a typo by the item owner.)");
      parsed.wantLists[i] = QStringList();
    }
    else if (graph.getNode(name) != NULL)
    {
      parsed.errors.append("**** Item " + name + " has multiple want lists--ignoring all but first.  (Sometimes the result of an accidental line break in the middle of a want list.)");
      parsed.wantLists[i] = QStringList();
    }
    else
    {
      parsed.numItems++; // increment the number of items being offered
      if (isDummy)
        parsed.numDummyItems++; // keep track of how many offered items are dummy items

      // Add sender and wanter vertices (nodes) to the graph
      Node *ptrNode = graph.addNode(name, owner, isDummy);

      // Mark this item's name as added to the graph
      if (parsed.officialNames.contains(name))
        parsed.usedNames.append(name);

      // Keep track of the longest name+entry length for output formatting purposes
      if (!isDummy && parsed.maxNameWidth < ptrNode->show(gOptions["sortByItem"].enabled).length())
        parsed.maxNameWidth = ptrNode->show(gOptions["sortByItem"].enabled).length();
    }
  }


  // Create the edges -------------------------------------------------------------
  parent->setBarFormat("Adding connections", parsed.wantLists.size());
  for (int i = 0; i < parsed.wantLists.size()  &&  *graph.ptrKeepRunning; i++)
  {
    if ((i & 0x1FF) == 0) // check once in a while
    {
      parent->setBarVal(i+1);
      QApplication::processEvents(); // update display every so often
    }

    QStringList list = parsed.wantLists.at(i);
    if (list.isEmpty())
      continue; // skip the duplicate lists

    QString fromName = list[0];
    Node *ptrFromNode = graph.getNode(fromName);

    // Add the "no-trade" edge to itself (from wanter to sender node)
    graph.addEdge(ptrFromNode, ptrFromNode->ptrTwin, gOptions["nonTradeCost"].value);

    // Evaluate each want for this item
    quint64 rank = 1;
    for (int i = 1; i < list.size(); i++)
    {
      QString toName = list[i]; // focus on this want

      // A single semicolon represents a large step in rank value between
      // two items
      if (toName == ";")
      {
        rank += gOptions["bigStep"].value;
        continue;
      }

      // Perform entry error checking on the current want
      if (toName.indexOf('=') >= 0)
      {
        // Handle explicit priorities (e.g., ThisWant=100)
        if (gOptions["priorityScheme"].value != EXPLICIT_PRIORITIES)
        {
          parsed.errors.append("**** Cannot use '=' annotation in item "+toName+" in want list for item "+fromName+" unless using EXPLICIT_PRIORITIES.");
          continue;
        }
        if (!toName.contains(QRegExp("[^=]+=[0-9]+")))
        {
          parsed.errors.append("**** Item "+toName+" in want list for item "+fromName+" must have the format 'name=number'.");
          continue;
        }
        QStringList parts = toName.split("=");
        Q_ASSERT(parts.size() == 2);
        quint64 explicitCost = parts[1].toLong();
        if (explicitCost < 1)
        {
          parsed.errors.append("**** Explicit priority must be positive in item "+toName+" in want list for item "+fromName+".");
          continue;
        }
        rank = explicitCost;
        toName = parts[0];
      }

      // Handle dummy items
      if (toName.left(1) == "%")
      {
        // Make sure this item has an associated username if it wants a dummy item
        if (ptrFromNode->owner.isEmpty())
        {
          parsed.errors.append("**** Dummy item " + toName + " used in want list for item " + fromName + ", which does not have a username.");
          continue;
        }

        // Append the username to the dummy item to prevent confusion in cases
        // where multiple users have dummy items with the same name.
        toName += " for user " + ptrFromNode->owner;
      }

      Node *ptrToNode = graph.getNode(toName); // grab the node for this want
      if (ptrToNode == NULL)
      {
        if (parsed.officialNames.contains(toName))
        {
          // this is an official item whose owner did not submit a want list
          rank += gOptions["smallStep"].value;
        }
        else
        {
          // there is no offical item list; track number of uknown items
          int occurrences = unknownNames.contains(toName) ? unknownNames.value(toName) : 0;
          unknownNames.insert(toName, occurrences + 1);
        }
        continue;
      }

      ptrToNode = ptrToNode->ptrTwin; // adjust to the sending node
      if (ptrToNode == ptrFromNode->ptrTwin)
      {
        parsed.errors.append("**** Item " + toName + " appears in its own want list.");
      }
      else if (ptrFromNode->containsEdge(ptrToNode))
      {
        if (gOptions["showRepeats"].enabled)
          parsed.errors.append("**** Item " + toName + " is repeated in want list for " + fromName + ".");
      }
      else if (!ptrToNode->isDummy &&
               ptrFromNode->owner == ptrToNode->owner)
      {
        parsed.errors.append("**** Item "+ptrFromNode->name +" contains item "+ptrToNode->name+" from the same user ("+ptrFromNode->owner+")");
      }
      else
      {
        quint64 cost = 1;
        switch (gOptions["priorityScheme"].value)
        {
          case NO_PRIORITIES:       cost = 1;               break;
          case LINEAR_PRIORITIES:   cost = rank;            break;
          case TRIANGLE_PRIORITIES: cost = rank*(rank+1)/2; break;
          case SQUARE_PRIORITIES:   cost = rank*rank;       break;
          case SCALED_PRIORITIES:   cost = rank;            break; // assign later
          case EXPLICIT_PRIORITIES: cost = rank;            break;
          default: Q_ASSERT(false); // invalid value
        }

        // All edges out of a dummy node have the same cost because
        // they are usually considered to be the same item for
        // duplicate protection purposes.
        if (ptrFromNode->isDummy)
          cost = gOptions["nonTradeCost"].value;

        // Add a connection from the listed item's receiver node to
        // the wanted item's sender node.
        graph.addEdge(ptrFromNode, ptrToNode, cost);

        // Increase the rank for calculating the priority of the next want
        rank += gOptions["smallStep"].value;
      }
    }

    // Update costs for those priority schemes that need information such as
    // number of wants
    if (!ptrFromNode->isDummy)
    {
      switch (gOptions["priorityScheme"].value)
      {
        case SCALED_PRIORITIES:
          int n = ptrFromNode->edges.size()-1;
          for (int idx=0; idx<ptrFromNode->edges.size(); idx++)
          {
            Edge *e = ptrFromNode->edges.at(idx);
            if (e->ptrSender != ptrFromNode->ptrTwin)
              e->cost = 1 + (e->cost-1)*2520/n;
          }
          break;
      } // end switch
    } // end if(isDummy)
  } // end for(create edges)
  parent->setBarVal(parsed.wantLists.size());

  // "Freeze" the graph, declaring that we have finished adding things to it
  // and readying it for cleanup and analysis
  graph.freeze();

  // If any unknown items were added as wants, display those to the user now
  QHash<QString,int>::iterator i;
  for (i = unknownNames.begin(); i != unknownNames.end()  &&  *graph.ptrKeepRunning; ++i)
  {
    QString item = i.key();
    int occurrences = i.value();
    QString plural = (occurrences == 1) ? "" : "s";
    parsed.errors.append("**** Unknown item " + item + " (" + QString::number(occurrences)
                          + " occurrence" + plural + ")");
  }

} // end buildGraph()



// Local functions follow -------------------------------------------------------

static void setDefaultOptions( QHash<QString, OptionType> &options )
{
  //               val enabled changed hasVal altName
  OptionType nope = {0, false,  false, false, ""};
  OptionType yup =  {0,  true,  false, false, ""};
  OptionType val =  {0,  true,  false, true,  ""};

  // These default to false:
  options["caseSensitive"]    = nope;
  options["requireColons"]    = nope;
  options["requireUsernames"] = nope;
  options["showMissing"]      = nope;
  options["sortByItem"]       = nope;
  options["allowDummies"]     = nope;
  options["showElapsedTime"]  = nope;
  options["verbose"]          = nope; // (1.4)
  // These default to true:
  options["showErrors"]       = yup;
  options["showRepeats"]      = yup;
  options["showLoops"]        = yup;
  options["showSummary"]      = yup;
  options["showNonTrades"]    = yup;
  options["showStats"]        = yup;

  val.value = CHAIN_SIZES_SOS;options["metric"]         = val; // (1.4)
  val.value = NO_PRIORITIES;  options["priorityScheme"] = val;
  val.value = 1;              options["smallStep"]      = val;
  val.value = 9;              options["bigStep"]        = val;
  val.value = 1000000000UL;   options["nonTradeCost"]   = val;
  val.value = 1;              options["iterations"]     = val;
  val.value = 0;              options["randSeed"]       = val;
}

static void setOption(QString optName, bool enabled, QString name, int val)
{
  gOptions[optName].enabled = enabled;
  gOptions[optName].value = val;
  gOptions[optName].changed = true;
  gOptions[optName].altName = name;
}

static bool fatalError(QWidget *parent, QString str, int line)
{
  QMessageBox::critical(parent, "Input Error", str+" (line "+QString::number(line)+")");

  return false;
}

