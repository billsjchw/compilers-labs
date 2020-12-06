#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "liveness.h"
#include "color.h"
#include "regalloc.h"
#include "table.h"
#include "flowgraph.h"

struct RA_result RA_regAlloc(F_frame frame, AS_instrList insts) {
	struct RA_result ret = {NULL, NULL};
	G_graph flow = FG_AssemFlowGraph(insts);
	struct Live_graph lg = Live_liveness(flow);
	struct COL_result cr = COL_color(lg.graph, F_tempMap(), F_registers());

	ret.coloring = cr.coloring;
	ret.il = insts;

	return ret;
}
