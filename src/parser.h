#include <QString>
#include "graph.h"
#include "MainWindow.h"

#ifndef PARSER_H
#define PARSER_H

//////////// OPTIONS

typedef struct
{
  QStringList officialNames, usedNames;
  QStringList errors;
  QStringList usernames;
  QList<QStringList> wantLists;

  int numItems, numDummyItems;
  int maxNameWidth;
} ParseDataType;

class MainWindow;
bool parseInput(MainWindow *parent, QString input, ParseDataType &parsed);
void buildGraph(MainWindow *parent, ParseDataType &parsed, Graph &graph);

//////////// OPTIONS

typedef struct
{
  unsigned int value; // This field is only applicable to some options
  bool enabled;
  bool changed;
  bool hasVal;
  QString altName; // the user knows the option by this name; the code by another
} OptionType;

typedef enum
{
  NO_PRIORITIES       = 0,
  LINEAR_PRIORITIES   = 1,
  TRIANGLE_PRIORITIES = 2,
  SQUARE_PRIORITIES   = 3,
  SCALED_PRIORITIES   = 4,
  EXPLICIT_PRIORITIES = 5,
} PRIORITY_TYPE;

typedef enum
{
  CHAIN_SIZES_SOS    = 0,
  USERS_TRADING      = 1,
  USERS_SOS          = 2,
  COMBINE_SHIPPING   = 3,
  // FAVOR_USER isn't implemented
} METRIC_TYPE;

#endif // PARSER_H

