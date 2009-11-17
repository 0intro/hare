# basic makefile - yes, this makefile is not written well.  it was 
# a quick hack.  feel free to fix it and contribute it back if it 
# offends you - hacks like this don't bother me.

CXX = c++
CC = cc
CFLAGS = -Wall -I../common -O0
LIBS = $(TAU_LIBS)
LDFLAGS = $(USER_OPT)

all: basic core15 core31 core63

basic:
	cc $(CFLAGS) -Wall ftq.c -o ftq

core15:
	cc $(CFLAGS) -funroll-loops ftq.c -DCORE15 -o ftq15

core31:
	cc $(CFLAGS) -funroll-loops ftq.c -DCORE31 -o ftq31

core63:
	cc $(CFLAGS) -funroll-loops ftq.c -DCORE63 -o ftq63

threads:
	cc $(CFLAGS) -funroll-loops ftq.c -D_WITH_PTHREADS_ -o t_ftq -lpthread
	cc $(CFLAGS) -funroll-loops ftq.c -D_WITH_PTHREADS_ -DCORE15 -o t_ftq15 -lpthread
	cc $(CFLAGS) -funroll-loops ftq.c -D_WITH_PTHREADS_ -DCORE31 -o t_ftq31 -lpthread
	cc $(CFLAGS) -funroll-loops ftq.c -D_WITH_PTHREADS_ -DCORE63 -o t_ftq63 -lpthread

clean:
	rm -f ftq.o ftq ftq15 ftq31 ftq63 t_ftq*
