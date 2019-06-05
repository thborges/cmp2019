all:
	flex --yylineno scalc.l
	bison -d scalc.y
	gcc lex.yy.c scalc.tab.c semantic.c -o scalc

test:
	./scalc teste02.txt
