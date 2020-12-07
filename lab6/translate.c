#include <stdio.h>
#include "util.h"
#include "table.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "tree.h"
#include "printtree.h"
#include "frame.h"
#include "translate.h"

//LAB5: you can modify anything you want.

struct Tr_access_ {
	Tr_level level;
	F_access access;
};

struct Tr_level_ {
	F_frame frame;
	Tr_level parent;
};

typedef struct patchList_ *patchList;
struct patchList_ 
{
	Temp_label *head; 
	patchList tail;
};

struct Cx 
{
	patchList trues; 
	patchList falses; 
	T_stm stm;
};

struct Tr_exp_ {
	enum {Tr_ex, Tr_nx, Tr_cx} kind;
	union {T_exp ex; T_stm nx; struct Cx cx; } u;
};

struct Tr_expList_ {
    Tr_exp head;
    Tr_expList tail;
};

static struct Tr_level_ outermostLevel;
static F_fragList frags;

static Tr_access Tr_Access(Tr_level, F_access);
static Tr_exp Tr_Ex(T_exp);
static Tr_exp Tr_Nx(T_stm);
static Tr_exp Tr_Cx(patchList, patchList, T_stm);
static T_exp unEx(Tr_exp);
static T_stm unNx(Tr_exp);
static struct Cx unCx(Tr_exp);
static patchList PatchList(Temp_label *, patchList);
static void doPatch(patchList, Temp_label);
static patchList joinPatch(patchList, patchList);
static Tr_accessList accessListToAccessList(F_accessList, Tr_level);
static T_exp simpleVarHelper(Tr_access, Tr_level, T_exp);
static T_expList expListToExpList(Tr_expList);
static T_exp callExpStaticLink(Tr_level, Tr_level, T_exp);
static Tr_exp simpleCx(T_relOp, Tr_exp, Tr_exp);
static T_stm recordExpHelper(Tr_expList, int, Temp_temp);
static int expListLen(Tr_expList);
static T_stm expListToSeq(Tr_expList);

Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail) {
    Tr_expList exps = checked_malloc(sizeof(*exps));

    exps->head = head;
    exps->tail = tail;

    return exps;
}

Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail) {
    Tr_accessList accesses = checked_malloc(sizeof(*accesses));

    accesses->head = head;
    accesses->tail = tail;

    return accesses;
}

Tr_level Tr_outermost(void) {
    return &outermostLevel;
}

Tr_level Tr_newLevel(Tr_level parent, Temp_label label, U_boolList escapes) {
    F_frame frame = F_newFrame(label, U_BoolList(TRUE, escapes));
    Tr_level level = checked_malloc(sizeof(*level));

    level->frame = frame;
    level->parent = parent;

    return level;
}

Tr_accessList Tr_formals(Tr_level level) {
    F_accessList accesses = F_formals(level->frame);

    return accessListToAccessList(accesses, level);
}

Tr_access Tr_allocLocal(Tr_level level, bool escape) {
    F_access access = F_allocLocal(level->frame, escape);

    return Tr_Access(level, access);
}

void Tr_procEntryExit(Tr_level level, Tr_exp body) {
    T_stm stm = F_procEntryExit1(level->frame, T_Move(T_Temp(F_RAX()), unEx(body)));
    
    frags = F_FragList(F_ProcFrag(stm, level->frame), frags);
}

F_fragList Tr_getResult(void) {
    return frags;
}

Tr_exp Tr_nop() {
    return Tr_Nx(T_Exp(T_Const(0)));
}

Tr_exp Tr_simpleVar(Tr_access access, Tr_level level) {
    return Tr_Ex(simpleVarHelper(access, level, T_Temp(F_FP())));
}

Tr_exp Tr_fieldVar(Tr_exp base, int offset) {
    return Tr_Ex(T_Mem(T_Binop(T_plus, unEx(base), T_Const(offset * F_wordSize))));
}

