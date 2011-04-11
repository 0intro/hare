CC=/bgsys/drivers/ppcfloor/gnu-linux/bin/powerpc-bgp-linux-gcc
LIB=rompi.o torus.o amring.o linux.o
CFLAGS=-Dcnk -Wall -O0
ALL=rompihello stupidreduce testmalloc reduce dumppv testpv testring timesyscall testfastinject hello testamring testamlib
all: $(ALL)

reduce: reduce.o rompilib.a
	$(CC) -I . -o reduce reduce.o rompilib.a
	cp reduce ~/rompi

hello: hello.o rompilib.a
	$(CC) -I . -o hello hello.o rompilib.a
	cp hello ~/rompi

timesyscall: timesyscall.o rompilib.a
	$(CC) -I . -o timesyscall timesyscall.o rompilib.a
	cp timesyscall ~/rompi

testamring: testamring.o rompilib.a
	$(CC) -I . -o testamring testamring.o rompilib.a
	cp testamring ~/rompi

testamlib: testamlib.o rompilib.a
	$(CC) -I . -o testamlib testamlib.o rompilib.a
	cp testamlib ~/rompi

testfastinject: testfastinject.o rompilib.a
	$(CC) -I . -o testfastinject testfastinject.o rompilib.a
	cp testfastinject ~/rompi

testring: testring.o rompilib.a
	$(CC) -I . -o testring testring.o rompilib.a
	cp testring ~/rompi

stupidreduce: stupidreduce.o rompilib.a
	$(CC) -I . -o stupidreduce stupidreduce.o rompilib.a
	cp stupidreduce ~/rompi

rompihello: rompihello.o rompilib.a
	$(CC) -I . -o rompihello rompihello.o rompilib.a
	cp rompihello ~/rompi

dumppv: dumppv.o 
	$(CC) -I . -o dumppv dumppv.o 
	cp dumppv ~/rompi

testpv: testpv.o 
	$(CC) -I . -o testpv testpv.o 
	cp testpv ~/rompi

testmalloc: testmalloc.o rompilib.a
	$(CC) -I . -o testmalloc testmalloc.o rompilib.a
	cp testmalloc ~/rompi

rompilib.a: $(LIB)
	ar r rompilib.a $(LIB)

clean: 
	rm -f *.a *.o   $(ALL) tags
