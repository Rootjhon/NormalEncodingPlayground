#pragma once

#include <string>
#include <stdarg.h>

std::string Format (const char* format, ...);
std::string VFormat (const char* format, va_list ap);
