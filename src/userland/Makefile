AS = nasm
CC = gcc
LD = ld

CC_FLAGS = -c -m32 -nostdlib -nodefaultlibs -nostartfiles -static -s -fno-ident -fno-pic -fno-asynchronous-unwind-tables -I include/

LD_FLAGS = --omagic -T src/scaffold.ld -m i386linux -L lib -l:syscall.o -l:start.o

lib/syscall.o: src/syscall.asm include/syscall.h
	${AS} -f elf -o lib/syscall.o src/syscall.asm

lib/start.o: src/start.c include/syscall.h
	${CC} ${CC_FLAGS} -o lib/start.o src/start.c

tests/build/hello.o: tests/src/hello.c
	${CC} ${CC_FLAGS} -o tests/build/hello.o tests/src/hello.c

tests/build/hello: tests/build/hello.o lib/syscall.o lib/start.o
	${LD} ${LD_FLAGS} -o tests/build/hello tests/build/hello.o	
	echo aaaaa >> tests/build/hello


.PHONY: clean
clean:
	rm lib/*
	rm tests/build/*

# vim: noexpandtab
