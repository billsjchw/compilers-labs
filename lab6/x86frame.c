#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "table.h"
#include "tree.h"
#include "frame.h"

#define FORMAL_REG_NUM 6

/*Lab5: Your implementation here.*/

//varibales
struct F_access_ {
	enum {inFrame, inReg} kind;
	union {
		int offset; //inFrame
		Temp_temp reg; //inReg
	} u;
};

struct F_frame_ {
    int inFrameCnt;
    Temp_label label;
    F_accessList formals;
    T_stm shiftOfView;
};

static Temp_temp fp;
static Temp_temp rdi;
static Temp_temp rsi;
static Temp_temp rdx;
static Temp_temp rcx;
static Temp_temp r8;
static Temp_temp r9;
static Temp_temp rbx;
static Temp_temp rbp;
static Temp_temp r12;
static Temp_temp r13;
static Temp_temp r14;
static Temp_temp r15;


static Temp_temp F_RDI();
static Temp_temp F_RSI();
static Temp_temp F_RDX();
static Temp_temp F_RCX();
static Temp_temp F_R8();
static Temp_temp F_R9();
static Temp_temp F_RBX();
static Temp_temp F_RBP();
static Temp_temp F_R12();
static Temp_temp F_R13();
static Temp_temp F_R14();
static Temp_temp F_R15();
static F_access F_InFrameAccess(int);
static F_access F_InRegAccess(Temp_temp);
static F_accessList F_AccessList(F_access, F_accessList);
static F_accessList formalAccessList(int, int, U_boolList);

F_frag F_StringFrag(Temp_label label, string str) {
    F_frag frag = (F_frag) checked_malloc(sizeof(*frag));

    frag->kind = F_stringFrag;
    frag->u.stringg.str = str;
    frag->u.stringg.label = label;

    return frag;
}                                                     
                                                      
F_frag F_ProcFrag(T_stm body, F_frame frame) {
    F_frag frag = (F_frag) checked_malloc(sizeof(*frag));

    frag->kind = F_procFrag;
    frag->u.proc.body = body;
    frag->u.proc.frame = frame;

    return frag;                           
}                                                     
                                                      
F_fragList F_FragList(F_frag head, F_fragList tail) {
    F_fragList frags = (F_fragList) checked_malloc(sizeof(*frags));

    frags->head = head;
    frags->tail = tail;

    return frags;                  
}                                                     

const int F_wordSize = 8;

Temp_temp F_FP(void) {
    if (fp == NULL)
        fp = Temp_newtemp();

    return fp;
}

F_frame F_newFrame(Temp_label label, U_boolList escapes) {
    Temp_temp formalRegs[] = {F_RDI(), F_RSI(), F_RDX(), F_RCX(), F_R8(), F_R9()};
    T_stm shiftOfView = T_Exp(T_Const(0));
    F_accessList formals = formalAccessList(0, 0, escapes);
    int inRegCnt = 0;
    F_frame frame = (F_frame) checked_malloc(sizeof(*frame));

    frame->inFrameCnt = 0;
    frame->label = label;
    frame->formals = formals;

    for (; escapes != NULL && inRegCnt < FORMAL_REG_NUM; escapes = escapes->tail, formals = formals->tail)
        if (!escapes->head) {
            shiftOfView = T_Seq(shiftOfView, T_Move(T_Temp(formals->head->u.reg), T_Temp(formalRegs[inRegCnt])));
            ++inRegCnt;
        }
    frame->shiftOfView = shiftOfView;

    return frame;
}

F_accessList F_formals(F_frame frame) {
    return frame->formals;
}

F_access F_allocLocal(F_frame frame, bool escape) {
    return escape ? F_InFrameAccess(-(++frame->inFrameCnt)) : F_InRegAccess(Temp_newtemp());
}

T_stm F_procEntryExit1(F_frame frame, T_stm body) {
    Temp_temp calleeSavedRegs[] = {F_RBX(), F_RBP(), F_R12(), F_R13(), F_R14(), F_R15()};
    Temp_temp tmps[] = {Temp_newtemp(), Temp_newtemp(), Temp_newtemp(), Temp_newtemp(), Temp_newtemp(), Temp_newtemp()};
    
    return
        T_Seq(frame->shiftOfView,
        T_Seq(T_Move(T_Temp(tmps[0]), T_Temp(calleeSavedRegs[0])),
        T_Seq(T_Move(T_Temp(tmps[1]), T_Temp(calleeSavedRegs[1])),
        T_Seq(T_Move(T_Temp(tmps[2]), T_Temp(calleeSavedRegs[2])),
        T_Seq(T_Move(T_Temp(tmps[3]), T_Temp(calleeSavedRegs[3])),
        T_Seq(T_Move(T_Temp(tmps[4]), T_Temp(calleeSavedRegs[4])),
        T_Seq(T_Move(T_Temp(tmps[5]), T_Temp(calleeSavedRegs[5])),
        T_Seq(body,
        T_Seq(T_Move(T_Temp(calleeSavedRegs[0]), T_Temp(tmps[0])),
        T_Seq(T_Move(T_Temp(calleeSavedRegs[1]), T_Temp(tmps[1])),
        T_Seq(T_Move(T_Temp(calleeSavedRegs[2]), T_Temp(tmps[2])),
        T_Seq(T_Move(T_Temp(calleeSavedRegs[3]), T_Temp(tmps[3])),
        T_Seq(T_Move(T_Temp(calleeSavedRegs[4]), T_Temp(tmps[4])),
              T_Move(T_Temp(calleeSavedRegs[5]), T_Temp(tmps[5])))))))))))))));
}

