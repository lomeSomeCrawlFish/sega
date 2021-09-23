#include "error.h"

#include <stdarg.h>
#include <stdio.h>

void PrintError(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fputs("Error: ", stderr);
	vfprintf(stderr, fmt, args);
	fputc('\n', stderr);
	va_end(args);
}