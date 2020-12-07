#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "flowgraph.h"
#include "liveness.h"
#include "table.h"

static Temp_tempList setUnion(Temp_tempList, Temp_tempList);
static Temp_tempList setDiff(Temp_tempList, Temp_tempList);
static bool setEqual(Temp_tempList, Temp_tempList);

Live_moveList Live_MoveList(G_node src, G_node dst, Live_moveList tail) {
	Live_moveList lm = (Live_moveList) checked_malloc(sizeof(*lm));
	lm->src = src;
	lm->dst = dst;
	lm->tail = tail;
	return lm;
}

Temp_temp Live_gtemp(G_node n) {
	return (Temp_temp) G_nodeInfo(n);
}

struct Live_graph Live_liveness(G_graph flow) {
	struct Live_graph lg = {G_Graph(), NULL};
	G_table liveMap = G_empty();
	G_nodeList nodes = G_nodes(flow);
	TAB_table nodeMap = TAB_empty();
	bool change = TRUE;

	while (change) {
		change = FALSE;
		G_nodeList p = NULL;
		for (p = nodes; p != NULL; p = p->tail) {
			G_nodeList q = NULL;
			Temp_tempList out = NULL;
			for (q = G_succ(p->head); q != NULL; q = q->tail)
				out = setUnion(
					out,
					setUnion(
						FG_use(q->head),
						setDiff(
							(Temp_tempList) G_look(liveMap, q->head),
							FG_def(q->head)
						)
					)
				);
			if (!setEqual(out, (Temp_tempList) G_look(liveMap, p->head))) {
				change = TRUE;
				G_enter(liveMap, p->head, out);
			}
		}
	}

	for (; nodes != NULL; nodes = nodes->tail) {
		Temp_tempList p = NULL;
		for (p = FG_def(nodes->head); p != NULL; p = p->tail) {
			G_node u = (G_node) TAB_look(nodeMap, p->head);
			Temp_tempList q = NULL;
			if (u == NULL) {
				u = G_Node(lg.graph, p->head);
				TAB_enter(nodeMap, p->head, u);
			}
			for (q = (Temp_tempList) G_look(liveMap, nodes->head); q != NULL; q = q->tail)
				if (!FG_isMove(nodes->head) || q->head != FG_use(nodes->head)->head) {
					G_node v = (G_node) TAB_look(nodeMap, q->head);
					if (v == NULL) {
						v = G_Node(lg.graph, q->head);
						TAB_enter(nodeMap, q->head, v);
					}
					assert(p->head != q->head || u == v);
					if (u != v) {
						G_addEdge(u, v);
						G_addEdge(v, u);
					}
				}
		}
		if (FG_isMove(nodes->head)) {
			Temp_temp t = FG_def(nodes->head)->head;
			Temp_temp s = FG_use(nodes->head)->head;
			G_node dst = (G_node) TAB_look(nodeMap, t);
			G_node src = (G_node) TAB_look(nodeMap, s);
			if (dst == NULL) {
				dst = G_Node(lg.graph, t);
				TAB_enter(nodeMap, t, dst);
			}
			if (src == NULL) {
				src = G_Node(lg.graph, s);
				TAB_enter(nodeMap, s, src);
			}
			lg.moves = Live_MoveList(src, dst, lg.moves);
		}
	}

	return lg;
}

static Temp_tempList setUnion(Temp_tempList left, Temp_tempList right) {
	if (left == NULL)
		return right;
	if (right == NULL)
		return left;
	
	if (left->head < right->head)
		return Temp_TempList(left->head, setUnion(left->tail, right));
	if (left->head == right->head)
		return Temp_TempList(left->head, setUnion(left->tail, right->tail));
	return Temp_TempList(right->head, setUnion(left, right->tail));
}

static Temp_tempList setDiff(Temp_tempList left, Temp_tempList right) {
	if (left == NULL)
		return NULL;
	if (right == NULL)
		return left;
	
	if (left->head < right->head)
		return Temp_TempList(left->head, setDiff(left->tail, right));
	if (left->head == right->head)
		return setDiff(left->tail, right->tail);
	return setDiff(left, right->tail);
}

static bool setEqual(Temp_tempList left, Temp_tempList right) {
	if (left == NULL && right == NULL)
		return TRUE;
	
	if (left == NULL || right == NULL)
		return FALSE;
	
	return left->head == right->head && setEqual(left->tail, right->tail);
}
