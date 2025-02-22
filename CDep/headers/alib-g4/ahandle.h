#ifndef ALIB4_AHANDLE_H_INCLUDED
#define ALIB4_AHANDLE_H_INCLUDED
#include <stdlib.h>

typedef unsigned long long AHandle;
typedef AHandle AStrHandle ;

#define AINVALID_HANDLE (AHandle)(0)

extern "C"{
    AStrHandle aallocateString(const char * data);
    const char* astr_add_handles(AStrHandle a,AStrHandle b);
    const char* astr_add_ptr(AStrHandle a,const char * b);
    size_t astr_length(AStrHandle a);
    AStrHandle astr_substr(AStrHandle a,size_t beg,size_t length);
    int afreeString(AStrHandle handle);
    const char * agetString(AStrHandle handle);
}

#endif
