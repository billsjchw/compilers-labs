#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "types.h"
#include "helper.h"
#include "env.h"
#include "semant.h"

typedef void* Tr_exp;
struct expty 
{
	Tr_exp exp; 
	Ty_ty ty;
};

struct expty expTy(Tr_exp exp, Ty_ty ty)
{
	struct expty e;

	e.exp = exp;
	e.ty = ty;

	return e;
}

static Ty_ty actualTy(Ty_ty);
static int isEqualTy(Ty_ty, Ty_ty);
static Ty_field getField(Ty_fieldList, S_symbol);
static Ty_fieldList transFieldList(S_table, A_fieldList);
static Ty_tyList fieldListToTyList(Ty_fieldList);

static string concat(string, string);

struct expty transVar(S_table venv, S_table tenv, A_var v)
{
	switch (v->kind) {
	case A_simpleVar: {
		E_enventry entry = S_look(venv, v->u.simple);
		if (entry != NULL && entry->kind == E_varEntry) {
			return expTy(NULL, actualTy(entry->u.var.ty));
		} else {
			EM_error(v->pos, "undefined variable %s", S_name(v->u.simple));
			return expTy(NULL, Ty_Int());
		}
	}
	case A_fieldVar: {
		struct expty et = transVar(venv, tenv, v->u.field.var);
		if (et.ty->kind == Ty_record) {
			Ty_field field = getField(et.ty->u.record, v->u.field.sym);
			if (field == NULL) {
				EM_error(v->pos, "field %s doesn't exist",
						S_name(v->u.field.sym));
				return expTy(NULL, Ty_Int());
			} else {
				return expTy(NULL, actualTy(field->ty));
			}
		} else {
			EM_error(v->pos, "not a record type");
			return expTy(NULL, Ty_Int());
		}
	}
	case A_subscriptVar: {
		struct expty var = transVar(venv, tenv, v->u.subscript.var);
		struct expty exp = transExp(venv, tenv, v->u.subscript.exp);
		if (var.ty->kind == Ty_array && exp.ty->kind == Ty_int) {
			return expTy(NULL, actualTy(var.ty->u.array));
		} else {
			EM_error(v->pos, "array type required");
			return expTy(NULL, Ty_Int());
		}
	}
	}
	assert(0);
}

