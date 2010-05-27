
/* 
8c start-res.c &&
8l -o start-res start-res.8 &&
cp start-res $home/bin/386 &&
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
	strcpy (buf, "wrapnompirun");
	for (i = 1 ; i < argc ; ++i ) {
		strcat (buf, " ");
		strcat (buf, argv[i]);
	}
	execute_cmd (buf);
	exits(0);
}
