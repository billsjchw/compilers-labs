#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "types.h"
#include "env.h"
#include "semant.h"
#include "helper.h"
#include "translate.h"

/*Lab5: Your implementation of lab5.*/

struct expty {
	Tr_exp exp; 
	Ty_ty ty;
};

struct expty expTy(Tr_exp exp, Ty_ty ty) {
	struct expty e;

	e.exp = exp;
	e.ty = ty;

	return e;
}

static Ty_ty actualTy(Ty_ty);
static int isEqualTy(Ty_ty, Ty_ty);
static Tr_expList transArgList(S_table, S_table, A_expList, Ty_tyList, Tr_level, Temp_label);
static Tr_expList transEfieldList(S_table, S_table, A_efieldList, Ty_fieldList, Tr_level, Temp_label);
static struct expty transSeqExp(S_table, S_table, A_expList, Tr_level, Temp_label);
static Tr_expList transDecList(S_table, S_table, A_decList, Tr_level, Temp_label);
static Ty_fieldList transFieldList(S_table, A_fieldList);
static Ty_tyList fieldListToTyList(Ty_fieldList);
static U_boolList fieldListToEscapeList(A_fieldList);

F_fragList SEM_transProg(A_exp exp) {
    S_table venv = E_base_venv();
    S_table tenv = E_base_tenv();

    Temp_label label = Temp_namedlabel("tigermain");
    Tr_level level = Tr_newLevel(Tr_outermost(), label, NULL);
	struct expty et = transExp(venv, tenv, exp, level, NULL);
    Tr_procEntryExit(level, et.exp);
    
    return Tr_getResult();
}

struct expty transVar(S_table venv, S_table tenv, A_var var, Tr_level level,
                      Temp_label done) {
    switch (var->kind) {
    case A_simpleVar: {
        E_enventry entry = S_look(venv, var->u.simple);
        if (entry != NULL && entry->kind == E_varEntry)
            return expTy(Tr_simpleVar(entry->u.var.access, level), actualTy(entry->u.var.ty));
        else
            exit(1);
    }
    case A_fieldVar: {
        struct expty et = transVar(venv, tenv, var->u.field.var, level, done);
        if (et.ty->kind == Ty_record) {
            Ty_fieldList fields = NULL;
            unsigned k = 0;
            for (fields = et.ty->u.record, k = 0; fields != NULL;
                 fields = fields->tail, ++k)
                if (fields->head->name == var->u.field.sym)
                    break;
            if (fields != NULL)
                return expTy(Tr_fieldVar(et.exp, k), actualTy(fields->head->ty));
            else
                exit(1);
        } else {
            exit(1);
        }
    }
    case A_subscriptVar: {
        struct expty etVar = transVar(venv, tenv, var->u.subscript.var, level, done);
        struct expty etExp = transExp(venv, tenv, var->u.subscript.exp, level, done);
        if (etVar.ty->kind == Ty_array && etExp.ty->kind == Ty_int)
            return expTy(Tr_subscriptVar(etVar.exp, etExp.exp), actualTy(etVar.ty->u.array));
        else
            exit(1);
    }
    default: {
        assert(0);
    }
    }
}

