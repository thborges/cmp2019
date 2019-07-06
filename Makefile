LLVMFLAGS=$(shell llvm-config --cxxflags)
LLVMLIBS=$(shell llvm-config --ldflags --libs engine) -lncurses

ARDUINOFLAGS=-fdata-sections -ffunction-sections -Wl,--gc-sections
ARDUINO=atmega328p

all:
	flex --yylineno scalc.l
	bison -d scalc.y
	g++ -ggdb -O0 -c -std=c++11 backarduino.cpp backllvm.cpp ${LLVMFLAGS}
	gcc -c lex.yy.c scalc.tab.c semantic.c 
	g++ lex.yy.o scalc.tab.o semantic.o backllvm.o backarduino.o ${LLVMLIBS} -o scalc

test:
	./scalc teste02.txt

stdc.o : stdc.c
	gcc -c stdc.c

%.ll : %.txt
	./scalc $< > $@

%.s : %.ll
	llc $< -o $@ -filetype=asm

%.avr.s : %.ll
	llc $< -o $@ -march=avr -mcpu=${ARDUINO} -filetype=asm

% : %.s stdc.o
	gcc -o $@ $< stdc.o

%.avr.o : %.avr.s arduinowire/core.a
	avr-gcc -Os ${ARDUINOFLAGS} -mmcu=${ARDUINO} $< arduinowire/core.a -o $@

%.hex : %.avr.o
	avr-objcopy -O ihex $< $@

%.up : %.hex
	avrdude -C ~/avrdude.conf -P /dev/tty.usbserial -V -carduino -p${ARDUINO} -b57600 -U flash:w:$<:i

zip:
	zip calc.zip backllvm.cpp backarduino.cpp stdc.c semantic.c scalc.l scalc.y *.txt header.h Makefile 

clean:
	rm -f *.o *.s *.hex *.ll

#.SILENT:
