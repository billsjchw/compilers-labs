#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "errormsg.h"
#include "tree.h"
#include "assem.h"
#include "frame.h"
#include "codegen.h"
#include "table.h"

static AS_instrList insts;
static AS_instrList last;
static string assemFP;

static Temp_temp munchExp(T_exp);
static void munchStm(T_stm);
static Temp_tempList munchExpList(T_expList);
static Temp_tempList prepareArgs(Temp_tempList, Temp_tempList);
static int expListLen(T_expList);
static void emit(AS_instr);

//Lab 6: put your code here
AS_instrList F_codegen(F_frame frame, T_stmList stms) {
    string name = Temp_labelstring(F_name(frame));
    
    assemFP = (string) checked_malloc(strlen(name) + 30);
    sprintf(assemFP, "LEAQ %s_frameSize(`s0), `d0", name);

    insts = last = NULL;
    for (; stms != NULL; stms = stms->tail)
        munchStm(stms->head);

    return F_procEntryExit2(insts);
}

static AS_instrList insts = NULL;
static AS_instrList last = NULL;

static Temp_temp munchExp(T_exp exp) {
    Temp_temp result = Temp_newtemp();
    switch (exp->kind) {
    case T_BINOP: {
        Temp_temp left = munchExp(exp->u.BINOP.left);
        Temp_temp right = munchExp(exp->u.BINOP.right);
        if (exp->u.BINOP.op == T_div) {
            emit(AS_Move("MOVQ `s0, `d0", Temp_TempList(F_RAX(), NULL), Temp_TempList(left, NULL)));
            emit(AS_Oper("CQTO", Temp_TempList(F_RAX(), Temp_TempList(F_RDX(), NULL)),
                         Temp_TempList(F_RAX(), NULL), NULL));
            emit(AS_Oper("IDIVQ `s0", Temp_TempList(F_RAX(), Temp_TempList(F_RDX(), NULL)),
                         Temp_TempList(right, Temp_TempList(F_RAX(), Temp_TempList(F_RDX(), NULL))), NULL));
            emit(AS_Move("MOVQ `s0, `d0", Temp_TempList(result, NULL), Temp_TempList(F_RAX(), NULL)));
        } else {
            string assem = NULL;
            emit(AS_Move("MOVQ `s0, `d0", Temp_TempList(result, NULL), Temp_TempList(left, NULL)));
            switch (exp->u.BINOP.op) {
            case T_plus: assem = "ADDQ `s0, `d0"; break;
            case T_minus: assem = "SUBQ `s0, `d0"; break;
            case T_mul: assem = "IMULQ `s0, `d0"; break;
            default: assert(0);
            }
            emit(AS_Oper(assem, Temp_TempList(result, NULL), Temp_TempList(right, NULL), NULL));
        }
        break;
    }
    case T_MEM: {
        Temp_temp addr = munchExp(exp->u.MEM);
        emit(AS_Oper("MOVQ (`s0), `d0", Temp_TempList(result, NULL), Temp_TempList(addr, NULL), NULL));
        break;
    }
    case T_TEMP: {
        if (exp->u.TEMP == F_FP())
            emit(AS_Oper(assemFP, Temp_TempList(result, NULL), Temp_TempList(F_RSP(), NULL), NULL));
        else
            emit(AS_Move("MOVQ `s0, `d0", Temp_TempList(result, NULL), Temp_TempList(exp->u.TEMP, NULL)));
        break;
    }
    case T_ESEQ: {
        assert(0);
    }
    case T_NAME: {
        string labstr = Temp_labelstring(exp->u.NAME);
        string assem = (string) checked_malloc(strlen(labstr) + 20);
        sprintf(assem, "MOVQ $%s, `d0", labstr);
        emit(AS_Oper(assem, Temp_TempList(result, NULL), NULL, NULL));
        break;
    }
    case T_CONST: {
        string assem = (string) checked_malloc(30);
        sprintf(assem, "MOVQ $%d, `d0", exp->u.CONST);
        emit(AS_Oper(assem, Temp_TempList(result, NULL), NULL, NULL));
        break;
    }
    case T_CALL: {
        Temp_tempList args = munchExpList(exp->u.CALL.args);
        Temp_tempList regs = prepareArgs(args, F_argregs());
        int argsNum = expListLen(exp->u.CALL.args);
        if (exp->u.CALL.fun->kind == T_NAME) {
            string labstr = Temp_labelstring(exp->u.CALL.fun->u.NAME);
            string assem = (string) checked_malloc(strlen(labstr) + 10);
            sprintf(assem, "CALL %s", labstr);
            emit(AS_Oper(assem, F_callersaves(), Temp_TempList(F_RSP(), NULL), NULL));
        } else {
            Temp_temp addr = munchExp(exp->u.CALL.fun);
            emit(AS_Oper("CALL *`s0", F_callersaves(), Temp_TempList(addr, Temp_TempList(F_RSP(), NULL)), NULL));
        }
        emit(AS_Move("MOVQ `s0, `d0", Temp_TempList(result, NULL), Temp_TempList(F_RAX(), NULL)));
        if (argsNum > F_argregsNum) {
            string assem = (string) checked_malloc(30);
            sprintf(assem, "ADDQ $%d, `d0", F_wordSize * (argsNum - F_argregsNum));
            emit(AS_Oper(assem, Temp_TempList(F_RSP(), NULL), NULL, NULL));
        }
        break;
    }
    default: {
        assert(0);
    }
    }
    return result;
}

static void munchStm(T_stm stm) {
    switch (stm->kind) {
    case T_SEQ: {
        assert(0);
    }
    case T_LABEL: {
        string labstr = Temp_labelstring(stm->u.LABEL);
        emit(AS_Label(labstr, stm->u.LABEL));
        break;
    }
    case T_JUMP: {
        if (stm->u.JUMP.exp->kind == T_NAME) {
            string labstr = Temp_labelstring(stm->u.JUMP.exp->u.NAME);
            string assem = (string) checked_malloc(strlen(labstr) + 10);
            sprintf(assem, "JMP %s", labstr);
            emit(AS_Oper(assem, NULL, NULL, AS_Targets(stm->u.JUMP.jumps)));
        } else {
            Temp_temp temp = munchExp(stm->u.JUMP.exp);
            emit(AS_Oper("JMP *`s0", NULL, Temp_TempList(temp, NULL), AS_Targets(stm->u.JUMP.jumps)));
        }
        break;
    }
    case T_CJUMP: {
        Temp_temp left = munchExp(stm->u.CJUMP.left);
        Temp_temp right = munchExp(stm->u.CJUMP.right);
        string labstr = Temp_labelstring(stm->u.CJUMP.true);
        string jmpInstStr = NULL;
        string assem = NULL;
        emit(AS_Oper("CMP `s0, `s1", NULL, Temp_TempList(right, Temp_TempList(left, NULL)), NULL));
        switch (stm->u.CJUMP.op) {
        case T_lt: jmpInstStr = "JL"; break;
        case T_gt: jmpInstStr = "JG"; break;
        case T_le: jmpInstStr = "JLE"; break;
        case T_ge: jmpInstStr = "JGE"; break;
        case T_eq: jmpInstStr = "JE"; break;
        case T_ne: jmpInstStr = "JNE"; break;
        default: assert(0);
        }
        assem = (string) checked_malloc(strlen(jmpInstStr) + strlen(labstr) + 10);
        sprintf(assem, "%s %s", jmpInstStr, labstr);
        emit(AS_Oper(assem, NULL, NULL,
                     AS_Targets(Temp_LabelList(stm->u.CJUMP.true, Temp_LabelList(stm->u.CJUMP.false, NULL)))));
        break;
    }
    case T_MOVE: {
        if (stm->u.MOVE.src->kind == T_TEMP && stm->u.MOVE.dst->kind == T_TEMP) {
            emit(AS_Move("MOVQ `s0, `d0", Temp_TempList(stm->u.MOVE.dst->u.TEMP, NULL),
                         Temp_TempList(stm->u.MOVE.src->u.TEMP, NULL)));
        } else {
            Temp_temp src = munchExp(stm->u.MOVE.src);
            if (stm->u.MOVE.dst->kind == T_MEM) {
                Temp_temp addr = munchExp(stm->u.MOVE.dst->u.MEM);
                emit(AS_Oper("MOVQ `s0, (`s1)", NULL, Temp_TempList(src, Temp_TempList(addr, NULL)), NULL));
            } else {
                emit(AS_Move("MOVQ `s0, `d0", Temp_TempList(stm->u.MOVE.dst->u.TEMP, NULL), Temp_TempList(src, NULL)));
            }
        }
        break;
    }
    case T_EXP: {
        munchExp(stm->u.EXP);
        break;
    }
    default: {
        assert(0);
    }
    }
}

static Temp_tempList munchExpList(T_expList exps) {
    Temp_temp temp = NULL;
    
    if (exps == NULL)
        return NULL;

    temp = munchExp(exps->head);
    return Temp_TempList(temp, munchExpList(exps->tail));
}

static void emit(AS_instr inst) {
    if (insts == NULL)
        insts = last = AS_InstrList(inst, NULL);
    else
        last = last->tail = AS_InstrList(inst, NULL);
}

static Temp_tempList prepareArgs(Temp_tempList args, Temp_tempList regs) {
    Temp_tempList temps = NULL;
    
    if (args == NULL)
        return NULL;

    temps = prepareArgs(args->tail, regs == NULL ? NULL : regs->tail);
    if (regs == NULL) {
        emit(AS_Oper("PUSHQ `s0", Temp_TempList(F_RSP(), NULL),
                     Temp_TempList(args->head, Temp_TempList(F_RSP(), NULL)), NULL));
        return temps;
    } else {
        emit(AS_Move("MOVQ `s0, `d0", Temp_TempList(regs->head, NULL), Temp_TempList(args->head, NULL)));
        return Temp_TempList(regs->head, temps);
    }
}

static int expListLen(T_expList exps) {
    return exps == NULL ? 0 : 1 + expListLen(exps->tail);
}
