#include "prog1.h"
#include "string.h"
#include "stdio.h"

typedef struct table *Table_;
typedef struct intAndTable *IntAndTable_;

struct table {
	string id;
	int value;
	Table_ tail;
};
static Table_ Table(string, int, Table_);

struct intAndTable {
	int i;
	Table_ table;
};
static IntAndTable_ IntAndTable(int, Table_);

int maxargs(A_stm);
void interp(A_stm);

static int max(int, int);
static int calc(int, A_binop, int);
static int expListLength(A_expList);
static int maxargsExp(A_exp);
static int maxargsExpList(A_expList);
static Table_ update(Table_, string, int);
static int lookup(Table_, string);
static Table_ interpStm(A_stm, Table_);
static IntAndTable_ interpExp(A_exp, Table_);
static Table_ interpExpList(A_expList, Table_);

int maxargs(A_stm stm) {
	switch (stm->kind) {
	case A_compoundStm:
		return max(maxargs(stm->u.compound.stm1),
				maxargs(stm->u.compound.stm2));
	case A_assignStm:
		return maxargsExp(stm->u.assign.exp);
	case A_printStm:
		return max(expListLength(stm->u.print.exps),
				maxargsExpList(stm->u.print.exps));
	default:
		assert(0);
	}
}

void interp(A_stm stm) {
	interpStm(stm, NULL);
}

static Table_ Table(string id, int value, Table_ tail) {
	Table_ table = checked_malloc(sizeof(*table));
	
	table->id = id;
	table->value = value;
	table->tail = tail;
	
	return table;
}

static IntAndTable_ IntAndTable(int i, Table_ table) {
	IntAndTable_ intAndTable = checked_malloc(sizeof(*intAndTable));
	
	intAndTable->i = i;
	intAndTable->table = table;

	return intAndTable;
}

static int max(int a, int b) {
	return a > b ? a : b;
}

static int calc(int left, A_binop binop, int right) {
	switch (binop) {
	case A_plus:
		return left + right;
	case A_minus:
		return left - right;
	case A_times:
		return left * right;
	case A_div:
		return left / right;
	default:
		assert(0);
	}
}

static int expListLength(A_expList expList) {
	switch (expList->kind) {
	case A_pairExpList:
		return 1 + expListLength(expList->u.pair.tail);
	case A_lastExpList:
		return 1;
	default:
		assert(0);
	}
}

static int maxargsExp(A_exp exp) {
	switch (exp->kind) {
	case A_idExp:
	case A_numExp:
		return 0;
	case A_opExp:
		return max(maxargsExp(exp->u.op.left), maxargsExp(exp->u.op.right));
	case A_eseqExp:
		return max(maxargs(exp->u.eseq.stm), maxargsExp(exp->u.eseq.exp));
	default:
		assert(0);
	}
}

static int maxargsExpList(A_expList expList) {
	switch (expList->kind) {
	case A_pairExpList:
		return max(maxargsExp(expList->u.pair.head),
				maxargsExpList(expList->u.pair.tail));
	case A_lastExpList:
		return maxargsExp(expList->u.last);
	default:
		assert(0);
	}
}

static Table_ update(Table_ table, string id, int value) {
	return Table(id, value, table);
}

static int lookup(Table_ table, string id) {
	if (table == NULL)
		return 0;
	else if (strcmp(table->id, id) == 0)
		return table->value;
	else
		return lookup(table->tail, id);
}

static Table_ interpStm(A_stm stm, Table_ table) {
	switch (stm->kind) {
	case A_compoundStm: {
		return interpStm(stm->u.compound.stm2,
				interpStm(stm->u.compound.stm1, table));
	}
	case A_assignStm: {
		IntAndTable_ intAndTable = interpExp(stm->u.assign.exp, table);
		return update(intAndTable->table, stm->u.assign.id, intAndTable->i);
	}
	case A_printStm: {
		return interpExpList(stm->u.print.exps, table);
	}
	default: {
		assert(0);
	}
	}
}

static IntAndTable_ interpExp(A_exp exp, Table_ table) {
	switch (exp->kind) {
	case A_idExp: {
		return IntAndTable(lookup(table, exp->u.id), table);
	}
	case A_numExp: {
		return IntAndTable(exp->u.num, table);
	}
	case A_opExp: {
		IntAndTable_ intAndTableLeft = interpExp(exp->u.op.left, table);
		IntAndTable_ intAndTableRight = interpExp(exp->u.op.right,
				intAndTableLeft->table);
		return IntAndTable(calc(intAndTableLeft->i, exp->u.op.oper,
				intAndTableRight->i), intAndTableRight->table);
	}
	case A_eseqExp: {
		return interpExp(exp->u.eseq.exp, interpStm(exp->u.eseq.stm, table));
	}
	default: {
		assert(0);
	}
	}
}

static Table_ interpExpList(A_expList expList, Table_ table) {
	switch (expList->kind) {
	case A_pairExpList: {
		IntAndTable_ intAndTable = interpExp(expList->u.pair.head, table);
		printf("%d ", intAndTable->i);
		return interpExpList(expList->u.pair.tail, intAndTable->table);
	}
	case A_lastExpList: {
		IntAndTable_ intAndTable = interpExp(expList->u.last, table);
		printf("%d\n", intAndTable->i);
		return intAndTable->table;
	}
	default: {
		assert(0);
	}
	}
}
