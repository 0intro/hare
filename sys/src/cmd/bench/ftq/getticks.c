#include <u.h>
#include <libc.h>

uvlong getticks(void)
{
	uvlong          t;

	cycles(&t);
	return t;
}
