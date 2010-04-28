CC=/bgsys/drivers/ppcfloor/gnu-linux/bin/powerpc-bgp-linux-gcc
all: mympihello

mympihello: mympihello.o mympilib.a
	$(CC) -I . -o mympihello mympihello.o mympilib.a
	cp mympihello ~/emu

mympilib.a: torus.o mympi.o
	ar r mympilib.a torus.o mympi.o

clean: 
	rm *.a *.o 
