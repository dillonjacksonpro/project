#ifndef AGGREGATION_H
#define AGGREGATION_H

#include "orch_common.h"

void node_agg_update(NodeAgg *agg, const CsvRow *row);
void node_agg_merge(NodeAgg *dst, const NodeAgg *src);

#endif
