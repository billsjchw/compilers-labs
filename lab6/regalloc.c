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
	struct COL_result cr = COL_color(Live_liveness(FG_AssemFlowGraph(insts)));

	while (cr.spills != NULL) {
		insts = AS_rewriteSpill(frame, insts, cr.spills);
		cr = COL_color(Live_liveness(FG_AssemFlowGraph(insts)));
	}

	ret.coloring = cr.coloring;
	ret.il = insts;

	return ret;
}