T_exp F_simpleVar(F_access access, T_exp framePtr) {
    if (access->kind == inReg)
        return T_Temp(access->u.reg);
    else
        return T_Mem(T_Binop(T_plus, framePtr, T_Const(access->u.offset * F_wordSize)));
}

T_exp F_staticLink(T_exp framePtr) {
    return T_Mem(T_Binop(T_plus, framePtr, T_Const(F_wordSize)));
}

F_frag F_string(Temp_label label, string str) {
    return F_StringFrag(label, str);
}

Temp_label F_name(F_frame frame) {
    return frame->label;
}

T_exp F_externalCall(string name, T_expList args) {
    return T_Call(T_Name(Temp_namedlabel(name)), args);
}

static Temp_temp fp = NULL;
static Temp_temp rdi = NULL;
static Temp_temp rsi = NULL;
static Temp_temp rdx = NULL;
static Temp_temp rcx = NULL;
static Temp_temp r8 = NULL;
static Temp_temp r9 = NULL;
static Temp_temp rbx = NULL;
static Temp_temp rbp = NULL;
static Temp_temp r12 = NULL;
static Temp_temp r13 = NULL;
static Temp_temp r14 = NULL;
static Temp_temp r15 = NULL;

static Temp_temp F_RDI(void) {
    if (rdi == NULL)
        rdi = Temp_newtemp();

    return rdi;
}

static Temp_temp F_RSI(void) {
    if (rsi == NULL)
        rsi = Temp_newtemp();

    return rsi;
}

static Temp_temp F_RDX(void) {
    if (rdx == NULL)
        rdx = Temp_newtemp();

    return rdx;
}

static Temp_temp F_RCX(void) {
    if (rcx == NULL)
        rcx = Temp_newtemp();

    return rcx;
}

static Temp_temp F_R8(void) {
    if (r8 == NULL)
        r8 = Temp_newtemp();

    return r8;
}

static Temp_temp F_R9(void) {
    if (r9 == NULL)
        r9 = Temp_newtemp();

    return r9;
}

static Temp_temp F_RBX(void) {
    if (rbx == NULL)
        rbx = Temp_newtemp();

    return rbx;
}

static Temp_temp F_RBP(void) {
    if (rbp == NULL)
        rbp = Temp_newtemp();

    return rbp;
}

static Temp_temp F_R12(void) {
    if (r12 == NULL)
        r12 = Temp_newtemp();

    return r12;
}

static Temp_temp F_R13(void) {
    if (r13 == NULL)
        r13 = Temp_newtemp();

    return r13;
}

static Temp_temp F_R14(void) {
    if (r14 == NULL)
        r14 = Temp_newtemp();

    return r14;
}

static Temp_temp F_R15(void) {
    if (r15 == NULL)
        r15 = Temp_newtemp();

    return r15;
}

static F_access F_InFrameAccess(int offset) {
    F_access access = (F_access) checked_malloc(sizeof(*access));

    access->kind = inFrame;
    access->u.offset = offset;

    return access;
}

static F_access F_InRegAccess(Temp_temp reg) {
    F_access access = (F_access) checked_malloc(sizeof(*access));

    access->kind = inReg;
    access->u.reg = reg;

    return access;
}

static F_accessList F_AccessList(F_access head, F_accessList tail) {
    F_accessList accesses = (F_accessList) checked_malloc(sizeof(*accesses));

    accesses->head = head;
    accesses->tail = tail;

    return accesses;
}

static F_accessList formalAccessList(int inFrameCnt, int inRegCnt, U_boolList escapes) {
    if (escapes == NULL)
        return NULL;
    
    if (escapes->head || inRegCnt == FORMAL_REG_NUM)
        return F_AccessList(F_InFrameAccess(inFrameCnt + 2), formalAccessList(inFrameCnt + 1, inRegCnt, escapes->tail));
    else
        return F_AccessList(F_InRegAccess(Temp_newtemp()), formalAccessList(inFrameCnt, inRegCnt + 1, escapes->tail));
}
