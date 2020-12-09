#include <stdio.h>
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "escape.h"
#include "table.h"

typedef struct escapeEntry_ {
	int depth;
	bool *escape;	
} *escapeEntry;

static escapeEntry EscapeEntry(int, bool *);
static void traverseExp(S_table, int, A_exp);
static void traverseDec(S_table, int, A_dec);
static void traverseVar(S_table, int, A_var);

void Esc_findEscape(A_exp exp) {
	S_table env = S_empty();

	traverseExp(env, 0, exp);
}

static escapeEntry EscapeEntry(int depth, bool *escape) {
	escapeEntry entry = (escapeEntry) checked_malloc(sizeof(*entry));

	entry->depth = depth;
	entry->escape = escape;

	return entry;
}

static void traverseExp(S_table env, int depth, A_exp exp) {
	switch (exp->kind) {
	case A_varExp: {
		traverseVar(env, depth, exp->u.var);
		break;
	}
	case A_callExp: {
		A_expList exps = NULL;
		for (exps = exp->u.call.args; exps != NULL; exps = exps->tail)
			traverseExp(env, depth, exps->head);
		break;
	}
	case A_opExp: {
		traverseExp(env, depth, exp->u.op.left);
		traverseExp(env, depth, exp->u.op.right);
		break;
	}
	case A_recordExp: {
		A_efieldList efields = NULL;
		for (efields = exp->u.record.fields; efields != NULL; efields = efields->tail)
			traverseExp(env, depth, efields->head->exp);
		break;
	}
	case A_seqExp: {
		A_expList exps = NULL;
		for (exps = exp->u.seq; exps != NULL; exps = exps->tail)
			traverseExp(env, depth, exps->head);
		break;
	}
	case A_assignExp: {
		traverseVar(env, depth, exp->u.assign.var);
		traverseExp(env, depth, exp->u.assign.exp);
		break;
	}
	case A_ifExp: {
		traverseExp(env, depth, exp->u.iff.test);
		traverseExp(env, depth, exp->u.iff.then);
		if (exp->u.iff.elsee != NULL)
			traverseExp(env, depth, exp->u.iff.elsee);
		break;
	}
	case A_whileExp: {
		traverseExp(env, depth, exp->u.whilee.test);
		traverseExp(env, depth, exp->u.whilee.body);
		break;
	}
	case A_forExp: {
		traverseExp(env, depth, exp->u.forr.lo);
		traverseExp(env, depth, exp->u.forr.hi);
		S_beginScope(env);
		S_enter(env, exp->u.forr.var, EscapeEntry(depth, &exp->u.forr.escape));
		traverseExp(env, depth, exp->u.forr.body);
		S_endScope(env);
		break;
	}
	case A_letExp: {
		A_decList decs = NULL;
		S_beginScope(env);
		for (decs = exp->u.let.decs; decs != NULL; decs = decs->tail)
			traverseDec(env, depth, decs->head);
		traverseExp(env, depth, exp->u.let.body);
		S_endScope(env);
		break;
	}
	case A_arrayExp: {
		traverseExp(env, depth, exp->u.array.size);
		traverseExp(env, depth, exp->u.array.init);
		break;
	}
	case A_nilExp:
	case A_intExp:
	case A_stringExp:
	case A_breakExp: {
		break;
	}
	default: {
		assert(0);
	}
	}
}

static void traverseDec(S_table env, int depth, A_dec dec) {
	switch (dec->kind) {
	case A_functionDec: {
		A_fundecList fundecs = NULL;
		for (fundecs = dec->u.function; fundecs != NULL; fundecs = fundecs->tail) {
			A_fieldList fields = NULL;
			S_beginScope(env);
			for (fields = fundecs->head->params; fields != NULL; fields = fields->tail)
				S_enter(env, fields->head->name, EscapeEntry(depth + 1, &fields->head->escape));
			traverseExp(env, depth + 1, fundecs->head->body);
			S_endScope(env);
		}
		break;
	}
	case A_varDec: {
		traverseExp(env, depth, dec->u.var.init);
		S_enter(env, dec->u.var.var, EscapeEntry(depth, &dec->u.var.escape));
		break;
	}
	case A_typeDec: {
		break;
	}
	default: {
		assert(0);
	}
	}
}

static void traverseVar(S_table env, int depth, A_var var) {
	switch (var->kind) {
	case A_simpleVar: {
		escapeEntry entry = (escapeEntry) S_look(env, var->u.simple);
		if (entry != NULL && depth > entry->depth)
			*entry->escape = TRUE;
		break;
	}
	case A_fieldVar: {
		traverseVar(env, depth, var->u.field.var);
		break;
	}
	case A_subscriptVar: {
		traverseVar(env, depth, var->u.subscript.var);
		traverseExp(env, depth, var->u.subscript.exp);
		break;
	}
	default: {
		assert(0);
	}
	}
}