Tr_exp Tr_subscriptVar(Tr_exp base, Tr_exp offset) {
    return Tr_Ex(T_Mem(T_Binop(T_plus, unEx(base), T_Binop(T_mul, unEx(offset), T_Const(F_wordSize)))));
}

Tr_exp Tr_nilExp() {
    return Tr_Ex(T_Const(0));
}

Tr_exp Tr_intExp(int value) {
    return Tr_Ex(T_Const(value));
}

Tr_exp Tr_stringExp(string value) {
    Temp_label label = Temp_newlabel();

    frags = F_FragList(F_string(label, value), frags);

    return Tr_Ex(T_Name(label));
}

Tr_exp Tr_callExp(Temp_label label, Tr_level caller, Tr_level callee, Tr_expList args) {
    return Tr_Ex(T_Call(
        T_Name(label),
        T_ExpList(
            callExpStaticLink(caller, callee, T_Temp(F_FP())),
            expListToExpList(args)
        )
    ));
}

Tr_exp Tr_intOpExp(A_oper oper, Tr_exp left, Tr_exp right) {
    switch (oper) {
    case A_plusOp:
        return Tr_Ex(T_Binop(T_plus, unEx(left), unEx(right)));
    case A_minusOp:
        return Tr_Ex(T_Binop(T_minus, unEx(left), unEx(right)));
    case A_timesOp:
        return Tr_Ex(T_Binop(T_mul, unEx(left), unEx(right)));
    case A_divideOp:
        return Tr_Ex(T_Binop(T_div, unEx(left), unEx(right)));
    case A_eqOp:
        return simpleCx(T_eq, left, right);
    case A_neqOp:
        return simpleCx(T_ne, left, right);
    case A_ltOp:
        return simpleCx(T_lt, left, right);
    case A_gtOp:
        return simpleCx(T_gt, left, right);
    case A_leOp:
        return simpleCx(T_le, left, right);
    case A_geOp:
        return simpleCx(T_ge, left, right);
    default:
        assert(0);
    }
}

Tr_exp Tr_stringOpExp(A_oper oper, Tr_exp left, Tr_exp right) {
    return Tr_Ex(F_externalCall(
        "stringCompare",
        T_ExpList(T_Const(oper), T_ExpList(unEx(left), T_ExpList(unEx(right), NULL)))
    ));
}

Tr_exp Tr_recordExp(Tr_expList exps) {
    int n = expListLen(exps);
    Temp_temp base = Temp_newtemp();

    return Tr_Ex(T_Eseq(
        T_Seq(
            T_Move(
                T_Temp(base),
                F_externalCall("malloc", T_ExpList(T_Const(n * F_wordSize), NULL))
            ),
            recordExpHelper(exps, 0, base)
        ),
        T_Temp(base)
    ));
}

Tr_exp Tr_seqExp(Tr_exp nx, Tr_exp ex) {
    return Tr_Ex(T_Eseq(unNx(nx), unEx(ex)));
}

Tr_exp Tr_assignExp(Tr_exp var, Tr_exp exp) {
    return Tr_Nx(T_Move(unEx(var), unEx(exp)));
}

Tr_exp Tr_ifExp(Tr_exp test, Tr_exp then, Tr_exp elsee) {
    Temp_label true = Temp_newlabel();
    Temp_label false = Temp_newlabel();
    Temp_label join = Temp_newlabel();
    Temp_temp temp = Temp_newtemp();
    struct Cx cx = unCx(test);

    doPatch(cx.trues, true);
    doPatch(cx.falses, false);

    return Tr_Ex(T_Eseq(
        T_Seq(cx.stm,
        T_Seq(T_Label(true),
        T_Seq(T_Move(T_Temp(temp), unEx(then)),
        T_Seq(T_Jump(T_Name(join), Temp_LabelList(join, NULL)),
        T_Seq(T_Label(false),
        T_Seq(T_Move(T_Temp(temp), elsee != NULL ? unEx(elsee) : T_Const(0)),
              T_Label(join))))))),
        T_Temp(temp)
    ));
}

