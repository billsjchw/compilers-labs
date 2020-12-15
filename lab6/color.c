#include <stdio.h>
#include <string.h>
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
#include "table.h"

static G_table adjList;
static G_table degree;
static G_table moveList;
static G_table alias;
static G_table color;
static G_nodeList precolored;
static G_nodeList initial;
static G_nodeList simplifyWorklist;
static G_nodeList freezeWorklist;
static G_nodeList spillWorklist;
static G_nodeList spilledNodes;
static G_nodeList coalescedNodes;
static G_nodeList coloredNodes;
static G_nodeList selectStack;
static Live_moveList worklistMoves;
static Live_moveList activeMoves;

static void makeWorklist(void);
static void simplify(void);
static void coalesce(void);
static void freeze(void);
static void selectSpill(void);
static void assignColors(void);
static bool moveRelated(G_node);
static G_nodeList adjacent(G_node);
static void decrementDegree(G_node);
static G_node getAlias(G_node);
static void addWorklist(G_node);
static bool george(G_node, G_node);
static bool briggs(G_node, G_node);
static void combine(G_node, G_node);
static void freezeMoves(G_node);
static Live_moveList nodeMoves(G_node);
static void enableMoves(G_nodeList);
static void addEdge(G_node, G_node);
static bool inTempList(Temp_temp, Temp_tempList);
static Temp_tempList cloneTempList(Temp_tempList);
static Temp_tempList removeFromTempList(Temp_temp, Temp_tempList);
static G_nodeList cloneNodeList(G_nodeList);
static G_nodeList removeFromNodeList(G_node, G_nodeList);
static bool inMoveList(G_node, G_node, Live_moveList);
static Live_moveList cloneMoveList(Live_moveList);
static Live_moveList removeFromMoveList(G_node, G_node, Live_moveList);

struct COL_result COL_color(struct Live_graph lg) {
	struct COL_result ret = {Temp_empty(), NULL};
	G_nodeList nodeItr = NULL;
	Live_moveList moveItr = NULL;
	Temp_tempList tempItr = NULL;

	adjList = G_empty();
	degree = G_empty();
	for (nodeItr = G_nodes(lg.graph); nodeItr != NULL; nodeItr = nodeItr->tail) {
		int *degPtr = (int *) checked_malloc(sizeof(int));
		*degPtr = G_degree(nodeItr->head) / 2;
		G_enter(degree, nodeItr->head, degPtr);
		G_enter(adjList, nodeItr->head, cloneNodeList(G_succ(nodeItr->head)));
	}
	moveList = G_empty();
	for (moveItr = lg.moves; moveItr != NULL; moveItr = moveItr->tail) {
		Live_moveList moves = (Live_moveList) G_look(moveList, moveItr->src);
		if (!inMoveList(moveItr->src, moveItr->dst, moves))
			G_enter(moveList, moveItr->src, Live_MoveList(moveItr->src, moveItr->dst, moves));
		moves = (Live_moveList) G_look(moveList, moveItr->dst);
		if (!inMoveList(moveItr->src, moveItr->dst, moves))
			G_enter(moveList, moveItr->dst, Live_MoveList(moveItr->src, moveItr->dst, moves));
	}
	alias = G_empty();
	color = G_empty();
	precolored = NULL;
	initial = NULL;
	for (nodeItr = G_nodes(lg.graph); nodeItr != NULL; nodeItr = nodeItr->tail)
		if (inTempList(Live_gtemp(nodeItr->head), F_registers())) {
			G_enter(color, nodeItr->head, Live_gtemp(nodeItr->head));
			precolored = G_NodeList(nodeItr->head, precolored);
		} else {
			initial = G_NodeList(nodeItr->head, initial);
		}
	simplifyWorklist = NULL;
	freezeWorklist = NULL;
	spillWorklist = NULL;
	spilledNodes = NULL;
	coalescedNodes = NULL;
	coloredNodes = NULL;
	selectStack = NULL;
	worklistMoves = NULL;
	for (moveItr = lg.moves; moveItr != NULL; moveItr = moveItr->tail)
		if (!inMoveList(moveItr->src, moveItr->dst, worklistMoves))
			worklistMoves = Live_MoveList(moveItr->src, moveItr->dst, worklistMoves);
	activeMoves = NULL;

	makeWorklist();
	while (simplifyWorklist != NULL || worklistMoves != NULL || freezeWorklist != NULL || spillWorklist != NULL)
		if (simplifyWorklist != NULL)
			simplify();
		else if (worklistMoves != NULL)
			coalesce();
		else if (freezeWorklist != NULL)
			freeze();
		else
			selectSpill();
	assignColors();

