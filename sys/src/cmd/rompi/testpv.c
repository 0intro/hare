
/*
 * test p==v mode
 */
#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char **argv)
{
	unsigned char *c = (void *)0x20000000; //malloc(1024);
	unsigned char *b = malloc(1024);

	printf("b is %p\n", b);
	printf("c is %p\n", c);
	if (argc > 1)
		sprintf((char *)c, "HI THERE!\n");
	else {
		sleep(5);
		printf(":%s:\n", c);
	}
	while (1);
}


