
#ifndef HEADER_H
#define HEADER_H

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
	void *token_args;
	short childcount;
	//...
	struct syntno *children[1]; // manter no ultimo campo	
};

typedef struct syntno syntno;

// assinatura de funcoes visitantes
typedef void (*visitor_action)(syntno **root, syntno *no);

// percorre a arvore no sentido esquerda direita raiz
void visitor_leaf_first(syntno **root, visitor_action act);

// visitante que elimina os stmts da arvore
void collapse_stmts(syntno **root, syntno *no);

// visitante que verifica declaracao de variáveis
void declared_vars(syntno **root, syntno *no);

#endif