struct expty transExp(S_table venv, S_table tenv, A_exp exp, Tr_level level, Temp_label done) {
    switch (exp->kind) {
    case A_varExp: {
        return transVar(venv, tenv, exp->u.var, level, done);
    }
    case A_nilExp: {
        return expTy(Tr_nilExp(), Ty_Nil());
    }
    case A_intExp: {
        return expTy(Tr_intExp(exp->u.intt), Ty_Int());
    }
    case A_stringExp: {
        return expTy(Tr_stringExp(exp->u.stringg), Ty_String());
    }
    case A_callExp: {
        Ty_tyList tys = NULL;
        A_expList exps = NULL;
        Tr_expList exprs = NULL;
        E_enventry entry = S_look(venv, exp->u.call.func);
        if (entry == NULL || entry->kind != E_funEntry)
            exit(1);
        struct expty et = expTy(Tr_callExp(entry->u.fun.label, level, entry->u.fun.level,
                                transArgList(venv, tenv, exp->u.call.args, entry->u.fun.formals, level, done)),
                     actualTy(entry->u.fun.result));
        return et;
    }
    case A_opExp: {
        struct expty etLeft = transExp(venv, tenv, exp->u.op.left, level, done);
        struct expty etRight = transExp(venv, tenv, exp->u.op.right, level, done);
        switch (exp->u.op.oper) {
        case A_plusOp:
        case A_minusOp:
        case A_timesOp:
        case A_divideOp:
            if (etLeft.ty->kind != Ty_int || etRight.ty->kind != Ty_int)
                exit(1);
            break;
        case A_eqOp:
        case A_neqOp:
            if (!isEqualTy(etLeft.ty, etRight.ty))
                exit(1);
            break;
        case A_ltOp:
        case A_gtOp:
        case A_leOp:
        case A_geOp:
            if ((etLeft.ty->kind != Ty_int ||
                 etRight.ty->kind != Ty_int) &&
                (etLeft.ty->kind != Ty_string ||
                 etRight.ty->kind != Ty_string))
                exit(1);
            break;
        default:
            assert(0);
        }
        return expTy(etLeft.ty->kind == Ty_string ?
                         Tr_stringOpExp(exp->u.op.oper, etLeft.exp, etRight.exp) :
                         Tr_intOpExp(exp->u.op.oper, etLeft.exp, etRight.exp),
                     Ty_Int());
    }
    case A_recordExp: {
        Ty_ty ty = actualTy(S_look(tenv, exp->u.record.typ));
        if (ty == NULL || ty->kind != Ty_record)
            exit(1);
        return expTy(Tr_recordExp(transEfieldList(venv, tenv, exp->u.record.fields, ty->u.record, level, done)), ty);
    }
    case A_seqExp: {
        return transSeqExp(venv, tenv, exp->u.seq, level, done);
    }
    case A_assignExp: {
        struct expty etVar = transVar(venv, tenv, exp->u.assign.var, level, done);
        struct expty etExp = transExp(venv, tenv, exp->u.assign.exp, level, done);
        if (exp->u.assign.var->kind == A_simpleVar) {
            E_enventry entry = S_look(venv, exp->u.assign.var->u.simple);
            if (entry == NULL || entry->readonly)
                exit(1);
        }
        if (!isEqualTy(etVar.ty, etExp.ty))
            exit(1);
        return expTy(Tr_assignExp(etVar.exp, etExp.exp), Ty_Void());
    }
    case A_ifExp: {
        struct expty etTest = transExp(venv, tenv, exp->u.iff.test, level, done);
        struct expty etThen = transExp(venv, tenv, exp->u.iff.then, level, done);
        
        if (etTest.ty->kind != Ty_int)
            exit(1);
        if (exp->u.iff.elsee != NULL) {
            struct expty etElse = transExp(venv, tenv, exp->u.iff.elsee, level, done);
            if (!isEqualTy(etThen.ty, etElse.ty))
                exit(1);
            struct expty et = expTy(Tr_ifExp(etTest.exp, etThen.exp, etElse.exp), etThen.ty);
            return et;
        } else {
            if (etThen.ty->kind != Ty_void)
                exit(1);
            return expTy(Tr_ifExp(etTest.exp, etThen.exp, NULL), Ty_Void());
        }
    }
    case A_whileExp: {
        Temp_label newDone = Temp_newlabel();
        struct expty etTest = transExp(venv, tenv, exp->u.whilee.test, level, done);
        struct expty etBody = transExp(venv, tenv, exp->u.whilee.body, level, newDone);
        if (etTest.ty->kind != Ty_int || etBody.ty->kind != Ty_void)
            exit(1);
        return expTy(Tr_whileExp(etTest.exp, etBody.exp, newDone), Ty_Void());
    }
    case A_forExp: {
        Temp_label newDone = Temp_newlabel();
        Tr_access access = Tr_allocLocal(level, TRUE);
        struct expty etLo = transExp(venv, tenv, exp->u.forr.lo, level, done);
        struct expty etHi = transExp(venv, tenv, exp->u.forr.hi, level, done);
        struct expty etBody = {NULL, NULL};
        S_beginScope(venv);
        S_enter(venv, exp->u.forr.var, E_ROVarEntry(access, Ty_Int()));
        etBody = transExp(venv, tenv, exp->u.forr.body, level, newDone);
        S_endScope(venv);
        if (etLo.ty->kind != Ty_int || etHi.ty->kind != Ty_int || etBody.ty->kind != Ty_void)
            exit(1);
        return expTy(Tr_forExp(etLo.exp, etHi.exp, etBody.exp, newDone), Ty_Void());
    }
    case A_breakExp: {
        if (done == NULL)
            exit(1);
        return expTy(Tr_breakExp(done), Ty_Void());
    }
    case A_letExp: {
        Tr_expList exps = NULL;
        struct expty et = {NULL, NULL};
        S_beginScope(venv);
        S_beginScope(tenv);
        exps = transDecList(venv, tenv, exp->u.let.decs, level, done);
        et = transExp(venv, tenv, exp->u.let.body, level, done);
        S_endScope(tenv);
        S_endScope(venv);
        return expTy(Tr_letExp(exps, et.exp), et.ty);
    }
    case A_arrayExp: {
        Ty_ty ty = actualTy(S_look(tenv, exp->u.array.typ));
        struct expty etSize = transExp(venv, tenv, exp->u.array.size, level, done);
        struct expty etInit = transExp(venv, tenv, exp->u.array.init, level, done);
        if (ty == NULL || ty->kind != Ty_array || !isEqualTy(ty->u.array, etInit.ty) || etSize.ty->kind != Ty_int)
            exit(1);
        return expTy(Tr_arrayExp(etSize.exp, etInit.exp), ty);
    }
    default: {
        assert(0);
    }
    }
}