Tr_exp Tr_whileExp(Tr_exp test, Tr_exp body, Temp_label done) {
    Temp_label start = Temp_newlabel();
    Temp_label exec = Temp_newlabel();
    struct Cx cx = unCx(test);

    doPatch(cx.trues, exec);
    doPatch(cx.falses, done);

    return Tr_Nx(
        T_Seq(T_Label(start),
        T_Seq(cx.stm,
        T_Seq(T_Label(exec),
        T_Seq(unNx(body),
        T_Seq(T_Jump(T_Name(start), Temp_LabelList(start, NULL)),
              T_Label(done))))))
    );
}

Tr_exp Tr_forExp(Tr_exp lo, Tr_exp hi, Tr_exp body, Temp_label done) {
    Temp_label start = Temp_newlabel();
    Temp_temp cnt = Temp_newtemp();
    Temp_temp limit = Temp_newtemp();

    return Tr_Nx(
        T_Seq(T_Move(T_Temp(cnt), unEx(lo)),
        T_Seq(T_Move(T_Temp(limit), unEx(hi)),
        T_Seq(T_Cjump(T_le, T_Temp(cnt), T_Temp(limit), start, done),
        T_Seq(T_Label(start),
        T_Seq(unNx(body),
        T_Seq(T_Move(T_Temp(cnt), T_Binop(T_plus, T_Temp(cnt), T_Const(1))),
        T_Seq(T_Cjump(T_lt, T_Temp(cnt), T_Temp(limit), start, done),
              T_Label(done))))))))
    );
}

Tr_exp Tr_breakExp(Temp_label done) {
    return Tr_Nx(T_Jump(T_Name(done), Temp_LabelList(done, NULL)));
}

Tr_exp Tr_letExp(Tr_expList initExps, Tr_exp body) {
    return Tr_Ex(T_Eseq(expListToSeq(initExps), unEx(body)));
}

Tr_exp Tr_arrayExp(Tr_exp size, Tr_exp init) {
    return Tr_Ex(F_externalCall("initArray", T_ExpList(unEx(size), T_ExpList(unEx(init), NULL))));
}

Tr_exp Tr_varDec(Tr_access access, Tr_exp init) {
    return Tr_Nx(T_Move(F_simpleVar(access->access, T_Temp(F_FP())), unEx(init)));
}

static struct Tr_level_ outermostLevel = {NULL, NULL};
static F_fragList frags = NULL;

static Tr_access Tr_Access(Tr_level level, F_access access) {
    Tr_access acc = (Tr_access) checked_malloc(sizeof(*acc));

    acc->level = level;
    acc->access = access;

    return acc;
}

static Tr_exp Tr_Ex(T_exp exp) {
    Tr_exp expr = (Tr_exp) checked_malloc(sizeof(*expr));

    expr->kind = Tr_ex;
    expr->u.ex = exp;

    return expr;
}

static Tr_exp Tr_Nx(T_stm stm) {
    Tr_exp exp = (Tr_exp) checked_malloc(sizeof(*exp));

    exp->kind = Tr_nx;
    exp->u.nx = stm;

    return exp;
}

static Tr_exp Tr_Cx(patchList trues, patchList falses, T_stm stm) {
    Tr_exp exp = (Tr_exp) checked_malloc(sizeof(*exp));

    exp->kind = Tr_cx;
    exp->u.cx.trues = trues;
    exp->u.cx.falses = falses;
    exp->u.cx.stm = stm;

    return exp;
}

static T_exp unEx(Tr_exp exp) {
    switch (exp->kind) {
    case Tr_ex: {
        return exp->u.ex;
    }
    case Tr_cx: {
        Temp_label true = Temp_newlabel();
        Temp_label false = Temp_newlabel();
        Temp_temp temp = Temp_newtemp();

        doPatch(exp->u.cx.trues, true);
        doPatch(exp->u.cx.falses, false);

        return T_Eseq(
            T_Seq(T_Move(T_Temp(temp), T_Const(1)),
            T_Seq(exp->u.cx.stm,
            T_Seq(T_Label(false),
            T_Seq(T_Move(T_Temp(temp), T_Const(0)),
                  T_Label(true))))),
            T_Temp(temp)
        );
    }
    case Tr_nx: {
        return T_Eseq(exp->u.nx, T_Const(0));
    }
    default: {
        assert(0);
    }
    }
}

