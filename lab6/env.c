#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "env.h" 

/*Lab4: Your implementation of lab4*/

E_enventry E_VarEntry(Tr_access access, Ty_ty ty)
{
	E_enventry entry = checked_malloc(sizeof(*entry));

	entry->kind = E_varEntry;
	entry->u.var.access = access;
	entry->u.var.ty = ty;
	return entry;
}

E_enventry E_ROVarEntry(Tr_access access, Ty_ty ty)
{
	E_enventry entry = checked_malloc(sizeof(*entry));

	entry->kind = E_varEntry;
	entry->u.var.access = access;
	entry->u.var.ty = ty;
	entry->readonly = 1;
	return entry;
}

E_enventry E_FunEntry(Tr_level level, Temp_label label, Ty_tyList formals, Ty_ty result)
{
	E_enventry entry = checked_malloc(sizeof(*entry));

	entry->kind = E_funEntry;
	entry->u.fun.level = level;
	entry->u.fun.label = label;
	entry->u.fun.formals = formals;
	entry->u.fun.result = result;
	return entry;
}

//sym->value
//type_id(name, S_symbol) -> type (Ty_ty)
S_table E_base_tenv(void)
{
	S_table table;
	S_symbol ty_int;
	S_symbol ty_string;

	table = S_empty();

	//basic type: string
	ty_int = S_Symbol("int");
	S_enter(table, ty_int, Ty_Int());	

	//basic type: string
	ty_string = S_Symbol("string");
	S_enter(table, ty_string, Ty_String());	

	return table;
}

S_table E_base_venv(void)
{
	S_table ret = S_empty();

	S_enter(ret, S_Symbol("print"), E_FunEntry(
		Tr_outermost(),
		Temp_namedlabel("print"),
		Ty_TyList(Ty_String(), NULL),
		Ty_Void()
	));
	S_enter(ret, S_Symbol("printi"), E_FunEntry(
		Tr_outermost(),
		Temp_namedlabel("printi"),
		Ty_TyList(Ty_Int(), NULL),
		Ty_Void()
	));
	S_enter(ret, S_Symbol("flush"), E_FunEntry(
		Tr_outermost(),
		Temp_namedlabel("flush"),
		NULL,
		Ty_Void()
	));
	S_enter(ret, S_Symbol("getchar"), E_FunEntry(
		Tr_outermost(),
		Temp_namedlabel("getchar"),
		NULL,
		Ty_String()
	));
	S_enter(ret, S_Symbol("ord"), E_FunEntry(
		Tr_outermost(),
		Temp_namedlabel("ord"),
		Ty_TyList(Ty_String(), NULL),
		Ty_Int()
	));
	S_enter(ret, S_Symbol("chr"), E_FunEntry(
		Tr_outermost(),
		Temp_namedlabel("chr"),
		Ty_TyList(Ty_Int(), NULL),
		Ty_String()
	));
	S_enter(ret, S_Symbol("size"), E_FunEntry(
		Tr_outermost(),
		Temp_namedlabel("size"),
		Ty_TyList(Ty_String(), NULL),
		Ty_Int()
	));
	S_enter(ret, S_Symbol("substring"), E_FunEntry(
		Tr_outermost(),
		Temp_namedlabel("subtring"),
		Ty_TyList(Ty_String(), Ty_TyList(Ty_Int(), Ty_TyList(Ty_Int(), NULL))),
		Ty_String()
	));
	S_enter(ret, S_Symbol("concat"), E_FunEntry(
		Tr_outermost(),
		Temp_namedlabel("concat"),
		Ty_TyList(Ty_String(), Ty_TyList(Ty_String(), NULL)),
		Ty_String()
	));
	S_enter(ret, S_Symbol("not"), E_FunEntry(
		Tr_outermost(),
		Temp_namedlabel("not"),
		Ty_TyList(Ty_Int(), NULL),
		Ty_Int()
	));
	S_enter(ret, S_Symbol("exit"), E_FunEntry(
		Tr_outermost(),
		Temp_namedlabel("exit"),
		Ty_TyList(Ty_Int(), NULL),
		Ty_Void()
	));

	return ret;
}