Tr_exp transDec(S_table venv, S_table tenv, A_dec dec, Tr_level level, Temp_label done) {
    switch (dec->kind) {
    case A_functionDec: {
        A_fundecList fundecs = NULL;
        A_fundecList p = NULL;
        A_fundecList q = NULL;
        for (p = dec->u.function; p != NULL; p = p->tail)
            for (q = p->tail; q != NULL; q = q->tail)
                if (p->head->name == q->head->name)
                    exit(1);
        for (fundecs = dec->u.function; fundecs != NULL; fundecs = fundecs->tail) {
            Ty_fieldList fields = transFieldList(tenv, fundecs->head->params);
            Ty_tyList formals = fieldListToTyList(fields);
            Ty_ty result = fundecs->head->result != NULL ? S_look(tenv, fundecs->head->result) : Ty_Void();
            U_boolList escapes = fieldListToEscapeList(fundecs->head->params);
            Temp_label label = Temp_newlabel();
            Tr_level newLevel = Tr_newLevel(level, label, escapes);
            if (result == NULL)
                exit(1);
            S_enter(venv, fundecs->head->name, E_FunEntry(newLevel, label, formals, result));
        }
        for (fundecs = dec->u.function; fundecs != NULL; fundecs = fundecs->tail) {
            struct expty et = {NULL, NULL};
            Ty_fieldList fields = transFieldList(tenv, fundecs->head->params);
            Ty_ty result = fundecs->head->result != NULL ? S_look(tenv, fundecs->head->result) : Ty_Void();
            E_enventry entry = S_look(venv, fundecs->head->name);
            Tr_level newLevel = entry->u.fun.level;
            Tr_accessList accesses = Tr_formals(newLevel);
            S_beginScope(venv);
            for (; fields != NULL; fields = fields->tail, accesses = accesses->tail)
                S_enter(venv, fields->head->name, E_VarEntry(accesses->head, fields->head->ty));
            et = transExp(venv, tenv, fundecs->head->body, newLevel, NULL);
            S_endScope(venv);
            if (!isEqualTy(et.ty, result))
                exit(1);
            Tr_procEntryExit(newLevel, et.exp);
        }
        return Tr_nop();
    }
    case A_varDec: {
        Tr_access access = Tr_allocLocal(level, dec->u.var.escape);
        struct expty et = transExp(venv, tenv, dec->u.var.init, level, done);
        if (dec->u.var.typ != NULL) {
            Ty_ty ty = S_look(tenv, dec->u.var.typ);
            if (ty == NULL || !isEqualTy(ty, et.ty))
                exit(1);
        } else if (et.ty->kind == Ty_nil) {
            exit(1);
        }
        S_enter(venv, dec->u.var.var, E_VarEntry(access, et.ty));
        return Tr_varDec(access, et.exp);
    }
    case A_typeDec: {
        A_nametyList nametys, p, q;
		bool illegal_cycle = FALSE;
		for (p = dec->u.type; p != NULL; p = p->tail)
			for (q = p->tail; q != NULL; q = q->tail)
				if (p->head->name == q->head->name)
					exit(1);
		for (nametys = dec->u.type; nametys != NULL; nametys = nametys->tail) {
			S_symbol sym = nametys->head->name;
			S_enter(tenv, sym, Ty_Name(sym, NULL));
		}
		for (nametys = dec->u.type; nametys != NULL; nametys = nametys->tail) {
			S_symbol sym = nametys->head->name;
			Ty_ty ty = transTy(tenv, nametys->head->ty);
			Ty_ty pro = S_look(tenv, sym);
			pro->u.name.ty = ty;
		}
		for (nametys = dec->u.type; nametys != NULL; nametys = nametys->tail) {
			S_symbol sym = nametys->head->name;
			Ty_ty ty = S_look(tenv, sym);
			Ty_ty p = ty;
			Ty_ty q = actualTy(ty);
			while (p != q) {
				p = actualTy(p);
				q = actualTy(actualTy(q));
			}
			if (p->kind == Ty_name)
				illegal_cycle = TRUE;
			ty->u.name.ty = p;
		}
		if (illegal_cycle)
			exit(1);
		return Tr_nop();
    }
    default: {
        assert(0);
    }
    }
}

