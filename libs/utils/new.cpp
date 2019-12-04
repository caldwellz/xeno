#ifdef NXDK
#include "new"

//Copied from the OSDev.org wiki page on C++

void *operator new(size_t size)
{
    return malloc(size);
}
 
void *operator new[](size_t size)
{
    return malloc(size);
}
 
void operator delete(void *p)
{
    free(p);
}
 
void operator delete[](void *p)
{
    free(p);
}

#endif
