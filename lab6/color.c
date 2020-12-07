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

static int tempListLen(Temp_tempList);
static bool inTempList(Temp_temp, Temp_tempList);

struct COL_result COL_color(G_graph ig, Temp_map initial, Temp_tempList regs) {
	struct COL_result ret = {Temp_empty(), NULL};
	G_nodeList nodes = G_nodes(ig);
	G_nodeList stack = NULL;
	int regsNum = tempListLen(regs);
	TAB_table colorMap = TAB_empty();
	TAB_table degreeMap = TAB_empty();
	G_nodeList p = NULL;

	for (p = nodes; p != NULL; p = p->tail) {
		int *degree = (int *) checked_malloc(sizeof(int));
		*degree = G_degree(p->head) / 2;
		TAB_enter(degreeMap, p->head, degree);
	}

	while (TRUE) {
		G_node nodeSelected = NULL;
		for (p = nodes; p != NULL; p = p->tail) {
			if (Temp_look(initial, Live_gtemp(p->head)) == NULL && !G_inNodeList(p->head, stack)) {
				int *degree = (int *) TAB_look(degreeMap, p->head);
				nodeSelected = p->head;
				if (*degree < regsNum)
					break;
			}
		}
		if (nodeSelected == NULL)
			break;
		stack = G_NodeList(nodeSelected, stack);
		for (p = G_succ(nodeSelected); p != NULL; p = p->tail) {
			int *degree = (int *) TAB_look(degreeMap, p->head);
			--(*degree);
		}
	}

	while (stack != NULL) {
		Temp_temp tempToBeColored = Live_gtemp(stack->head);
		Temp_temp colorSelected = NULL;
		Temp_tempList neighbourColors = NULL;
		Temp_tempList q = NULL;
		for (p = G_succ(stack->head); p != NULL; p = p->tail) {
			Temp_temp temp = Live_gtemp(p->head);
			if (Temp_look(initial, temp) != NULL) {
				neighbourColors = Temp_TempList(temp, neighbourColors);
			} else {
				Temp_temp color = (Temp_temp) TAB_look(colorMap, temp);
				if (color != NULL)
					neighbourColors = Temp_TempList(color, neighbourColors);
			}
		}
		for (q = regs; q != NULL; q = q->tail)
			if (!inTempList(q->head, neighbourColors)) {
				colorSelected = q->head;
				break;
			}
		if (colorSelected == NULL) {
			ret.spills = Temp_TempList(tempToBeColored, ret.spills);
		} else {
			Temp_enter(ret.coloring, tempToBeColored, Temp_look(F_tempMap(), colorSelected));
			TAB_enter(colorMap, tempToBeColored, colorSelected);
		}
		stack = stack->tail;
	}

	return ret;
}

static int tempListLen(Temp_tempList temps) {
	return temps == NULL ? 0 : 1 + tempListLen(temps->tail);
}

static bool inTempList(Temp_temp temp, Temp_tempList temps) {
	return temps == NULL ? FALSE : temp == temps->head || inTempList(temp, temps->tail);
}