Ty_ty transTy(S_table tenv, A_ty ty) {
    switch (ty->kind) {
	case A_nameTy: {
		Ty_ty type = S_look(tenv, ty->u.name);
        if (type == NULL)
            exit(1);
        return type;
	}
	case A_recordTy: {
		return Ty_Record(transFieldList(tenv, ty->u.record));
	}
	case A_arrayTy: {
		Ty_ty type = S_look(tenv, ty->u.name);
		if (type == NULL)
            exit(1);
		return Ty_Array(type);
	}
    default: {
        assert(0);
    }
	}
}

static Ty_ty actualTy(Ty_ty ty) {
    if (ty == NULL)
        return NULL;
    else if (ty->kind == Ty_name)
        return ty->u.name.ty;
    else
        return ty;
}

static int isEqualTy(Ty_ty ty1, Ty_ty ty2) {
	ty1 = actualTy(ty1);
	ty2 = actualTy(ty2);
	return ty1 == ty2 || (ty1->kind == Ty_nil && ty2->kind == Ty_record) ||
	       (ty2->kind == Ty_nil && ty1->kind == Ty_record);
}

static Tr_expList transArgList(S_table venv, S_table tenv, A_expList args, Ty_tyList tys, Tr_level level,
                               Temp_label done) {
    struct expty et = {NULL, NULL};
    
    if (args == NULL && tys == NULL)
        return NULL;
    
    if (args == NULL || tys == NULL)
        exit(1);
    et = transExp(venv, tenv, args->head, level, done);
    if (!isEqualTy(et.ty, tys->head))
        exit(1);
    return Tr_ExpList(et.exp, transArgList(venv, tenv, args->tail, tys->tail, level, done));
}

static Tr_expList transEfieldList(S_table venv, S_table tenv, A_efieldList efields, Ty_fieldList fields,
                                  Tr_level level, Temp_label done) {
    struct expty et = {NULL, NULL};
    
    if (efields == NULL && fields == NULL)
        return 0;

    if (efields == NULL || fields == NULL)
        exit(1);
    if (efields->head->name != fields->head->name)
        exit(1);
    et = transExp(venv, tenv, efields->head->exp, level, done);
    if (!isEqualTy(et.ty, fields->head->ty))
        exit(1);
    return Tr_ExpList(et.exp, transEfieldList(venv, tenv, efields->tail, fields->tail, level, done));
}

static struct expty transSeqExp(S_table venv, S_table tenv, A_expList exps, Tr_level level, Temp_label done) {
    struct expty etHead = {NULL, NULL};
    struct expty etTail = {NULL, NULL};
    
    if (exps == NULL)
        return expTy(Tr_nop(), Ty_Void());
    
    if (exps->tail == NULL)
        return transExp(venv, tenv, exps->head, level, done);
    
    etHead = transExp(venv, tenv, exps->head, level, done);
    etTail = transSeqExp(venv, tenv, exps->tail, level, done);
    return expTy(Tr_seqExp(etHead.exp, etTail.exp), etTail.ty);
}

static Tr_expList transDecList(S_table venv, S_table tenv, A_decList decs, Tr_level level, Temp_label done) {
    Tr_exp exp = NULL;
    Tr_expList exps = NULL;
    
    if (decs == NULL)
        return NULL;

    exp = transDec(venv, tenv, decs->head, level, done);
    exps = transDecList(venv, tenv, decs->tail, level, done);
    return Tr_ExpList(exp, exps);
}

static Ty_fieldList transFieldList(S_table tenv, A_fieldList fields) {
	Ty_ty ty = NULL;
    
    if (fields == NULL)
		return NULL;
	
	ty = S_look(tenv, fields->head->typ);
	if (ty == NULL)
        exit(1);
	return Ty_FieldList(Ty_Field(fields->head->name, ty), transFieldList(tenv, fields->tail));
}

static Ty_tyList fieldListToTyList(Ty_fieldList fields) {
    if (fields == NULL)
        return NULL;
    
    return Ty_TyList(fields->head->ty, fieldListToTyList(fields->tail));
}

static U_boolList fieldListToEscapeList(A_fieldList fields) {
    if (fields == NULL)
        return NULL;

    return U_BoolList(fields->head->escape, fieldListToEscapeList(fields->tail));
}
