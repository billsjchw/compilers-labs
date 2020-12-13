#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "table.h"
#include "tree.h"
#include "frame.h"
#include "assem.h"

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
    int size;
    Temp_label name;
    F_accessList formals;
    T_stm shiftOfView;
};

static Temp_temp fp;
static Temp_temp rdi;
static Temp_temp rsi;
static Temp_temp rdx;
static Temp_temp rcx;
static Temp_temp rbx;
static Temp_temp rbp;
static Temp_temp rsp;
static Temp_temp rax;
static Temp_temp r8;
static Temp_temp r9;
static Temp_temp r10;
static Temp_temp r11;
static Temp_temp r12;
static Temp_temp r13;
static Temp_temp r14;
static Temp_temp r15;
static Temp_map tempMap;
static Temp_tempList registers;
static Temp_tempList callersaves;
static Temp_tempList calleesaves;
static Temp_tempList argregs;
static Temp_tempList returnsinks;

static F_access F_InFrameAccess(int);
static F_access F_InRegAccess(Temp_temp);
static F_accessList F_AccessList(F_access, F_accessList);
static F_accessList formalAccessList(int, U_boolList, F_frame);
static T_stm shiftOfView(F_accessList, Temp_tempList);
static T_stm saveCalleesaves(Temp_tempList, Temp_tempList);
static T_stm restoreCalleesaves(Temp_tempList, Temp_tempList);

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
const int F_argregsNum = 6;

Temp_temp F_FP(void) {
    if (fp == NULL)
        fp = Temp_newtemp();

    return fp;
}

Temp_temp F_RDI(void) {
    if (rdi == NULL)
        rdi = Temp_newtemp();

    return rdi;
}

Temp_temp F_RSI(void) {
    if (rsi == NULL)
        rsi = Temp_newtemp();

    return rsi;
}

Temp_temp F_RDX(void) {
    if (rdx == NULL)
        rdx = Temp_newtemp();

    return rdx;
}

Temp_temp F_RCX(void) {
    if (rcx == NULL)
        rcx = Temp_newtemp();

    return rcx;
}

Temp_temp F_RBX(void) {
    if (rbx == NULL)
        rbx = Temp_newtemp();

    return rbx;
}

Temp_temp F_RBP(void) {
    if (rbp == NULL)
        rbp = Temp_newtemp();

    return rbp;
}

Temp_temp F_RSP(void) {
    if (rsp == NULL)
        rsp = Temp_newtemp();

    return rsp;
}

Temp_temp F_RAX(void) {
    if (rax == NULL)
        rax = Temp_newtemp();

    return rax;
}

Temp_temp F_R8(void) {
    if (r8 == NULL)
        r8 = Temp_newtemp();

    return r8;
}

Temp_temp F_R9(void) {
    if (r9 == NULL)
        r9 = Temp_newtemp();

    return r9;
}

Temp_temp F_R10(void) {
    if (r10 == NULL)
        r10 = Temp_newtemp();

    return r10;
}

Temp_temp F_R11(void) {
    if (r11 == NULL)
        r11 = Temp_newtemp();

    return r11;
}

Temp_temp F_R12(void) {
    if (r12 == NULL)
        r12 = Temp_newtemp();

    return r12;
}

Temp_temp F_R13(void) {
    if (r13 == NULL)
        r13 = Temp_newtemp();

    return r13;
}

Temp_temp F_R14(void) {
    if (r14 == NULL)
        r14 = Temp_newtemp();

    return r14;
}

Temp_temp F_R15(void) {
    if (r15 == NULL)
        r15 = Temp_newtemp();

    return r15;
}

Temp_map F_tempMap(void) {
    if (tempMap == NULL) {
        tempMap = Temp_empty();
        Temp_enter(tempMap, F_RDI(), "%rdi");
        Temp_enter(tempMap, F_RSI(), "%rsi");
        Temp_enter(tempMap, F_RDX(), "%rdx");
        Temp_enter(tempMap, F_RCX(), "%rcx");
        Temp_enter(tempMap, F_RBX(), "%rbx");
        Temp_enter(tempMap, F_RBP(), "%rbp");
        Temp_enter(tempMap, F_RSP(), "%rsp");
        Temp_enter(tempMap, F_RAX(), "%rax");
        Temp_enter(tempMap, F_R8(), "%r8");
        Temp_enter(tempMap, F_R9(), "%r9");
        Temp_enter(tempMap, F_R10(), "%r10");
        Temp_enter(tempMap, F_R11(), "%r11");
        Temp_enter(tempMap, F_R12(), "%r12");
        Temp_enter(tempMap, F_R13(), "%r13");
        Temp_enter(tempMap, F_R14(), "%r14");
        Temp_enter(tempMap, F_R15(), "%r15");
    }
    return tempMap;
}

