#ifndef AGE_BASE
#define AGE_BASE

///For windows support
#ifdef _WIN32
#include <windows.h>
#ifndef AGE_API
#ifdef AGE_BUILD_DLL
    #define AGE_API __declspec(dllexport)
#else
    #define AGE_API __declspec(dllimport)
#endif
#endif // BUILD_DLL

#elif __linux__

#include <unistd.h>
#ifndef AGE_API
#ifdef AGE_BUILD_DLL
    #define AGE_API
#else
    #define AGE_API
#endif
#endif
#endif

#endif
