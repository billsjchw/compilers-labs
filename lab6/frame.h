
/*Lab5: This header file is not complete. Please finish it with more definition.*/

#ifndef FRAME_H
#define FRAME_H

#include "tree.h"
#include "assem.h"

typedef struct F_frame_ *F_frame;

typedef struct F_access_ *F_access;
typedef struct F_accessList_ *F_accessList;

struct F_accessList_ {F_access head; F_accessList tail;};


/* declaration for fragments */
typedef struct F_frag_ *F_frag;
struct F_frag_ {enum {F_stringFrag, F_procFrag} kind;
			union {
				struct {Temp_label label; string str;} stringg;
				struct {T_stm body; F_frame frame;} proc;
			} u;
};

F_frag F_StringFrag(Temp_label label, string str);
F_frag F_ProcFrag(T_stm body, F_frame frame);

typedef struct F_fragList_ *F_fragList;
struct F_fragList_ 
{
	F_frag head; 
	F_fragList tail;
};

F_fragList F_FragList(F_frag, F_fragList);

extern const int F_wordSize;
extern const int F_argregsNum;

Temp_temp F_FP(void);
Temp_temp F_RDI(void);
Temp_temp F_RSI(void);
Temp_temp F_RDX(void);
Temp_temp F_RCX(void);
Temp_temp F_RBX(void);
Temp_temp F_RBP(void);
Temp_temp F_RSP(void);
Temp_temp F_RAX(void);
Temp_temp F_R8(void);
Temp_temp F_R9(void);
Temp_temp F_R10(void);
Temp_temp F_R11(void);
Temp_temp F_R12(void);
Temp_temp F_R13(void);
Temp_temp F_R14(void);
Temp_temp F_R15(void);
Temp_map F_tempMap(void);
Temp_tempList F_registers(void);
Temp_tempList F_callersaves(void);
Temp_tempList F_calleesaves(void);
Temp_tempList F_argregs(void);
Temp_tempList F_returnsinks(void);
F_frame F_newFrame(Temp_label, U_boolList);
F_accessList F_formals(F_frame);
F_access F_allocLocal(F_frame, bool);
T_stm F_procEntryExit1(F_frame, T_stm);
T_exp F_simpleVar(F_access, T_exp);
T_exp F_staticLink(T_exp);
F_frag F_string(Temp_label, string);
Temp_label F_name(F_frame);
T_exp F_externalCall(string, T_expList);
AS_instrList F_procEntryExit2(AS_instrList);
AS_proc F_procEntryExit3(F_frame, AS_instrList);
AS_instr F_load(F_access, Temp_temp, F_frame);
AS_instr F_store(F_access, Temp_temp, F_frame);

#endif
