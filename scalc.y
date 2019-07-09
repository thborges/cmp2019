%{

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "header.h"

int col = 0;
extern int yylineno;
extern FILE *yyin;
int yyerror(const char *s);
int yylex (void);

int sycount = 0;
int error_count = 0;
char *filename;

syntno *create_no(const char name, enum syntno_type t, short children); 
void print_tree(syntno *root);
targs *copy_targs(const targs a);

%}

%union {
	targs args;
	struct syntno *no;
}

%define parse.error verbose

%token PRINT WHILE AND OR IF ELSE FUNCTION
%token IN OUT DELAY
%token <args> INT DBL
%token <args> VAR

%type <no> stmts stmt arit term factor unary print 
%type <no> while logical lterm lfactor if
%type <no> out delay fcall functiondef functioncall

%start program

%%

program : stmts	{ //print_symbols();
				  //print_tree($1);

				  visitor_leaf_first(&($1), collapse_stmts);
				  visitor_leaf_first(&($1), declared_vars);

				  if (error_count > 0)
				  	exit(error_count);

				  // gera codigo
				  setup_llvm_global();
				  main_generate_llvm_nodes($1);
				  print_llvm_ir();
				}
		;

stmts	: stmt stmts		{ syntno *u = create_no('S', NO_STMTS, 2);
							  u->children[0] = $1;
							  u->children[1] = $2;
							  $$ = u;
							}
		| stmt				{ syntno *u = create_no('s', NO_STMT, 1);
							  u->children[0] = $1;
							  $$ = u;
							}
		;


stmt	: VAR '=' arit ';'	{ add_symbol($1.varname, $1.line, $1.col); 
							  syntno *u = create_no('A', NO_ATTR, 2);

							  syntno *uvar = create_no('V', NO_TOK, 0);
							  uvar->token_args = copy_targs($1);
							  u->children[0] = uvar;

							  u->children[1] = $3;
							  $$ = u;
							} 
		| print ';'			{ $$ = $1; }
		| while				{ $$ = $1; }
		| if				{ $$ = $1; }
		| out ';'			{ $$ = $1; }
		| delay ';'			{ $$ = $1; }
		| functiondef		{ $$ = $1; }
		| functioncall ';'	{ $$ = $1; }
		| error ';'			{ }
		;


functiondef : FUNCTION VAR '(' ')' '{' stmts '}'
				{ add_symbol($2.varname, $2.line, $2.col);
				  syntno *u = create_no('F', NO_FUNC, 1);
				  u->token_args = copy_targs($2);
				  u->children[0] = $6;
				  $$ = u;
				}
			;

functioncall : VAR '(' ')'	{ syntno *u = create_no('C', NO_CALL, 0);
							  u->token_args = copy_targs($1);
							  $$ = u;
							}
			 ;

out : OUT INT '=' factor	{ syntno *intconst = create_no('I', NO_TOK, 0);
							  intconst->token_args = copy_targs($2);
							  
							  syntno *u = create_no('O', NO_OUT, 2);
							  u->children[0] = intconst;
							  u->children[1] = $4;
							  $$ = u;
							}

	| OUT VAR '=' arit		{ syntno *var = create_no('V', NO_TOK, 0);
							  var->token_args = copy_targs($2);

							  syntno *u = create_no('O', NO_OUT, 2);
							  u->children[0] = var;
							  u->children[1] = $4;
							  $$ = u;
							}
	;

delay : DELAY arit			{ syntno *u = create_no('D', NO_DELAY, 1);
							  u->children[0] = $2;
							  $$ = u;
							}
	  ;

if : IF '(' logical ')' '{' stmts '}'
				{ syntno *u = create_no('I', NO_IF, 2);
				  u->children[0] = $3;
				  u->children[1] = $6;
				  $$ = u;
				}

   | IF '(' logical ')' '{' stmts '}' ELSE if
				{ syntno *u = create_no('I', NO_IF, 3);
				  u->children[0] = $3;
				  u->children[1] = $6;
				  u->children[2] = $9;
				  $$ = u;
				}

   | IF '(' logical ')' '{' stmts '}' ELSE '{' stmts '}'
				{ syntno *u = create_no('I', NO_IF, 3);
				  u->children[0] = $3;
				  u->children[1] = $6;
				  u->children[2] = $10;
				  $$ = u;
				}
   ;

print	: PRINT VAR			{ syntno *u = create_no('P', NO_PRNT, 0);
							  u->token_args = copy_targs($2);
							  $$ = u;
							}
		;

while	: WHILE '(' logical ')' '{' stmts '}'
							{ syntno *u = create_no('W', NO_WHILE, 2);
							  u->children[0] = $3;
							  u->children[1] = $6;
							  $$ = u;
							}
		;

logical : logical OR lterm	{ syntno *u = create_no('l', NO_OR, 2);
							  u->children[0] = $1;
							  u->children[1] = $3;
							  $$ = u;
							}
		| lterm				{ $$ = $1; }
		;

lterm	: lterm AND lfactor	{ syntno *u = create_no('l', NO_AND, 2);
							  u->children[0] = $1;
							  u->children[1] = $3;
							  $$ = u;
							} 
		| lfactor			{ $$ = $1; }
		;

