#include "rompi.h"

void print(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
}

void fprint(int fd, const char *fmt, ...)
{
	char *s;
	va_list ap;
	va_start(ap, fmt);
	s = smprint(fmt, ap);
	write(fd, s, strlen(s));
	free(s);
}


char *smprint(const char *fmt, ...)
{
	char *str;
	va_list ap;
	va_start(ap, fmt);
	str = malloc(1024);
	vsnprintf(str, 1024, fmt, ap);
	return str;
}

void *mallocz(size_t size, int zero)
{
	char *cp = malloc(size);
	if (zero)
		memset(cp, 0, size);
	return cp;
}

void exits(char *s)
{
	if (s == NULL) 
		exit(0);
	fprint(2, s);
	exit(1);
}

