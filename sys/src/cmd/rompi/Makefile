CC=/bgsys/drivers/ppcfloor/gnu-linux/bin/powerpc-bgp-linux-gcc
CLIB=rompi.c torus.c
CFLAGS=-Dcnk -Wall
ALL=rompihello stupidreduce testmalloc reduce testpv testring
all: $(ALL)

reduce: reduce.o rompilib.a
	$(CC) -I . -o reduce reduce.o rompilib.a
	cp reduce ~/rompi

testring: testring.o rompilib.a
	$(CC) -I . -o testring testring.o rompilib.a
	cp testring ~/rompi

stupidreduce: stupidreduce.o rompilib.a
	$(CC) -I . -o stupidreduce stupidreduce.o rompilib.a
	cp stupidreduce ~/rompi

rompihello: rompihello.o rompilib.a
	$(CC) -I . -o rompihello rompihello.o rompilib.a
	cp rompihello ~/rompi

testpv: testpv.o 
	$(CC) -I . -o testpv testpv.o 
	cp testpv ~/rompi

testmalloc: testmalloc.o rompilib.a
	$(CC) -I . -o testmalloc testmalloc.o rompilib.a
	cp testmalloc ~/rompi

rompilib.a: rompi.o torus.o linux.o
	ar r rompilib.a rompi.o torus.o linux.o

clean: 
	rm -f *.a *.o   $(ALL) tags
#
##
##		programs
##
#all: ${TARG}
#
#TARG=\
#	rompihello\
#	ring \
#	stupidreduce \
#	reduce \
#
#DIRS=\
#
#OTHEROFILES=\
#	torus.o\
#	rompi.o\
#	support.o\
#
#HFILES=mpi.h
#
#LIB=lib.a
#BIN=.
#
##UPDATE=\
##	mkfile\
##	$HFILES\
##	${OTHEROFILES}\
##	${TARGc}\
#
#clean:
#	rm -f *.[oS] *.[$OS].a [$OS].* y.tab.? y.debug y.output $TARG
#
#nuke:
#	rm -f *.[oS] *.[$OS].a [$OS].* y.tab.? y.debug y.output $TARG *.acid
#
#o.o: lib.a
#
#${LIB}: ${OTHEROFILES}
#	ar r $LIB oTHEROFILES
#
#
