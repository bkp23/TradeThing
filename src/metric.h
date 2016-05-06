#ifndef METRIC_H
#define METRIC_H

#include "parser.h" // for METRIC_TYPE
#include "graph.h"  // for CyclesType

QString metricString(METRIC_TYPE metricType);
int calculateMetric(CyclesType *ptrCycles, METRIC_TYPE metricType);

#endif // METRIC_H
