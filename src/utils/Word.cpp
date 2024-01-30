#include "Word.h"

std::string Format (const char* format, ...)
{
	va_list va;
	va_start( va, format );
	return VFormat (format, va);
}

std::string VFormat (const char* format, va_list ap)
{
	char buffer[1024 * 10];
	vsnprintf (buffer, 1024 * 10, format, ap);
	return buffer;
}