lfactor : '(' logical ')'	{ $$ = $2; }
		| arit '>' arit		{ syntno *u = create_no('l', NO_GT, 2);
							  u->children[0] = $1;
							  u->children[1] = $3;
							  $$ = u;
							}
		| arit '<' arit		{ syntno *u = create_no('l', NO_LT, 2);
							  u->children[0] = $1;
							  u->children[1] = $3;
							  $$ = u;
							}
		| arit '=''=' arit	{ syntno *u = create_no('l', NO_EQ, 2);
							  u->children[0] = $1;
							  u->children[1] = $4;
							  $$ = u;
							}
		| arit '>''=' arit	{ syntno *u = create_no('l', NO_GE, 2);
							  u->children[0] = $1;
							  u->children[1] = $4;
							  $$ = u;
							}
		| arit '<''=' arit	{ syntno *u = create_no('l', NO_LE, 2);
							  u->children[0] = $1;
							  u->children[1] = $4;
							  $$ = u;
							}
		| arit '!''=' arit	{ syntno *u = create_no('l', NO_NE, 2);
							  u->children[0] = $1;
							  u->children[1] = $4;
							  $$ = u;
							}
		;

arit	: arit '+' term		{ syntno *u = create_no('+', NO_ADD, 2);
							  u->children[0] = $1;
							  u->children[1] = $3;
							  $$ = u;
							}
		| arit '-' term		{ syntno *u = create_no('-', NO_SUB, 2);
							  u->children[0] = $1;
							  u->children[1] = $3;
							  $$ = u;
							}
		| term				{ $$ = $1; }
		;

term	: term '*' factor	{ syntno *u = create_no('*', NO_MULT, 2);
							  u->children[0] = $1;
							  u->children[1] = $3;
							  $$ = u;
							}
		| term '/' factor	{ syntno *u = create_no('/', NO_DIV, 2);
							  u->children[0] = $1;
							  u->children[1] = $3;
							  $$ = u;
							}
		| factor			{ $$ = $1; }
		;

factor	: '(' arit ')'		{ /*syntno *u = create_no('p', NO_PAR, 1);
							  u->children[0] = $2;
							  $$ = u;*/
							  $$ = $2;
							}
	| INT					{ syntno *u = create_no('I', NO_TOK, 0);
							  u->token_args = copy_targs($1);
							  $$ = u;
							}
	| DBL					{ syntno *u = create_no('D', NO_TOK, 0);
							  u->token_args = copy_targs($1);
							  $$ = u;
							}
	| VAR					{ syntno *u = create_no('V', NO_TOK, 0);
							  u->token_args = copy_targs($1);
							  $$ = u;
							}
	| unary					{ $$ = $1; }
	| fcall					{ $$ = $1; }
	;

fcall   : IN factor			{ syntno *u = create_no('N', NO_TOK, 1);
							  u->children[0] = $2;
							  $$ = u;
							}
		| VAR '(' ')'		{ syntno *u = create_no('C', NO_TOK, 0);
							  u->token_args = copy_targs($1);
							  $$ = u;
							}
		;

unary	: '-' factor		{ syntno *u = create_no('U', NO_UNA, 1);
							  u->children[0] = $2;
							  $$ = u;
							}
		;

%%

int yyerror(const char *s) {
	printf("%s:%d:%d %s.\n", filename, yylineno, col, s);
	return 1;
}

int yywrap() {
	return 1;
}


int main(int argc, char *argv[]) {

	if (argc > 1) {
		filename = argv[1];
		yyin = fopen(argv[1], "r");
	}

	// invoca o analisador sintático
	yyparse();

	if (yyin)
		fclose(yyin);

	return 0;	

}

void add_symbol(const char *varname, int line, int col) {
	int e = search_symbol(varname);
	if (e == -1) {
		strncpy(synames[sycount].name, varname, 100);
		synames[sycount].line = line;
		synames[sycount].col = col;
		synames[sycount].exists = false; // usado na analise semantica
		synames[sycount].llvm = NULL;
		sycount++;
	}
}

int search_symbol(const char *varname) {
	for(int i = 0; i < sycount; i++) {
		if (strcmp(synames[i].name, varname) == 0)
			return i;
	}
	return -1;
}

void print_symbols() {
	printf("\n\nTabela de simbolos:\n");
	for(int i = 0; i < sycount; i++) {
		printf("%s\t%d\t%d\n", synames[i].name,
			synames[i].line, synames[i].col);
	}
}

syntno *create_no(const char name, enum syntno_type t, short childcount) {
	static short IDCOUNT = 0;

	int s = sizeof(syntno);
	if (childcount > 1)
		s += sizeof(syntno*) * (childcount-1);
	syntno *u = (syntno*)malloc(s);
	u->id = IDCOUNT++;
	u->type = t;
	u->token = name;
	u->childcount = childcount;
	return u;
}

void print_tree_recursiv(syntno *root) {

	if (root->type == NO_TOK || root->type == NO_PRNT) {
		targs *args = root->token_args;
		switch (root->token) {
			case 'V':
				printf("\tN%d[label=\"%s\"];\n", root->id, args->varname);
				break;
			case 'D':
				printf("\tN%d[label=\"%lf\"];\n", root->id, args->constvalue);
				break;
			case 'I':
				printf("\tN%d[label=\"%d\"];\n", root->id, (int)args->constvalue);
				break;
			case 'P':
				printf("\tN%d[label=\"PRINT %s\"];\n", root->id, args->varname);
				break;

		}
	} else {
		printf("\tN%d[label=\"%s\"];\n", root->id, node_names[root->type]);
	}

	for(int i = 0; i < root->childcount; i++) {
		print_tree_recursiv(root->children[i]);
		printf("\tN%d -- N%d;\n", root->id, root->children[i]->id);
	}
}

void print_tree(syntno *root) {
	printf("graph {\n");

	print_tree_recursiv(root);

	printf("}\n");
}

targs *copy_targs(const targs a) {
	targs *r = malloc(sizeof(targs));
	memcpy(r, &a, sizeof(targs));
	//*r = a;
	return r;
}

