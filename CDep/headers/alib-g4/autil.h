#ifndef ALIB4_AUTIL_H_INCLUDED
#define ALIB4_AUTIL_H_INCLUDED
#include <alib-g4/ahandle.h>
#include <stdbool.h>

//=============== 基础样式 ================
#define ACP_RESET       "\e[0m"   // 重置所有样式
#define ACP_BOLD        "\e[1m"   // 加粗
#define ACP_DIM         "\e[2m"   // 暗化
#define ACP_ITALIC      "\e[3m"   // 斜体
#define ACP_UNDERLINE   "\e[4m"   // 下划线
#define ACP_BLINK       "\e[5m"   // 慢闪烁
#define ACP_BLINK_FAST  "\e[6m"   // 快闪烁
#define ACP_REVERSE     "\e[7m"   // 反色（前景/背景交换）
#define ACP_HIDDEN      "\e[8m"   // 隐藏文字

//=============== 标准前景色 ================
#define ACP_BLACK     "\e[30m"
#define ACP_RED       "\e[31m"
#define ACP_GREEN     "\e[32m"
#define ACP_YELLOW    "\e[33m"
#define ACP_BLUE      "\e[34m"
#define ACP_MAGENTA   "\e[35m"
#define ACP_CYAN      "\e[36m"
#define ACP_WHITE     "\e[37m"

//=============== 标准背景色 ================
#define ACP_BG_BLACK   "\e[40m"
#define ACP_BG_RED     "\e[41m"
#define ACP_BG_GREEN   "\e[42m"
#define ACP_BG_YELLOW  "\e[43m"
#define ACP_BG_BLUE    "\e[44m"
#define ACP_BG_MAGENTA "\e[45m"
#define ACP_BG_CYAN    "\e[46m"
#define ACP_BG_WHITE   "\e[47m"

//=============== 亮色模式 ================
// 前景亮色（如高亮红/绿等）
#define ACP_LRED     "\e[91m"
#define ACP_LGREEN   "\e[92m"
#define ACP_LYELLOW  "\e[93m"
#define ACP_LBLUE    "\e[94m"
#define ACP_LMAGENTA "\e[95m"
#define ACP_LCYAN    "\e[96m"
#define ACP_LWHITE   "\e[97m"

// 背景亮色
#define ACP_BG_LRED     "\e[101m"
#define ACP_BG_LGREEN   "\e[102m"
#define ACP_BG_LYELLOW  "\e[103m"
#define ACP_BG_LBLUE    "\e[104m"
#define ACP_BG_LMAGENTA "\e[105m"
#define ACP_BG_LCYAN    "\e[106m"
#define ACP_BG_LWHITE   "\e[107m"

//=============== 256色模式 ================
/* 用法示例：
   ACP_FG256(160)  // 设置前景色为256色编号160
   ACP_BG256(231)  // 设置背景色为256色编号231 */
#define ACP_FG256(n) "\e[38;5;" #n "m"
#define ACP_BG256(n) "\e[48;5;" #n "m"

//=============== RGB真彩色模式 ================
/* 用法示例：
   ACP_FGRGB(255,0,100)  // 前景RGB颜色
   ACP_BGRGB(50,100,200) // 背景RGB颜色 */
#define ACP_FGRGB(r,g,b) "\e[38;2;" #r ";" #g ";" #b "m"
#define ACP_BGRGB(r,g,b) "\e[48;2;" #r ";" #g ";" #b "m"

#define AE_SUCCESS 0
#define AE_FAILED -1
#define AE_CANT_FIND -2
#define AE_EMPTY_DATA -3
#define AE_IO -4

#ifdef _WIN32
typedef __int64 amem_bytes;
#elif __linux__
typedef __int64_t amem_bytes;
#endif // _WIN32

typedef void*(*aErrorCallbackFn)(int code,const char * content,void * reserved);

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
ALIB4_API void aclearLastError();
ALIB4_API void asetLastErrorf(int code,const char * fmt,...);
ALIB4_API void aaddOnErrorCallback(aErrorCallbackFn fn,void * reserved);
//just simply output the data to the console
ALIB4_API void* adefaultErrorCallback(int,const char *,void*);
ALIB4_API void aenableVirtualTerminal();
ALIB4_API long aio_fileSize(const char * filepath);
ALIB4_API long aio_readAll(const char * filename,AStrHandle appender);
ALIB4_API const char * asafe(const char *);
ALIB4_API long aio_writeAll(const char * fp,const char * data,long length);
ALIB4_API _Bool aio_checkExistence(const char * fp);
ALIB4_API const char * agetTime();
ALIB4_API const char * aformatDuration(int seconds);

}

#endif
