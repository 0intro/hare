
/*
 * test p==v mode
 */
#include <stdio.h>
#include <stdlib.h>

void 
main (int argc, char **argv)
{
	unsigned char *c = malloc(1024);

	printf("c is %p\n", c);
	if (argc > 1)
		sprintf(c, "HI THERE!\n");
	else {
		sleep(5);
		printf(":%s:\n", c);
	}
	while (1);
}


