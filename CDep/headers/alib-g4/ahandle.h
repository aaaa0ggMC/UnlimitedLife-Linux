#ifndef ALIB4_AHANDLE_H_INCLUDED
#define ALIB4_AHANDLE_H_INCLUDED
#include <stdlib.h>

///For windows support
#ifdef _WIN32
#include <windows.h>
#ifndef ALIB4_API
#ifdef ALIB4_BUILD_DLL
    #define ALIB4_API __declspec(dllexport)
#else
    #define ALIB4_API __declspec(dllimport)
#endif
#endif // BUILD_DLL

#elif __linux__

#include <unistd.h>
#ifndef ALIB4_API
#ifdef ALIB4_BUILD_DLL
    #define ALIB4_API
#else
    #define ALIB4_API
#endif
#endif
#endif

typedef unsigned long long AHandle;
typedef AHandle AStrHandle ;

#define AINVALID_HANDLE (AHandle)(0)

extern "C"{
    ALIB4_API AStrHandle astr_allocate(const char * data);
    ALIB4_API const char* astr_add(AStrHandle a,AStrHandle b);
    ALIB4_API const char* astr_add_ptr(AStrHandle a,const char * b);
    ALIB4_API size_t astr_length(AStrHandle a);
    ALIB4_API AStrHandle astr_substr(AStrHandle a,size_t beg,size_t length);
    ALIB4_API int astr_free(AStrHandle handle);
    ALIB4_API const char * astr_get(AStrHandle handle);
}

#endif
