// malloc, calloc etc. are in stdlib
#include <stdlib.h>

// va_list, etc. are in stdarg
#include <stdarg.h>

// Shim alloca to calloc since it isn't currently implemented, and PhysFS hooks the other two allocation functions
#define alloca(x) allocator.Malloc(x)
