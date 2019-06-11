
#ifndef HEADER_H
#define HEADER_H

#include <stdbool.h>

typedef struct {
	int line;
	int col;
	char *varname;
	double constvalue;
} targs;

/* no para a arvore sintatica
 *
 */

enum syntno_type { NO_ADD=0, NO_SUB, NO_MULT, NO_DIV, NO_PAR, NO_STMTS, NO_STMT, NO_UNA,
  NO_ATTR, NO_TOK, NO_PRNT};

struct syntno {
	short id;
	enum syntno_type type;
	char token;
	targs *token_args;
	short childcount;
	//...
	struct syntno *children[1]; // manter no ultimo campo	
};

typedef struct syntno syntno;

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
	int exists;
	void *llvm;
} symbol;

symbol synames[100];
void add_symbol(const char *varname, int line, int col);
int  search_symbol(const char *varname);
void print_symbols();

// assinatura de funcoes visitantes
typedef void (*visitor_action)(syntno **root, syntno *no);

// percorre a arvore no sentido esquerda direita raiz
void visitor_leaf_first(syntno **root, visitor_action act);

// visitante que elimina os stmts da arvore
void collapse_stmts(syntno **root, syntno *no);

// visitante que verifica declaracao de variáveis
void declared_vars(syntno **root, syntno *no);

// gera nos de LLVM IR
void setup_llvm_global();
void print_llvm_ir();
void generate_llvm_node(syntno **root, syntno *no);

#endif