Temp_tempList F_registers(void) {
    if (registers == NULL)
        registers = Temp_TempList(F_RDI(),
                    Temp_TempList(F_RSI(),
                    Temp_TempList(F_RDX(),
                    Temp_TempList(F_RCX(),
                    Temp_TempList(F_RBX(),
                    Temp_TempList(F_RBP(),
                    Temp_TempList(F_RSP(),
                    Temp_TempList(F_RAX(),
                    Temp_TempList(F_R8(),
                    Temp_TempList(F_R9(),
                    Temp_TempList(F_R10(),
                    Temp_TempList(F_R11(),
                    Temp_TempList(F_R12(),
                    Temp_TempList(F_R13(),
                    Temp_TempList(F_R14(),
                    Temp_TempList(F_R15(), NULL))))))))))))))));
    return registers;
}

Temp_tempList F_callersaves(void) {
    if (callersaves == NULL)
        callersaves = Temp_TempList(F_RAX(),
                      Temp_TempList(F_RCX(),
                      Temp_TempList(F_RDX(),
                      Temp_TempList(F_RDI(),
                      Temp_TempList(F_RSI(),
                      Temp_TempList(F_RSP(),
                      Temp_TempList(F_R8(),
                      Temp_TempList(F_R9(),
                      Temp_TempList(F_R10(),
                      Temp_TempList(F_R11(), NULL))))))))));
    return callersaves;
}

Temp_tempList F_calleesaves(void) {
    if (calleesaves == NULL)
        calleesaves = Temp_TempList(F_RBX(),
                      Temp_TempList(F_RBP(),
                      Temp_TempList(F_R12(),
                      Temp_TempList(F_R13(),
                      Temp_TempList(F_R14(),
                      Temp_TempList(F_R15(), NULL))))));
    return calleesaves;
}

Temp_tempList F_argregs(void) {
    if (argregs == NULL)
        argregs = Temp_TempList(F_RDI(),
                  Temp_TempList(F_RSI(),
                  Temp_TempList(F_RDX(),
                  Temp_TempList(F_RCX(),
                  Temp_TempList(F_R8(),
                  Temp_TempList(F_R9(), NULL))))));
    return argregs;
}

Temp_tempList F_returnsinks(void) {
    if (returnsinks == NULL)
        returnsinks = Temp_TempList(F_RAX(), Temp_TempList(F_RSP(), F_calleesaves()));
    return returnsinks;
}

F_frame F_newFrame(Temp_label name, U_boolList escapes) {
    F_frame frame = (F_frame) checked_malloc(sizeof(*frame));

    frame->size = 0;
    frame->name = name;
    frame->formals = formalAccessList(0, escapes, frame);
}

F_accessList F_formals(F_frame frame) {
    return frame->formals;
}

F_access F_allocLocal(F_frame frame, bool escape) {
    return escape ? F_InFrameAccess(-(++frame->size)) : F_InRegAccess(Temp_newtemp());
}

T_stm F_procEntryExit1(F_frame frame, T_stm body) {
    Temp_tempList temps = Temp_TempList(Temp_newtemp(),
                          Temp_TempList(Temp_newtemp(),
                          Temp_TempList(Temp_newtemp(),
                          Temp_TempList(Temp_newtemp(),
                          Temp_TempList(Temp_newtemp(),
                          Temp_TempList(Temp_newtemp(), NULL))))));
    
    return
        T_Seq(shiftOfView(frame->formals, F_argregs()),
        T_Seq(saveCalleesaves(F_calleesaves(), temps),
        T_Seq(body,
              restoreCalleesaves(F_calleesaves(), temps))));
}

T_exp F_simpleVar(F_access access, T_exp framePtr) {
    if (access->kind == inReg)
        return T_Temp(access->u.reg);
    else
        return T_Mem(T_Binop(T_plus, framePtr, T_Const(access->u.offset * F_wordSize)));
}

