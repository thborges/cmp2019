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

/* tabela de simbolos
 * maximo 100 simbolos
 * nome simbolo maximo 10 caracteres
 * nao otimizada. Deveria usar hash ou pelo menos
 * busca binaria.
 */
typedef struct {
	char name[11];
	int line;
	int col;
} symbol;


symbol synames[100];
int sycount = 0;

void add_symbol(const char *varname, int line, int col);
int  search_symbol(const char *varname);
void print_symbols();
syntno *create_no(const char name, enum syntno_type t, short children); 
void print_tree(syntno *root);
targs *copy_targs(const targs a);

%}

%union {
	targs args;
	struct syntno *no;
}

%define parse.error verbose

%token PRINT 
%token <args> INT DBL
%token <args> VAR

%type <no> stmts stmt arit term factor unary print

%start program

%%

program : stmts	{ print_symbols();
				  visitor_leaf_first(&($1), collapse_stmts);
				  print_tree($1);
				}
		;

stmts	: stmt ';' stmts	{ syntno *u = create_no('S', NO_STMTS, 2);
							  u->children[0] = $1;
							  u->children[1] = $3;
							  $$ = u;
							}
		| stmt ';'			{ syntno *u = create_no('s', NO_STMT, 1);
							  u->children[0] = $1;
							  $$ = u;
							}
		;


stmt	: VAR '=' arit		{ add_symbol($1.varname, $1.line, $1.col); 
							  syntno *u = create_no('A', NO_ATTR, 2);

							  syntno *uvar = create_no('V', NO_TOK, 0);
							  uvar->token_args = copy_targs($1);
							  u->children[0] = uvar;

							  u->children[1] = $3;
							  $$ = u;
							} 
		| print				{ $$ = $1; }
		| error				{ }
		;

print	: PRINT VAR			{ syntno *u = create_no('P', NO_PRNT, 0);
							  u->token_args = copy_targs($2);
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
	;

unary	: '-' factor		{ syntno *u = create_no('-', NO_UNA, 1);
							  u->children[0] = $2;
							  $$ = u;
							}
		;

%%

int yyerror(const char *s) {
	printf("teste02.txt:%d:%d %s.\n", yylineno, col, s);
	return 1;
}

int yywrap() {
	return 1;
}

int main(int argc, char *argv[]) {

	if (argc > 1) {
//		filename = argv[1];
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
		strncpy(synames[sycount].name, varname, 10);
		synames[sycount].line = line;
		synames[sycount].col = col;
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

	// NO_ADD=0, NO_SUB, NO_MULT, NO_DIV, NO_PAR, 
 	// NO_STMTS, NO_STMT, NO_UNA, NO_ATTR, NO_TOK, NO_PRNT
	const char *node_names[] = {
		"NO_ADD", "NO_SUB", "NO_MULT", "NO_DIV", "NO_PAR", 
 		"NO_STMTS", "NO_STMT", "NO_UNA", "NO_ATTR", "NO_TOK", "NO_PRNT"};

//	if (root->type == NO_TOK || root->type == NO_PRNT || root->type == NO_ATTR) {
//	}
//	else {
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
//	}
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

