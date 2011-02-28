CC=/bgsys/drivers/ppcfloor/gnu-linux/bin/powerpc-bgp-linux-gcc
LIB=../../rompi/rompilib.a
ALL=
all: $(ALL)

reduce: reduce.o rompilib.a
	$(CC) -I . -o reduce reduce.o rompilib.a
	cp reduce ~/rompi

