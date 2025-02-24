#ifndef ALIB4_AUTIL_H_INCLUDED
#define ALIB4_AUTIL_H_INCLUDED
#include <alib-g4/ahandle.h>

#define APCF_black        0
#define APCF_blue         1
#define APCF_green        2
#define APCF_cyan         3
#define APCF_red          4
#define APCF_magenta      5
#define APCF_yellow       6
#define APCF_white        7
#define APCF_gray         8
#define APCF_light_blue   9
#define APCF_light_green  10
#define APCF_light_cyan   11
#define APCF_light_red    12
#define APCF_light_magenta 13
#define APCF_light_yellow 14
#define APCF_light_white 15
#define	APCF_origin ( 1 << 16)
#define	APCF_keep ( 1 << 17)
#define APCF_        APCF_keep

#define APCB_black        (0<<4)
#define APCB_blue         (1<<4)
#define APCB_green        (2<<4)
#define APCB_cyan         (3<<4)
#define APCB_red          (4<<4)
#define APCB_magenta      (5<<4)
#define APCB_yellow       (6<<4)
#define APCB_white        (7<<4)
#define APCB_gray         (8<<4)
#define APCB_light_blue   (9<<4)
#define APCB_light_green  (10<<4)
#define APCB_light_cyan   (11<<4)
#define APCB_light_red    (12<<4)
#define APCB_light_magenta (13<<4)
#define APCB_light_yellow (14<<4)
#define APCB_light_white (15<<4)
#define APCB_origin (1 << 18)
#define APCB_keep (1 << 19)
#define APCB_        APCB_keep

#define AE_SUCCESS 0
#define AE_CANT_FIND 1
#define AE_EMPTY_DATA 2

/**Color Macro Examples:
*	ALIB4_COLOR_BACK(red) ==> APCB_red
*	ALIB4_COLOR(,red) ==> (APCF_ | APCB_red) ==> (APCF_origin | APCB_red)
*You can even use the macro like this:
*	ALIB4_COLOR(,) ===> ( APCF_origin | APCB_origin )
*/
#define ALIB4_COLOR_FRONT(C) (APCF_##C)
#define ALIB4_COLOR_BACK(C) (APCB_##C)
#define ALIB4_COLOR(FRONT,BACK) (APCF_##FRONT | APCF_##BACK)

#ifdef _WIN32
typedef __int64 amem_bytes;
#elif __linux__
typedef __int64_t amem_bytes;
#endif // _WIN32

extern "C"{
/** \brief Program Memory 程序使用内存**/
typedef struct ALIB4_API {
    amem_bytes memory;
    amem_bytes virtualMemory;
} ALIB4ProgramMemUsage;

/** \brief Global Memory Usage 全局内存使用情况**/
typedef struct ALIB4_API {
    ///In linux,percent = phyUsed / phyTotal
    unsigned int percent;
    amem_bytes physicalTotal;
    amem_bytes virtualTotal;
    amem_bytes physicalUsed;
    amem_bytes virtualUsed;
    amem_bytes pageTotal;
    amem_bytes pageUsed;
} aGlobalMemUsage;

/** \brief GetCPUInfo 获取CPU信息**/
typedef struct ALIB4_API {
    AStrHandle CpuID;
} aCPUInfo;

typedef struct ALIB4_API{
    int code;
    const char * content;
} aError;

ALIB4_API void asetLastError(int code,const char * content);
ALIB4_API aError agetLastError();
ALIB4_API aError aclearLastError();

}

#endif
