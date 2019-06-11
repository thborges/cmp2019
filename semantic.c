
#include <stdio.h>
#include <stdlib.h>
#include "header.h"

void visitor_leaf_first(syntno **root, visitor_action act) {
	syntno *r = *root;
	for(int i = 0; i < r->childcount; i++) {
		visitor_leaf_first(&r->children[i], act);
		if (act)
			act(root, r->children[i]);
	}
}

void collapse_stmts(syntno **root, syntno *no) {
	syntno *or = *root;
	if (or->type == NO_STMTS && no->type == NO_STMTS) {
		//printf("%d: P%d\n", no->id, or->id);
		int nsize = sizeof(syntno);
		nsize += sizeof(syntno*) * (or->childcount-1);
		nsize += sizeof(syntno*) * (no->childcount-1);

		// aloca mais espaco para os novos filhos
		or = *root = realloc(*root, nsize);

		or->childcount--;
		for(int i = 0; i < no->childcount; i++) {
//			or->children[or->childcount++] =
//				no->children[i];
			or->children[or->childcount] =
				no->children[i];
			or->childcount++;
		}

		free(no);
	}
}

void declared_vars(syntno **root, syntno *no) {
	// se o pai não é NO_ATTR e o no é do tipo 
	// NO_TOK e 'V', verifica a existencia na tabela de símbolos
	syntno *nr = *root;

	if (nr->type != NO_ATTR && no->type == NO_TOK && no->token == 'V') {
		targs *args = no->token_args;
		int s = search_symbol(args->varname);
		if (s == -1 || !synames[s].exists) {
			printf(":%d:%d: identifier %s undeclared.\n", 
				args->line, args->col, args->varname);
		}
	}

	// se o nó é NO_ATTR, marca a variável como declarada
	if (no->type == NO_ATTR) {
		targs *args = no->children[0]->token_args;
		int s = search_symbol(args->varname);
		if (s != -1)
			synames[s].exists = true;
	}
}