	for (nodeItr = G_nodes(lg.graph); nodeItr != NULL; nodeItr = nodeItr->tail) {
		Temp_temp colorTemp = (Temp_temp) G_look(color, nodeItr->head);
		if (colorTemp != NULL)
			Temp_enter(ret.coloring, Live_gtemp(nodeItr->head), Temp_look(F_tempMap(), colorTemp));
	}
	for (nodeItr = spilledNodes; nodeItr != NULL; nodeItr = nodeItr->tail)
		ret.spills = Temp_TempList(Live_gtemp(nodeItr->head), ret.spills);

	return ret;
}

static G_table adjList = NULL;
static G_table degree = NULL;
static G_table moveList = NULL;
static G_table alias = NULL;
static G_table color = NULL;
static G_nodeList precolored = NULL;
static G_nodeList initial = NULL;
static G_nodeList simplifyWorklist = NULL;
static G_nodeList freezeWorklist = NULL;
static G_nodeList spillWorklist = NULL;
static G_nodeList spilledNodes = NULL;
static G_nodeList coalescedNodes = NULL;
static G_nodeList coloredNodes = NULL;
static G_nodeList selectStack = NULL;
static Live_moveList worklistMoves = NULL;
static Live_moveList activeMoves = NULL;

static void makeWorklist(void) {
	G_nodeList nodeItr = NULL;

	for (nodeItr = initial; nodeItr != NULL; nodeItr = nodeItr->tail) {
		int *degPtr = (int *) G_look(degree, nodeItr->head);
		if (*degPtr >= F_regsNum)
			spillWorklist = G_NodeList(nodeItr->head, spillWorklist);
		else if (moveRelated(nodeItr->head))
			freezeWorklist = G_NodeList(nodeItr->head, freezeWorklist);
		else
			simplifyWorklist = G_NodeList(nodeItr->head, simplifyWorklist);
	}
}

static void simplify(void) {
	G_node node = simplifyWorklist->head;
	G_nodeList nodeItr = NULL;

	simplifyWorklist = simplifyWorklist->tail;
	selectStack = G_NodeList(node, selectStack);
	for (nodeItr = adjacent(node); nodeItr != NULL; nodeItr = nodeItr->tail)
		decrementDegree(nodeItr->head);
	freezeMoves(node);
}

static void coalesce(void) {
	G_node src = worklistMoves->src;
	G_node dst = worklistMoves->dst;
	G_node u = NULL;
	G_node v = NULL;

	worklistMoves = worklistMoves->tail;
	if (G_inNodeList(getAlias(dst), precolored)) {
		u = getAlias(dst);
		v = getAlias(src);
	} else {
		u = getAlias(src);
		v = getAlias(dst);
	}
	if (u == v) {
		addWorklist(u);
	} else if (G_inNodeList(v, precolored) || G_inNodeList(v, (G_nodeList) G_look(adjList, u))) {
		addWorklist(u);
		addWorklist(v);
	} else if ((G_inNodeList(u, precolored) && george(u, v)) || (!G_inNodeList(u, precolored) && briggs(u, v))) {
		combine(u, v);
		addWorklist(u);
	} else {
		activeMoves = Live_MoveList(src, dst, activeMoves);
	}
}

static void freeze(void) {
	G_node node = freezeWorklist->head;

	freezeWorklist = freezeWorklist->tail;
	simplifyWorklist = G_NodeList(node, simplifyWorklist);
	freezeMoves(node);
}

static void selectSpill(void) {
	G_node node = NULL;
	G_nodeList nodeItr = NULL;

	for (nodeItr = spillWorklist; nodeItr != NULL; nodeItr = nodeItr->tail)
		if (node == NULL)
			node = nodeItr->head;
		else if (inTempList(Live_gtemp(node), AS_badSpillChoices()))
			node = nodeItr->head;
		else if (!inTempList(Live_gtemp(nodeItr->head), AS_badSpillChoices()) &&
		         *((int *) G_look(degree, nodeItr->head)) > *((int *) G_look(degree, node)))
			node = nodeItr->head;
	spillWorklist = removeFromNodeList(node, spillWorklist);
	simplifyWorklist = G_NodeList(node, simplifyWorklist);
	freezeMoves(node);
}

static void assignColors(void) {
	G_nodeList nodeItr = NULL;
	while (selectStack != NULL) {
		G_node node = selectStack->head;
		Temp_tempList okColors = cloneTempList(F_registers());
		for (nodeItr = (G_nodeList) G_look(adjList, node); nodeItr != NULL; nodeItr = nodeItr->tail) {
			Temp_temp colorTemp = (Temp_temp) G_look(color, getAlias(nodeItr->head));
			if (colorTemp != NULL)
				okColors = removeFromTempList(colorTemp, okColors);
		}
		if (okColors == NULL) {
			spilledNodes = G_NodeList(node, spilledNodes);
		} else {
			coloredNodes = G_NodeList(node, coloredNodes);
			G_enter(color, node, okColors->head);
		}
		selectStack = selectStack->tail;
	}
	for (nodeItr = coalescedNodes; nodeItr != NULL; nodeItr = nodeItr->tail) {
		Temp_temp colorTemp = (Temp_temp) G_look(color, getAlias(nodeItr->head));
		if (colorTemp != NULL)
			G_enter(color, nodeItr->head, colorTemp);
	}
}