struct expty transExp(S_table venv, S_table tenv, A_exp a)
{
	switch (a->kind) {
	case A_varExp: {
		struct expty et = transVar(venv, tenv, a->u.var);
		return expTy(NULL, et.ty);
	}
	case A_nilExp: {
		return expTy(NULL, Ty_Nil());
	}
	case A_intExp: {
		return expTy(NULL, Ty_Int());
	}
	case A_stringExp: {
		return expTy(NULL, Ty_String());
	}
	case A_callExp: {
		Ty_tyList tys;
		A_expList exps;
		E_enventry entry = S_look(venv, a->u.call.func);
		if (entry == NULL) {
			EM_error(a->pos, "undefined function %s", S_name(a->u.call.func));
			return expTy(NULL, Ty_Int());
		}
		for (tys = entry->u.fun.formals, exps = a->u.call.args;
				tys != NULL && exps != NULL;
				tys = tys->tail, exps = exps->tail) {
			struct expty et = transExp(venv, tenv, exps->head);
			if (!isEqualTy(tys->head, et.ty))
				EM_error(exps->head->pos, "para type mismatch");
		}
		if (tys == NULL && exps != NULL)
			EM_error(a->pos, "too many params in function %s",
					S_name(a->u.call.func));
		if (tys != NULL && exps == NULL)
			EM_error(a->pos, "");
		return expTy(NULL, actualTy(entry->u.fun.result));
	}
	case A_opExp: {
		struct expty left = transExp(venv, tenv, a->u.op.left);
		struct expty right = transExp(venv, tenv, a->u.op.right);
		switch (a->u.op.oper) {
		case A_plusOp:
		case A_minusOp:
		case A_timesOp:
		case A_divideOp:
			if (left.ty->kind != Ty_int)
				EM_error(a->u.op.left->pos, "integer required");
			if (right.ty->kind != Ty_int)
				EM_error(a->u.op.right->pos, "integer required");
			return expTy(NULL, Ty_Int());
		case A_eqOp:
		case A_neqOp:
			if (!isEqualTy(left.ty, right.ty))
				EM_error(a->pos, "same type required");
			return expTy(NULL, Ty_Int());
		case A_ltOp:
		case A_gtOp:
		case A_leOp:
		case A_geOp:
			if (!isEqualTy(left.ty, right.ty))
				EM_error(a->pos, "same type required");
			else if (left.ty->kind != Ty_int && left.ty->kind != Ty_string)
				EM_error(a->pos, "");
			return expTy(NULL, Ty_Int());
		}
		assert(0);
	}
	case A_recordExp: {
		Ty_ty ty = actualTy(S_look(tenv, a->u.record.typ));
		if (ty == NULL) {
			EM_error(a->pos, "undefined type %s", S_name(a->u.record.typ));
			return expTy(NULL, Ty_Nil());
		} else if (ty->kind != Ty_record) {
			EM_error(a->pos, "");
			return expTy(NULL, Ty_Nil());
		} else {
			A_efieldList efields;
			Ty_fieldList fields;
			for (efields = a->u.record.fields, fields = ty->u.record;
					efields != NULL && fields != NULL;
					efields = efields->tail, fields = fields->tail) {
				struct expty et = transExp(venv, tenv, efields->head->exp);
				if (efields->head->name != fields->head->name)
					EM_error(a->pos, "");
				else if (!isEqualTy(fields->head->ty, et.ty))
					EM_error(a->pos, "");
			}
			if (efields != NULL && fields == NULL)
				EM_error(a->pos, "");
			if (efields == NULL && fields != NULL)
				EM_error(a->pos, "");
			return expTy(NULL, ty);
		}
	}
	case A_seqExp: {
		A_expList exps;
		Ty_ty ty = Ty_Void();
		for (exps = a->u.seq; exps != NULL; exps = exps->tail) {
			struct expty et = transExp(venv, tenv, exps->head);
			ty = et.ty;
		}
		return expTy(NULL, ty);
	}
	case A_assignExp: {
		struct expty var = transVar(venv, tenv, a->u.assign.var);
		struct expty exp = transExp(venv, tenv, a->u.assign.exp);
		if (a->u.assign.var->kind == A_simpleVar) {
			E_enventry entry = S_look(venv, S_Symbol(concat("<loop_var>",
					S_name(a->u.assign.var->u.simple))));
			if (entry != NULL)
				EM_error(a->pos, "loop variable can't be assigned");
		}
		if (var.ty != exp.ty)
			EM_error(a->pos, "unmatched assign exp");
		return expTy(NULL, Ty_Void());
	}
	case A_ifExp: {
		struct expty test = transExp(venv, tenv, a->u.iff.test);
		struct expty then = transExp(venv, tenv, a->u.iff.then);
		if (test.ty->kind != Ty_int)
			EM_error(a->u.iff.test->pos, "");
		if (a->u.iff.elsee != NULL) {
			struct expty elsee = transExp(venv, tenv, a->u.iff.elsee);
			if (!isEqualTy(then.ty, elsee.ty))
				EM_error(a->pos, "then exp and else exp type mismatch");
			return expTy(NULL, then.ty);
		} else {
			if (then.ty->kind != Ty_void)
				EM_error(a->u.iff.then->pos,
						"if-then exp's body must produce no value");
			return expTy(NULL, Ty_Void());
		}
	}
	case A_whileExp: {
		struct expty body;
		struct expty test = transExp(venv, tenv, a->u.whilee.test);
		if (test.ty->kind != Ty_int)
			EM_error(a->u.whilee.test->pos, "");
		S_beginScope(venv);
		S_enter(venv, S_Symbol("<loop>"), E_VarEntry(Ty_Int()));
		body = transExp(venv, tenv, a->u.whilee.body);
		if (body.ty->kind != Ty_void)
			EM_error(a->u.whilee.body->pos, "while body must produce no value");
		S_endScope(venv);
		return expTy(NULL, Ty_Void());
	}
	case A_forExp: {
		struct expty body;
		struct expty lo = transExp(venv, tenv, a->u.forr.lo);
		struct expty hi = transExp(venv, tenv, a->u.forr.hi);
		if (lo.ty->kind != Ty_int)
			EM_error(a->u.forr.lo->pos, "for exp's range type is not integer");
		if (hi.ty->kind != Ty_int)
			EM_error(a->u.forr.hi->pos, "for exp's range type is not integer");
		S_beginScope(venv);
		S_enter(venv, a->u.forr.var, E_VarEntry(Ty_Int()));
		S_enter(venv, S_Symbol("<loop>"), E_VarEntry(Ty_Int()));
		S_enter(venv, S_Symbol(concat("<loop_var>", S_name(a->u.forr.var))),
					E_VarEntry(Ty_Int()));
		body = transExp(venv, tenv, a->u.forr.body);
		if (body.ty->kind != Ty_void)
			EM_error(a->u.forr.body->pos, "");
		S_endScope(venv);
		return expTy(NULL, Ty_Void());
	}
	case A_breakExp: {
		E_enventry entry = S_look(venv, S_Symbol("<loop>"));
		if (entry == NULL)
			EM_error(a->pos, "");
		return expTy(NULL, Ty_Void());
	}
	case A_letExp: {
		struct expty et;
		A_decList decs;
		S_beginScope(venv);
		S_beginScope(tenv);
		for (decs = a->u.let.decs; decs != NULL; decs = decs->tail)
			transDec(venv, tenv, decs->head);
		et = transExp(venv, tenv, a->u.let.body);
		S_endScope(tenv);
		S_endScope(venv);
		return et;
	}
	case A_arrayExp: {
		Ty_ty ty = actualTy(S_look(tenv, a->u.array.typ));
		struct expty init = transExp(venv, tenv, a->u.array.init);
		struct expty size = transExp(venv, tenv, a->u.array.size);
		if (size.ty->kind != Ty_int)
			EM_error(a->u.array.size->pos, "");
		if (ty == NULL) {
			EM_error(a->pos, "");
			return expTy(NULL, Ty_Array(Ty_Int()));
		} else if (ty->kind != Ty_array) {
			EM_error(a->pos, "");
			return expTy(NULL, Ty_Array(Ty_Int()));
		} else {
			if (!isEqualTy(ty->u.array, init.ty))
				EM_error(a->pos, "type mismatch");
			return expTy(NULL, ty);
		}
	}
	}
	assert(0);
}

