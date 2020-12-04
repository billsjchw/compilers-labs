/*
 * flowgraph.h - Function prototypes to represent control flow graphs.
 */

#ifndef FLOWGRAPH_H
#define FLOWGRAPH_H

#include "graph.h"
#include "temp.h"
#include "util.h"
#include "assem.h"

Temp_tempList FG_def(G_node);
Temp_tempList FG_use(G_node);
bool FG_isMove(G_node);
G_graph FG_AssemFlowGraph(AS_instrList);

#endif
