LLVMFLAGS=$(shell llvm-config-7 --cxxflags)
LLVMLIBS=$(shell llvm-config-7 --ldflags --libs) -lncurses -lpthread

ARDUINOFLAGS=-fdata-sections -ffunction-sections -Wl,--gc-sections
ARDUINO=atmega328p
USBPORT=/dev/ttyUSB0

all:
	flex --yylineno scalc.l
	bison -d scalc.y
	g++ -ggdb -O0 -c -std=c++11 backarduino.cpp backllvm.cpp ${LLVMFLAGS}
	gcc -ggdb -O0 -c lex.yy.c scalc.tab.c semantic.c 
	g++ -ggdb -O0 lex.yy.o scalc.tab.o semantic.o backllvm.o backarduino.o ${LLVMLIBS} -o scalc

test:
	./scalc teste02.txt

stdc.o : stdc.c
	gcc -c stdc.c

%.ll : %.txt scalc
	./scalc $< > $@

%.s : %.ll
	llc $< -o $@ -filetype=asm

%.avr.s : %.ll
	llc $< -o $@ -march=avr -mcpu=${ARDUINO} -filetype=asm

% : %.s stdc.o
	gcc -o $@ $< stdc.o

stdc_avr.o : stdc_avr.cpp
	avr-g++ -Os ${ARDUINOFLAGS} -mmcu=${ARDUINO} -Iarduinowire -c $< -o $@

%.avr.o : %.avr.s arduinowire/core.a stdc_avr.o
	avr-g++ -Os ${ARDUINOFLAGS} -mmcu=${ARDUINO} $< stdc_avr.o arduinowire/core.a -o $@

%.hex : %.avr.o
	avr-objcopy -O ihex $< $@

%.up : %.hex
	avrdude -C ~/avrdude.conf -P ${USBPORT} -V -carduino -p${ARDUINO} -b57600 -U flash:w:$<:i

zip:
	zip calc.zip arduinowire/*.{cpp,c,h} *.{txt,h,l,y,cpp} Makefile 

clean:
	rm -f *.o *.s *.hex *.ll

#.SILENT:
