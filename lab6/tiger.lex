%{
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "y.tab.h"
#include "errormsg.h"

int charPos=1;

int yywrap(void)
{
 charPos=1;
 return 1;
}

void adjust(void)
{
 EM_tokPos=charPos;
 charPos+=yyleng;
}

/* @function: getstr
 * @input: a string literal
 * @output: the string value for the input which has all the escape sequences 
 * translated into their meaning.
 */
char *getstr(const char *str)
{
	char *ret;
  size_t i, j, len;

  len = strlen(str);
  
  ret = checked_malloc(len);

  j = 0;
  for (i = 1; i < len - 1; ++i)
    if (str[i] != '\\') {
      ret[j++] = str[i];
    } else if (str[i + 1] == 'n') {
      ret[j++] = '\n';
      ++i;
    } else if (str[i + 1] == 't') {
      ret[j++] = '\t';
      ++i;
    } else if (str[i + 1] == '\"') {
      ret[j++] = '\"';
      ++i;
    } else if (str[i + 1] == '\\') {
      ret[j++] = '\\';
      ++i;
    } else if (str[i + 1] == '^') {
      ret[j++] = (str[i + 2] - 'A') + 1;
      i += 2;
    } else if (str[i + 1] >= '0' && str[i + 1] <= '9') {
      ret[j++] = (str[i + 1] - '0') * 100 + (str[i + 2] - '0') * 10 +
          (str[i + 3] - '0');
      i += 3;
    } else {
      do
        ++i;
      while (str[i] != '\\');
    }
  ret[j] = '\0';

	return ret;
}

int comm_depth;

%}

%Start ORIGIN COMMENT
printable [\40-\41\43-\133\135-\176]
escape (\\[nt\"\\]|\\\^[A-Z]|\\[0-9][0-9][0-9]|\\[\ \t\n\f]+\\)

%%
<ORIGIN>, { adjust(); return COMMA; }
<ORIGIN>: { adjust(); return COLON; }
<ORIGIN>; { adjust(); return SEMICOLON; }
<ORIGIN>\( { adjust(); return LPAREN; }
<ORIGIN>\) { adjust(); return RPAREN; }
<ORIGIN>\[ { adjust(); return LBRACK; }
<ORIGIN>\] { adjust(); return RBRACK; }
<ORIGIN>\{ { adjust(); return LBRACE; }
<ORIGIN>\} { adjust(); return RBRACE; }
<ORIGIN>\. { adjust(); return DOT; }
<ORIGIN>\+ { adjust(); return PLUS; }
<ORIGIN>\- { adjust(); return MINUS; }
<ORIGIN>\* { adjust(); return TIMES; }
<ORIGIN>\/ { adjust(); return DIVIDE; }
<ORIGIN>= { adjust(); return EQ; }
<ORIGIN><> { adjust(); return NEQ; }
<ORIGIN>< { adjust(); return LT; }
<ORIGIN><= { adjust(); return LE; }
<ORIGIN>> { adjust(); return GT; }
<ORIGIN>>= { adjust(); return GE; }
<ORIGIN>& { adjust(); return AND; }
<ORIGIN>\| { adjust(); return OR; }
<ORIGIN>:= { adjust(); return ASSIGN; }
<ORIGIN>array { adjust(); return ARRAY; }
<ORIGIN>if { adjust(); return IF; }
<ORIGIN>then { adjust(); return THEN; }
<ORIGIN>else { adjust(); return ELSE; }
<ORIGIN>while { adjust(); return WHILE; }
<ORIGIN>for { adjust(); return FOR; }
<ORIGIN>to { adjust(); return TO; }
<ORIGIN>do { adjust(); return DO; }
<ORIGIN>let { adjust(); return LET; }
<ORIGIN>in { adjust(); return IN; }
<ORIGIN>end { adjust(); return END; }
<ORIGIN>of { adjust(); return OF; }
<ORIGIN>break { adjust(); return BREAK; }
<ORIGIN>nil { adjust(); return NIL; }
<ORIGIN>function { adjust(); return FUNCTION; }
<ORIGIN>var { adjust(); return VAR; }
<ORIGIN>type { adjust(); return TYPE; }
<ORIGIN>\/\* { adjust(); comm_depth = 1; BEGIN COMMENT; }
<ORIGIN>[A-Za-z][A-Za-z0-9_]* { adjust(); yylval.sval = String(yytext); return ID; }
<ORIGIN>[0-9]+ { adjust(); yylval.ival = atoi(yytext); return INT; }
<ORIGIN>\"({printable}|{escape})*\" { adjust(); yylval.sval = String(getstr(yytext)); return STRING; }
<ORIGIN>[\ \n\t]+ { adjust(); }
<ORIGIN>. { adjust(); EM_error(EM_tokPos, "illegal character"); }
<COMMENT>\*\/ { adjust(); if (--comm_depth == 0) BEGIN ORIGIN; }
<COMMENT>\/\* { adjust(); ++comm_depth; }
<COMMENT>(.|\n) { adjust(); }
. { BEGIN ORIGIN; yyless(0); }
