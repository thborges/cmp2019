LLVMFLAGS=$(shell llvm-config --cxxflags)
LLVMLIBS=$(shell llvm-config --ldflags --libs engine) -lncurses

all:
	flex --yylineno scalc.l
	bison -d scalc.y
	g++ -ggdb -O0 -c -std=c++11 backllvm.cpp -o backllvm.o
	gcc -c lex.yy.c scalc.tab.c semantic.c 
	g++ lex.yy.o scalc.tab.o semantic.o backllvm.o ${LLVMLIBS} -o scalc

test:
	./scalc teste02.txt

stdc.o : stdc.c
	gcc -c stdc.c

%.ll : %.txt
	./scalc $< > $@

%.s : %.ll
	llc $< -o $@ -filetype=asm

% : %.s stdc.o
	gcc -o $@ $< stdc.o

#.SILENT:

