
/*
 * dump p==v mode. 
 * forget going to stdout for now -- too much noise from machcnk. 
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main (int argc, char **argv)
{
	unsigned long *base = (void *)0x20000000;
	int amt=2048;
	int i, j;

	if (argc > 1) base = (void *)strtoul(argv[1], 0, 0);
	if (argc > 2) amt = strtoul(argv[2], 0, 0);

#if 0
	/* kernel does not know about p==v dammit */
	for(i = 0; i < amt; i++) {
		unsigned long x;
		x = base[i];
		write(1, &x, 4);
	}
#endif
	for(i = 0; i < amt; i += 8) {
		printf("%p:", &base[i]);
		for(j = 0; j < 8; j++) {
			printf(" %08lx", base[i+j]);
		}
		printf("\n");
	}

	return 1;
}