static T_stm unNx(Tr_exp exp) {
    switch (exp->kind) {
    case Tr_ex:
        return T_Exp(exp->u.ex);
    case Tr_nx:
        return exp->u.nx;
    case Tr_cx:
        return T_Exp(unEx(exp));
    default:
        assert(0);
    }
}

static struct Cx unCx(Tr_exp exp) {
    switch (exp->kind) {
    case Tr_ex: {
        T_stm stm = T_Cjump(T_ne, exp->u.ex, T_Const(0), NULL, NULL);
        patchList trues = PatchList(&stm->u.CJUMP.true, NULL);
        patchList falses = PatchList(&stm->u.CJUMP.false, NULL);
        return (struct Cx) {trues, falses, stm};
    }
    case Tr_cx: {
        return exp->u.cx;
    }
    case Tr_nx: {
        assert(0);
    }
    default: {
        assert(0);
    }
    }
}

static patchList PatchList(Temp_label *head, patchList tail) {
	patchList list;

	list = (patchList) checked_malloc(sizeof(*list));
	list->head = head;
	list->tail = tail;
	
    return list;
}

static void doPatch(patchList tList, Temp_label label) {
	for(; tList; tList = tList->tail)
		*(tList->head) = label;
}

static patchList joinPatch(patchList first, patchList second) {
	if (!first)
        return second;
	
    for(; first->tail; first = first->tail);
	first->tail = second;
    return first;
}

static Tr_accessList accessListToAccessList(F_accessList accesses, Tr_level level) {
    if (accesses == NULL)
        return NULL;

    return Tr_AccessList(Tr_Access(level, accesses->head), accessListToAccessList(accesses->tail, level));
}

static T_exp simpleVarHelper(Tr_access access, Tr_level level, T_exp framePtr) {
    if (access->level == level)
        return F_simpleVar(access->access, framePtr);
    
    return simpleVarHelper(access, level->parent, F_staticLink(framePtr));
}

static T_expList expListToExpList(Tr_expList exps) {
    if (exps == NULL)
        return NULL;

    return T_ExpList(unEx(exps->head), expListToExpList(exps->tail));
}

static T_exp callExpStaticLink(Tr_level caller, Tr_level callee, T_exp framePtr) {
    if (callee->parent == caller)
        return framePtr;

    return callExpStaticLink(caller->parent, callee, F_staticLink(framePtr));
}

static Tr_exp simpleCx(T_relOp oper, Tr_exp left, Tr_exp right) {
    T_stm stm = T_Cjump(oper, unEx(left), unEx(right), NULL, NULL);
    patchList trues = PatchList(&stm->u.CJUMP.true, NULL);
    patchList falses = PatchList(&stm->u.CJUMP.false, NULL);
    return Tr_Cx(trues, falses, stm);
}

T_stm recordExpHelper(Tr_expList exps, int k, Temp_temp base) {
    T_stm moveStm = NULL;
    
    if (exps == NULL)
        return T_Exp(T_Const(0));
    
    moveStm = T_Move(
        T_Mem(T_Binop(
            T_plus,
            T_Temp(base),
            T_Const(k * F_wordSize)
        )),
        unEx(exps->head)
    );
    return exps->tail == NULL ? moveStm : T_Seq(moveStm, recordExpHelper(exps->tail, k + 1, base));
}

static int expListLen(Tr_expList exps) {
    return exps == NULL ? 0 : expListLen(exps->tail) + 1;
}

static T_stm expListToSeq(Tr_expList exps) {
    if (exps == NULL)
        return T_Exp(T_Const(0));
    
    if (exps->tail == NULL)
        return unNx(exps->head);
    
    return T_Seq(unNx(exps->head), expListToSeq(exps->tail));
}