static bool moveRelated(G_node node) {
	return nodeMoves(node) != NULL;
}

static G_nodeList adjacent(G_node node) {
	G_nodeList ret = NULL;
	G_nodeList nodeItr = NULL;

	for (nodeItr = (G_nodeList) G_look(adjList, node); nodeItr != NULL; nodeItr = nodeItr->tail)
		if (!G_inNodeList(nodeItr->head, selectStack) && !G_inNodeList(nodeItr->head, coalescedNodes))
			ret = G_NodeList(nodeItr->head, ret);
	
	return ret;
}

static void decrementDegree(G_node node) {
	int *degPtr = (int *) G_look(degree, node);
	if ((*degPtr)-- == F_regsNum) {
		enableMoves(G_NodeList(node, adjacent(node)));
		if (!G_inNodeList(node, precolored)) {
			spillWorklist = removeFromNodeList(node, spillWorklist);
			if (moveRelated(node))
				freezeWorklist = G_NodeList(node, freezeWorklist);
			else
				simplifyWorklist = G_NodeList(node, simplifyWorklist);
		}
	}
}

static G_node getAlias(G_node node) {
	return G_inNodeList(node, coalescedNodes) ? getAlias((G_node) G_look(alias, node)) : node;
}

static void addWorklist(G_node node) {
	int *degPtr = (int *) G_look(degree, node);

	if (!G_inNodeList(node, precolored) && !moveRelated(node) && *degPtr < F_regsNum) {
		freezeWorklist = removeFromNodeList(node, freezeWorklist);
		simplifyWorklist = G_NodeList(node, simplifyWorklist);
	}
}

static bool george(G_node u, G_node v) {
	G_nodeList nodeItr = NULL;

	for (nodeItr = adjacent(v); nodeItr != NULL; nodeItr = nodeItr->tail) {
		int *degPtr = (int *) G_look(degree, nodeItr->head);
		if (*degPtr >= F_regsNum && !G_inNodeList(nodeItr->head, precolored) &&
		    !G_inNodeList(nodeItr->head, (G_nodeList) G_look(adjList, u)))
			return FALSE;
	}

	return TRUE;
}

static bool briggs(G_node u, G_node v) {
	G_nodeList nodes = NULL;
	G_nodeList nodeItr = NULL;
	int k = 0;

	for (nodeItr = adjacent(u); nodeItr != NULL; nodeItr = nodeItr->tail)
		if (!G_inNodeList(nodeItr->head, nodes))
			nodes = G_NodeList(nodeItr->head, nodes);
	for (nodeItr = adjacent(v); nodeItr != NULL; nodeItr = nodeItr->tail)
		if (!G_inNodeList(nodeItr->head, nodes))
			nodes = G_NodeList(nodeItr->head, nodes);
	
	for (nodeItr = nodes; nodeItr != NULL; nodeItr = nodeItr->tail) {
		int *degPtr = (int *) G_look(degree, nodeItr->head);
		if (*degPtr >= F_regsNum)
			++k;
	}

	return k < F_regsNum;
}

static void combine(G_node u, G_node v) {
	Live_moveList moves = NULL;
	Live_moveList moveItr = NULL;
	G_nodeList nodeItr = NULL;
	int *degPtr = NULL;

	if (G_inNodeList(v, freezeWorklist))
		freezeWorklist = removeFromNodeList(v, freezeWorklist);
	else
		spillWorklist = removeFromNodeList(v, spillWorklist);
	coalescedNodes = G_NodeList(v, coalescedNodes);
	G_enter(alias, v, u);
	moves = cloneMoveList((Live_moveList) G_look(moveList, u));
	for (moveItr = (Live_moveList) G_look(moveList, v); moveItr != NULL; moveItr = moveItr->tail)
		if (!inMoveList(moveItr->src, moveItr->dst, moves))
			moves = Live_MoveList(moveItr->src, moveItr->dst, moves);
	G_enter(moveList, u, moves);
	enableMoves(G_NodeList(v, NULL));
	for (nodeItr = adjacent(v); nodeItr != NULL; nodeItr = nodeItr->tail) {
		addEdge(nodeItr->head, u);
		decrementDegree(nodeItr->head);
	}
	degPtr = (int *) G_look(degree, u);
	if (*degPtr >= F_regsNum && G_inNodeList(u, freezeWorklist)) {
		freezeWorklist = removeFromNodeList(u, freezeWorklist);
		spillWorklist = G_NodeList(u, spillWorklist);
	}
}

