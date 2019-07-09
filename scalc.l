%{

#include <stdio.h>
#include "header.h"
#include "scalc.tab.h"

extern int col;
#define YY_USER_ACTION col += strlen(yytext);

%}

%option yylineno

%%

[ \t]	 	{   }
[\n]		{ col = 0; }
"//".*		{   }

";"			{ return ';'; }
"+"			{ return '+'; }
"-"			{ return '-'; }
"*"			{ return '*'; }
"/"			{ return '/'; }
"="			{ return '='; }
[<>!{}]		{ return yytext[0]; }
"FUNCTION"  { return FUNCTION; }
"PRINT"		{ return PRINT; }
"WHILE"		{ return WHILE; }
"AND"		{ return AND; }
"OR"		{ return OR; }
"IF"		{ return IF; }
"ELSE"		{ return ELSE; }
"("			{ return '('; }
")"			{ return ')'; }

 /* arduino */
"IN"		{ return IN; }
"OUT"		{ return OUT; }
"DELAY"		{ return DELAY; }

[0-9]+		{ yylval.args.constvalue = atoi(yytext); 
			  return INT; }

[0-9]+\.[0-9]+	{ yylval.args.constvalue = atof(yytext);
				  return DBL; 
				}


[A-Z][A-Z_0-9]*		{ yylval.args.varname = strndup(yytext, yyleng);
					  yylval.args.line = yylineno;
					  yylval.args.col = col - yyleng;
					  return VAR; 
					}

.		{ printf("Nao era esperado %s\n", yytext);
		  exit(1);
		}

%%


