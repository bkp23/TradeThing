#include "metric.h"
#include <QString> // for returning strings

static METRIC_TYPE lastMetricType;
static QString sMetString;

static int calcSumOfSquares(CyclesType *ptrCycles);
static int calcUsersTrading(CyclesType *ptrCycles);
static int calcUsersSumOfSquares(CyclesType *ptrCycles);
static int calcCombineShipping(CyclesType *ptrCycles);



QString metricString(METRIC_TYPE metricType)
{
  if (lastMetricType != metricType)
    return "Error: Metric was never computed or type has changed";
  return sMetString; // Report back the string put together earlier
}


int calculateMetric(CyclesType *ptrCycles, METRIC_TYPE metricType)
{
  int ret;

  if (ptrCycles == NULL)
    return 0;

  lastMetricType = metricType;
  switch (metricType)
  {
    case CHAIN_SIZES_SOS:   ret = calcSumOfSquares(ptrCycles);      break;
    case USERS_TRADING:     ret = calcUsersTrading(ptrCycles);      break;
    case USERS_SOS:         ret = calcUsersSumOfSquares(ptrCycles); break;
    case COMBINE_SHIPPING:  ret = calcCombineShipping(ptrCycles);   break;
    default:
      Q_ASSERT(false);
      ret = -1;
  }
  return ret;
}


static int calcSumOfSquares(CyclesType *ptrCycles)
{
  int sum = 0;
  QList<int> groups;
  //groups.reserve(ptrCycles->size());

  for(int idx=0; idx<ptrCycles->size(); idx++)
  {
    sum += ptrCycles->at(idx)->size() * ptrCycles->at(idx)->size();
    groups.append(ptrCycles->at(idx)->size());
  }
  std::sort(groups.begin(), groups.end());
  sMetString = "[ " + QString::number(sum) + " :";
  for (int j = groups.size()-1; j >= 0; j--)
    sMetString = sMetString + " " + QString::number(groups.at(j));

  sMetString += " ]";
  return sum;
}


static int calcUsersTrading(CyclesType *ptrCycles)
{
  QStringList users;
  int count;

  for(int idx=0; idx<ptrCycles->size(); idx++)
    for(int j=0; j<ptrCycles->at(idx)->size(); j++)
    {
      QString name = ptrCycles->at(idx)->at(j).owner;
      if (!users.contains(name))
        users.append(name);
    }

  count = users.size();

  sMetString = "[ users trading = " + QString::number(count) + " ]";
  return -count;
}


static int calcUsersSumOfSquares(CyclesType *ptrCycles)
{
  QHash<QString,int> users;
  QHash<QString,int>::iterator iter;
  int sum = 0;

  for(int idx=0; idx<ptrCycles->size(); idx++)
    for(int j=0; j<ptrCycles->at(idx)->size(); j++)
    {
      QString name = ptrCycles->at(idx)->at(j).owner;
      users[name] = 1 + users.value(name,0);
    }

  sMetString = "[ users trading = " + QString::number(users.size());

  for (iter = users.begin(); iter != users.end(); iter++)
    sum += iter.value() * iter.value();

  sMetString += ", sum of squares = " + QString::number(sum) + " ]";
  return sum;
}


static int calcCombineShipping(CyclesType *ptrCycles)
{
  QHash<QString,int> pairs;
  QHash<QString,int>::iterator iter;
  int count = 0;

  for(int idx=0; idx<ptrCycles->size(); idx++)
    for(int j=0; j<ptrCycles->at(idx)->size(); j++)
    {
      Node n = ptrCycles->at(idx)->at(j);
      QString key  = n.owner + " receives " + n.ptrMatch->ptrTwin->owner;
      pairs[key] = 1 + pairs.value(key,0);
    }

  for (iter = pairs.begin(); iter != pairs.end(); iter++)
    if (iter.value() > 1)
      count += iter.value()-1;

  sMetString = "[ combine shipping = " + QString::number(count) + " ]";
  return -count;
}

