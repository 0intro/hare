</$objtype/mkfile

TARG=ftq15\
	ftq31\
	ftq63\

default:V: all

OFILES=getticks.$O

CFLAGS=-DPlan9

BIN=/$home/bin/$objtype
</sys/src/cmd/mkmany

ftq15.$O: getticks.$O ftq.c
	$O^c -c $CFLAGS -DCORE15 -o ftq15.$O ftq.c
ftq31.$O: getticks.$O ftq.c
	$O^c -c $CFLAGS -DCORE31 -o ftq31.$O ftq.c
ftq63.$O: getticks.$O ftq.c
	$O^c -c $CFLAGS -DCORE63 -o ftq63.$O ftq.c

