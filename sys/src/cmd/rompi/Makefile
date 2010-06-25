CC=/bgsys/drivers/ppcfloor/gnu-linux/bin/powerpc-bgp-linux-gcc
CLIB=rompi.c torus.c
CFLAGS=-Dcnk
all: rompihello

rompihello: rompihello.o rompilib.a
	$(CC) -I . -o rompihello rompihello.o rompilib.a
	cp rompihello ~/emu

rompilib.a: rompi.o torus.o linux.o
	ar r rompilib.a rompi.o torus.o linux.o

clean: 
	rm *.a *.o 
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