static void freezeMoves(G_node u) {
	Live_moveList moveItr = NULL;
	for (moveItr = nodeMoves(u); moveItr != NULL; moveItr = moveItr->tail) {
		G_node v = getAlias(moveItr->dst) == getAlias(u) ? getAlias(moveItr->src) : getAlias(moveItr->dst);
		int *degPtr = (int *) G_look(degree, v);
		activeMoves = removeFromMoveList(moveItr->src, moveItr->dst, activeMoves);
		if (!G_inNodeList(v, precolored) && !moveRelated(v) && *degPtr < F_regsNum) {
			freezeWorklist = removeFromNodeList(v, freezeWorklist);
			simplifyWorklist = G_NodeList(v, simplifyWorklist);
		}
	}
}

static Live_moveList nodeMoves(G_node node) {
	Live_moveList ret = NULL;
	Live_moveList moveItr = NULL;

	for (moveItr = (Live_moveList) G_look(moveList, node); moveItr != NULL; moveItr = moveItr->tail)
		if (inMoveList(moveItr->src, moveItr->dst, activeMoves) || inMoveList(moveItr->src, moveItr->dst, worklistMoves))
			ret = Live_MoveList(moveItr->src, moveItr->dst, ret);

	return ret;
}

static void enableMoves(G_nodeList nodes) {
	G_nodeList nodeItr = NULL;
	Live_moveList moveItr = NULL;

	for (nodeItr = nodes; nodeItr != NULL; nodeItr = nodeItr->tail)
		for (moveItr = nodeMoves(nodeItr->head); moveItr != NULL; moveItr = moveItr->tail)
			if (inMoveList(moveItr->src, moveItr->dst, activeMoves)) {
				activeMoves = removeFromMoveList(moveItr->src, moveItr->dst, activeMoves);
				worklistMoves = Live_MoveList(moveItr->src, moveItr->dst, worklistMoves);
			}
}

static void addEdge(G_node u, G_node v) {
	G_nodeList nodes = NULL;
	int *degPtr = NULL;

	if (u == v)
		return;

	nodes = (G_nodeList) G_look(adjList, u);
	degPtr = (int *) G_look(degree, u);
	if (!G_inNodeList(v, nodes)) {
		G_enter(adjList, u, G_NodeList(v, nodes));
		++*degPtr;
	}
	nodes = (G_nodeList) G_look(adjList, v);
	degPtr = (int *) G_look(degree, v);
	if (!G_inNodeList(u, nodes)) {
		G_enter(adjList, v, G_NodeList(u, nodes));
		++*degPtr;
	}
}

static bool inTempList(Temp_temp temp, Temp_tempList temps) {
	return temps == NULL ? FALSE : temp == temps->head || inTempList(temp, temps->tail);
}

static Temp_tempList cloneTempList(Temp_tempList temps) {
	return temps == NULL ? NULL : Temp_TempList(temps->head, cloneTempList(temps->tail));
}

static Temp_tempList removeFromTempList(Temp_temp temp, Temp_tempList temps) {
	if (temps == NULL)
		return NULL;
	else if (temps->head == temp)
		return removeFromTempList(temp, temps->tail);
	else
		return Temp_TempList(temps->head, removeFromTempList(temp, temps->tail));
}

static G_nodeList cloneNodeList(G_nodeList nodes) {
	return nodes == NULL ? NULL : G_NodeList(nodes->head, cloneNodeList(nodes->tail));
}

static G_nodeList removeFromNodeList(G_node node, G_nodeList nodes) {
	if (nodes == NULL)
		return NULL;
	else if (nodes->head == node)
		return removeFromNodeList(node, nodes->tail);
	else
		return G_NodeList(nodes->head, removeFromNodeList(node, nodes->tail));
}

static bool inMoveList(G_node src, G_node dst, Live_moveList moves) {
	return moves == NULL ? FALSE : (moves->src == src && moves->dst == dst) || inMoveList(src, dst, moves->tail);
}

static Live_moveList cloneMoveList(Live_moveList moves) {
	return moves == NULL ? NULL : Live_MoveList(moves->src, moves->dst, cloneMoveList(moves->tail));
}

static Live_moveList removeFromMoveList(G_node src, G_node dst, Live_moveList moves) {
	if (moves == NULL)
		return NULL;
	else if (moves->src == src && moves->dst == dst)
		return removeFromMoveList(src, dst, moves->tail);
	else
		return Live_MoveList(src, dst, removeFromMoveList(src, dst, moves->tail));
}