T_exp F_staticLink(T_exp framePtr) {
    return T_Mem(T_Binop(T_minus, framePtr, T_Const(F_wordSize)));
}

F_frag F_string(Temp_label label, string str) {
    return F_StringFrag(label, str);
}

Temp_label F_name(F_frame frame) {
    return frame->name;
}

T_exp F_externalCall(string name, T_expList args) {
    return T_Call(T_Name(Temp_namedlabel(name)), args);
}

AS_instrList F_procEntryExit2(AS_instrList body) {
    return AS_splice(body, AS_InstrList(AS_Oper("", NULL, F_returnsinks(), NULL), NULL));
}

AS_proc F_procEntryExit3(F_frame frame, AS_instrList body) {
    string labstr = Temp_labelstring(frame->name);
    string prolog = (string) checked_malloc(5 * strlen(labstr) + 120);
    string epilog = (string) checked_malloc(strlen(labstr) + 40);
    string prologFormat = ".text\n"
                          ".globl %s\n"
                          ".type %s, @function\n"
                          ".set %s_frameSize, %d\n"
                          "%s:\n"
                          "subq $%s_frameSize, %%rsp\n";
    string epilogFormat = "addq $%s_frameSize, %%rsp\n"
                          "ret\n";

    sprintf(prolog, prologFormat, labstr, labstr, labstr, frame->size * F_wordSize, labstr, labstr);
    sprintf(epilog, epilogFormat, labstr);

    return AS_Proc(prolog, body, epilog);
}

AS_instr F_load(F_access access, Temp_temp temp, F_frame frame) {
    string labstr = Temp_labelstring(frame->name);
    string assem = (string) checked_malloc(strlen(labstr) + 50);
    
    assert(access->kind == inFrame);

    sprintf(assem, "movq %s_frameSize+(%d)(`s0), `d0", labstr, access->u.offset * F_wordSize);

    return AS_Oper(assem, Temp_TempList(temp, NULL), Temp_TempList(F_RSP(), NULL), NULL);
}

AS_instr F_store(F_access access, Temp_temp temp, F_frame frame) {
    string labstr = Temp_labelstring(frame->name);
    string assem = (string) checked_malloc(strlen(labstr) + 50);
    
    assert(access->kind == inFrame);

    sprintf(assem, "movq `s0, %s_frameSize+(%d)(`s1)", labstr, access->u.offset * F_wordSize);

    return AS_Oper(assem, NULL, Temp_TempList(temp, Temp_TempList(F_RSP(), NULL)), NULL);
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
static Temp_map tempMap = NULL;
static Temp_tempList registers = NULL;
static Temp_tempList callersaves = NULL;
static Temp_tempList calleesaves = NULL;
static Temp_tempList argregs = NULL;

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

static F_accessList formalAccessList(int k, U_boolList escapes, F_frame frame) {
    F_access access = NULL;
    
    if (escapes == NULL)
        return NULL;

    access = k < F_argregsNum ? F_allocLocal(frame, escapes->head) : F_InFrameAccess(k - F_argregsNum + 1);

    return F_AccessList(access, formalAccessList(k + 1, escapes->tail, frame));
}

static T_stm shiftOfView(F_accessList formals, Temp_tempList regs) {
    if (formals == NULL || regs == NULL)
        return T_Exp(T_Const(0));

    return T_Seq(
        T_Move(
            F_simpleVar(formals->head, T_Temp(F_FP())),
            T_Temp(regs->head)
        ),
        shiftOfView(formals->tail, regs->tail)
    );
}

static T_stm saveCalleesaves(Temp_tempList regs, Temp_tempList temps) {
    if (regs == NULL)
        return T_Exp(T_Const(0));

    return T_Seq(
        T_Move(
            T_Temp(temps->head),
            T_Temp(regs->head)
        ),
        saveCalleesaves(regs->tail, temps->tail)
    );
}

static T_stm restoreCalleesaves(Temp_tempList regs, Temp_tempList temps) {
    if (regs == NULL)
        return T_Exp(T_Const(0));

    return T_Seq(
        T_Move(
            T_Temp(regs->head),
            T_Temp(temps->head)
        ),
        restoreCalleesaves(regs->tail, temps->tail)
    );
}
