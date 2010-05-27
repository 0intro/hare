
/* 
8c send-cmd.c &&
8l -o send-cmd send-cmd.8 &&
cp send-cmd $home/bin/386 &&
8.out
*/
#include "send-cmd.h"

void
main(int argc, char *argv[])
{
	char buf[1024];
	int i;
	
	if ( argc <= 0) {
		print ("not enough arguments\n");
		exits(0);
	}
	strcpy (buf, argv[1]);
	for (i = 2 ; i < argc ; ++i ) {
		strcat (buf, " ");
		strcat (buf, argv[i]);
	}
	execute_cmd (buf);
	exits(0);
}