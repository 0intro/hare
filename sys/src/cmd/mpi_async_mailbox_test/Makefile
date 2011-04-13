CC=/bgsys/drivers/ppcfloor/gnu-linux/bin/powerpc-bgp-linux-g++
CXXFLAGS += -I . -I ../src/boost_1_45_0/~/boostsucks/include
ALL=plan9
all: $(ALL)

plan9: plan9.o 
	cp ~/hare/sys/src/cmd/rompi/rompilib.a .
	$(CC) -o plan9 plan9.o rompilib.a
	cp plan9 ~/rompi

clean: 
	rm -f *.a *.o   $(ALL) tags
