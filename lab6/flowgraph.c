#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "flowgraph.h"
#include "errormsg.h"
#include "table.h"

Temp_tempList FG_def(G_node n) {
	AS_instr inst = (AS_instr) G_nodeInfo(n);

	switch (inst->kind) {
	case I_OPER:
		return inst->u.OPER.dst;
	case I_MOVE:
		return inst->u.MOVE.dst;
	case I_LABEL:
		return NULL;
	default:
		assert(0);
	}
}

Temp_tempList FG_use(G_node n) {
	AS_instr inst = (AS_instr) G_nodeInfo(n);

	switch (inst->kind) {
	case I_OPER:
		return inst->u.OPER.src;
	case I_MOVE:
		return inst->u.MOVE.src;
	case I_LABEL:
		return NULL;
	default:
		assert(0);
	}
}

bool FG_isMove(G_node n) {
	AS_instr inst = (AS_instr) G_nodeInfo(n);

	return inst->kind == I_MOVE;
}

G_graph FG_AssemFlowGraph(AS_instrList insts) {
	G_graph graph = G_Graph();
	G_nodeList nodes = NULL;
	G_nodeList p = NULL;

	for (; insts != NULL; insts = insts->tail)
		G_Node(graph, insts->head);

	nodes = G_nodes(graph);
	for (p = nodes; p != NULL; p = p->tail) {
		AS_instr inst = (AS_instr) G_nodeInfo(p->head);
		Temp_labelList labels = inst->kind == I_OPER ? inst->u.OPER.jumps : NULL;
		if (labels == NULL) {
			if (p->tail != NULL)
				G_addEdge(p, p->tail->head);
		} else {
			G_nodeList q = NULL;
			for (q = nodes; q != NULL; q = q->tail) {
				AS_instr inst = (AS_instr) G_nodeInfo(p->head);
				if (inst->kind == I_LABEL && inst->u.LABEL.label) {
					Temp_labelList r = NULL;
					for (r = labels; r != NULL; r = r->tail)
						if (r->head == inst->u.LABEL.label)
							G_addEdge(p->head, q->head);
				}
			}
		}
	}

	return graph;
}
