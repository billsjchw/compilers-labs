#ifndef TRANSLATE_H
#define TRANSLATE_H

#include "util.h"
#include "absyn.h"
#include "temp.h"
#include "frame.h"

/* Lab5: your code below */

typedef struct Tr_exp_ *Tr_exp;
typedef struct Tr_expList_ *Tr_expList;
typedef struct Tr_access_ *Tr_access;
typedef struct Tr_accessList_ *Tr_accessList;
typedef struct Tr_level_ *Tr_level;

struct Tr_accessList_ {
	Tr_access head;
	Tr_accessList tail;	
};

Tr_expList Tr_ExpList(Tr_exp, Tr_expList);
Tr_accessList Tr_AccessList(Tr_access, Tr_accessList);
Tr_level Tr_outermost(void);
Tr_level Tr_newLevel(Tr_level, Temp_label, U_boolList);
Tr_accessList Tr_formals(Tr_level);
Tr_access Tr_allocLocal(Tr_level, bool);
void Tr_procEntryExit(Tr_level, Tr_exp);
F_fragList Tr_getResult(void);
Tr_exp Tr_nop();
Tr_exp Tr_simpleVar(Tr_access, Tr_level);
Tr_exp Tr_fieldVar(Tr_exp, int);
Tr_exp Tr_subscriptVar(Tr_exp, Tr_exp);
Tr_exp Tr_nilExp();
Tr_exp Tr_intExp(int);
Tr_exp Tr_stringExp(string);
Tr_exp Tr_callExp(Temp_label, Tr_level, Tr_level, Tr_expList);
Tr_exp Tr_intOpExp(A_oper, Tr_exp, Tr_exp);
Tr_exp Tr_stringOpExp(A_oper, Tr_exp, Tr_exp);
Tr_exp Tr_recordExp(Tr_expList);
Tr_exp Tr_seqExp(Tr_exp, Tr_exp);
Tr_exp Tr_assignExp(Tr_exp, Tr_exp);
Tr_exp Tr_ifExp(Tr_exp, Tr_exp, Tr_exp);
Tr_exp Tr_whileExp(Tr_exp, Tr_exp, Temp_label);
Tr_exp Tr_forExp(Tr_exp, Tr_exp, Tr_exp, Temp_label);
Tr_exp Tr_breakExp(Temp_label);
Tr_exp Tr_letExp(Tr_expList, Tr_exp);
Tr_exp Tr_arrayExp(Tr_exp, Tr_exp);
Tr_exp Tr_varDec(Tr_access, Tr_exp);

#endif
