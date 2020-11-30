
/*Lab5: This header file is not complete. Please finish it with more definition.*/

#ifndef FRAME_H
#define FRAME_H

#include "tree.h"

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

Temp_temp F_FP(void);
F_frame F_newFrame(Temp_label, U_boolList);
F_accessList F_formals(F_frame);
F_access F_allocLocal(F_frame, bool);
T_stm F_procEntryExit1(F_frame, T_stm);
T_exp F_simpleVar(F_access, T_exp);
T_exp F_staticLink(T_exp);
F_frag F_string(Temp_label, string);
Temp_label F_name(F_frame);
T_exp F_externalCall(string, T_expList);

#endif