void transDec(S_table venv, S_table tenv, A_dec d)
{
	switch (d->kind) {
	case A_functionDec: {
		A_fundecList fundecs, p, q;
		for (p = d->u.function; p != NULL; p = p->tail)
			for (q = p->tail; q != NULL; q = q->tail)
				if (p->head->name == q->head->name)
					EM_error(d->pos, "two functions have the same name");
		for (fundecs = d->u.function; fundecs != NULL;
				fundecs = fundecs->tail) {
			Ty_fieldList fields = transFieldList(tenv, fundecs->head->params);
			Ty_tyList formals = fieldListToTyList(fields);
			Ty_ty result = fundecs->head->result != NULL ?
					S_look(tenv, fundecs->head->result) : Ty_Void();
			S_enter(venv, fundecs->head->name, E_FunEntry(formals, result));
		}
		for (fundecs = d->u.function; fundecs != NULL;
				fundecs = fundecs->tail) {
			struct expty et;
			Ty_fieldList fields = transFieldList(tenv, fundecs->head->params);
			Ty_tyList formals = fieldListToTyList(fields);
			Ty_ty result = fundecs->head->result != NULL ?
					S_look(tenv, fundecs->head->result) : Ty_Void();
			S_beginScope(venv);
			for (; fields != NULL;
					fields = fields->tail, formals = formals->tail)
				S_enter(venv, fields->head->name, E_VarEntry(formals->head));
			et = transExp(venv, tenv, fundecs->head->body);
			S_endScope(venv);
			if (result->kind == Ty_void && et.ty->kind != Ty_void)
				EM_error(fundecs->head->pos, "procedure returns value");
			else if (!isEqualTy(result, et.ty))
				EM_error(fundecs->head->pos, "");
		}
		return;
	}
	case A_varDec: {
		struct expty et = transExp(venv, tenv, d->u.var.init);
		if (d->u.var.typ != NULL) {
			Ty_ty ty = S_look(tenv, d->u.var.typ);
			if (ty == NULL)
				EM_error(d->pos, "");
			else if (!isEqualTy(ty, et.ty))
				EM_error(d->pos, "type mismatch");
		} else if (et.ty->kind == Ty_nil) {
			EM_error(d->pos, "init should not be nil without type specified");
		}
		S_enter(venv, d->u.var.var, E_VarEntry(et.ty));
		return;
	}
	case A_typeDec: {
		A_nametyList nametys, p, q;
		bool illegal_cycle = FALSE;
		for (p = d->u.type; p != NULL; p = p->tail)
			for (q = p->tail; q != NULL; q = q->tail)
				if (p->head->name == q->head->name)
					EM_error(d->pos, "two types have the same name");
		for (nametys = d->u.type; nametys != NULL; nametys = nametys->tail) {
			S_symbol sym = nametys->head->name;
			S_enter(tenv, sym, Ty_Name(sym, NULL));
		}
		for (nametys = d->u.type; nametys != NULL; nametys = nametys->tail) {
			S_symbol sym = nametys->head->name;
			Ty_ty ty = transTy(tenv, nametys->head->ty);
			Ty_ty pro = S_look(tenv, sym);
			pro->u.name.ty = ty;
		}
		for (nametys = d->u.type; nametys != NULL; nametys = nametys->tail) {
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
			EM_error(d->pos, "illegal type cycle");
		return;
	}
	}
	assert(0);
}

Ty_ty transTy(S_table tenv, A_ty a)
{
	switch (a->kind) {
	case A_nameTy: {
		Ty_ty ty = S_look(tenv, a->u.name);
		if (ty != NULL) {
			return ty;
		} else {
			EM_error(a->pos, "undefined type %s", S_name(a->u.name));
			return Ty_Int();
		}
	}
	case A_recordTy: {
		return Ty_Record(transFieldList(tenv, a->u.record));
	}
	case A_arrayTy: {
		Ty_ty ty = S_look(tenv, a->u.name);
		if (ty == NULL) {
			EM_error(a->pos, "undefined type %s", S_name(a->u.name));
			ty = Ty_Int();
		}
		return Ty_Array(ty);
	}
	}
	assert(0);
}

void SEM_transProg(A_exp exp)
{
	S_table venv = E_base_venv();
	S_table tenv = E_base_tenv();
	transExp(venv, tenv, exp);
}

static Ty_ty actualTy(Ty_ty ty)
{
	if (ty == NULL)
		return NULL;
	else
		return ty->kind == Ty_name ? ty->u.name.ty : ty;
}

static int isEqualTy(Ty_ty ty1, Ty_ty ty2)
{
	ty1 = actualTy(ty1);
	ty2 = actualTy(ty2);
	return ty1 == ty2 || (ty1->kind == Ty_nil && ty2->kind == Ty_record) ||
			(ty2->kind == Ty_nil && ty1->kind == Ty_record);
}

static Ty_field getField(Ty_fieldList fields, S_symbol sym)
{
	if (fields == NULL)
		return NULL;
	else if (fields->head->name == sym)
		return fields->head;
	else
		return getField(fields->tail, sym);
}

static Ty_fieldList transFieldList(S_table tenv, A_fieldList a)
{
	if (a == NULL)
		return NULL;
	
	Ty_ty ty = S_look(tenv, a->head->typ);
	if (ty == NULL) {
		EM_error(a->head->pos, " undefined type %s", S_name(a->head->typ));
		ty = Ty_Int();
	}

	Ty_field field = Ty_Field(a->head->name, ty);

	return Ty_FieldList(field, transFieldList(tenv, a->tail));
}

static string concat(string str1, string str2)
{
	string ret;

	ret = (char *) checked_malloc(strlen(str1) + strlen(str2) + 1);
	strcpy(ret, str1);
	strcat(ret, str2);

	return ret;
}

static Ty_tyList fieldListToTyList(Ty_fieldList fields)
{
	return fields != NULL ?
			Ty_TyList(fields->head->ty, fieldListToTyList(fields->tail)) : NULL;
}
