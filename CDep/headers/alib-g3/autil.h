#ifndef AAAA_UTIL_H_INCLUDED
#define AAAA_UTIL_H_INCLUDED
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#ifndef ALIB_DISABLE_CPP20
#include <format>
#endif
namespace fs = std::filesystem;

///Platform Related
#ifdef _WIN32
#define THREAD_LOCAL __declspec(thread)
#include <windows.h>
#ifndef DLL_EXPORT
#ifdef BUILD_DLL
    #define DLL_EXPORT __declspec(dllexport)
#else
    #define DLL_EXPORT __declspec(dllimport)
#endif
#endif // BUILD_DLL
#elif __linux__
#define THREAD_LOCAL thread_local
#include <unistd.h>
#ifndef DLL_EXPORT
#ifdef BUILD_DLL
    #define DLL_EXPORT
#else
    #define DLL_EXPORT
#endif
#endif // BUILD_DLL
#endif

///Colors 使用了DeepSeek生成
#ifndef __ALIB_CONSOLE_COLORS
#define __ALIB_CONSOLE_COLORS
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
#define ACP_GRAY      "\e[36m"
#define ACP_WHITE     "\e[37m"

//=============== 标准背景色 ================
#define ACP_BG_BLACK   "\e[40m"
#define ACP_BG_RED     "\e[41m"
#define ACP_BG_GREEN   "\e[42m"
#define ACP_BG_YELLOW  "\e[43m"
#define ACP_BG_BLUE    "\e[44m"
#define ACP_BG_MAGENTA "\e[45m"
#define ACP_BG_CYAN    "\e[46m"
#define ACP_BG_GRAY    "\e[46m"
#define ACP_BG_WHITE   "\e[47m"

//=============== 亮色模式 ================
// 前景亮色（如高亮红/绿等）
#define ACP_LRED     "\e[91m"
#define ACP_LGREEN   "\e[92m"
#define ACP_LYELLOW  "\e[93m"
#define ACP_LBLUE    "\e[94m"
#define ACP_LMAGENTA "\e[95m"
#define ACP_LCYAN    "\e[96m"
#define ACP_LGRAY    "\e[96m"
#define ACP_LWHITE   "\e[97m"

// 背景亮色
#define ACP_BG_LRED     "\e[101m"
#define ACP_BG_LGREEN   "\e[102m"
#define ACP_BG_LYELLOW  "\e[103m"
#define ACP_BG_LBLUE    "\e[104m"
#define ACP_BG_LMAGENTA "\e[105m"
#define ACP_BG_LCYAN    "\e[106m"
#define ACP_BG_LGRAY    "\e[106m"
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
#endif

#ifndef ALIB_TO_STRING_RESERVE_SIZE
#define ALIB_TO_STRING_RESERVE_SIZE 128   
#endif // ALIB_TO_STRING_RESERVE_SIZE


///Errcodes
#define AE_SUCCESS 0
#define AE_FAILED -1

namespace alib {
namespace g3 {

using dstring = const std::string&;
#ifdef _WIN32
using mem_bytes = __int64;
#elif __linux__
using mem_bytes = __int64_t;
#endif // _WIN32

namespace ext_toString{
    #ifndef ALIB_DISABLE_CPP20
    inline thread_local std::string fmtBuf;
    [[maybe_unused]] inline thread_local bool inited = []()->bool{
        fmtBuf.reserve(ALIB_TO_STRING_RESERVE_SIZE);
        return true;
    }();
    #endif // ALIB_DISABLE_CPP20

    inline const std::string& toString(const std::string& v){
        return v;
    }
    template<class T> auto toString(const T& v){
        #ifndef ALIB_DISABLE_CPP20
        fmtBuf.clear();
        std::format_to(std::back_inserter(fmtBuf),"{}",v);
        fmtBuf.append("\0");
        return (const std::string&)fmtBuf;
        #else
            if constexpr(std::is_same<T,const char *>::value)return std::string(v);
            else if constexpr(std::is_same<T,char *>::value)return std::string(v);
            else if constexpr (std::is_same_v<std::remove_extent_t<T>, char> && std::is_array_v<T>)return std::string(v);
            else return std::to_string(v);
        #endif // ALIB_DISABLE_CPP20
    }
}

/** \brief Program Memory 程序使用内存**/
struct DLL_EXPORT ProgramMemUsage {
    mem_bytes memory;
    mem_bytes virtualMemory;
};
/** \brief Global Memory Usage 全局内存使用情况**/
struct DLL_EXPORT GlobalMemUsage {
    ///In linux,percent = phyUsed / phyTotal
    unsigned int percent;
    mem_bytes physicalTotal;
    mem_bytes virtualTotal;
    mem_bytes physicalUsed;
    mem_bytes virtualUsed;
    mem_bytes pageTotal;
    mem_bytes pageUsed;
};

/** \brief GetCPUInfo 获取CPU信息**/
struct DLL_EXPORT CPUInfo {
    std::string CpuID;
    CPUInfo();
};

/** \brief Utility 工具类**/
class DLL_EXPORT Util {
public:
    ///io//通过const常量引用支持const char*与std::string
    /** \brief print with colors 颜色输出
     * print something with a custom color
     * 输出带自定义颜色的字符串
     * \return just like printf 和printf一样
     *
     * @@Reserved 保留，但是api发生变化
     */
    static int io_printColor(dstring message,const char * color);
    /** \brief get file size 获取文件大小
     * get file size efficiently using direct.h (better than fstream::seekg&tellg[ChatGPT says])
     * 使用direct.h快速获取文件大小(比fstream::seekg&tellg快[ChatGPT说的])
     * \param file path 文件路径
     * \return file size 文件大小
     */
    static long io_fileSize(dstring filePath);
    /** \brief read a file
     * read the rest content of a std::ifstream
     * 读取std::ifstream吃剩下的所有内容
     *
     * \param ifstream
     * \param storer content 内容（残羹剩饭）
     * \return status 状态
     */
    static int io_readAll(std::ifstream & reader,std::string & out);
    /** \brief read a file
     *
     *  read a file
     *  读取文件
     *
     * \param path
     * \param storer content 满汉全席
     * \return status
     */
    static int io_readAll(dstring path,std::string & out);
    /** \brief write data 写入数据
     *
     * \param file path
     * \param
     * \return 0 success,-1 error
     */
    static int io_writeAll(dstring path,dstring data);
    /** \brief check file/directory existence 检查文件或者文件夹（目录）存在与否
     * \param path
     * \return 是否存在
     *
     */
    static bool io_checkExistence(dstring path);

    /**
     * @brief 核心遍历实现
     * @param physicalPath 实际要遍历的物理路径
     * @param results 结果存储容器
     * @param remainingDepth 剩余遍历深度（-1表示无限）
     * @param appender 固定前缀字符串
     * @param includeFiles 是否包含文件
     * @param includeDirs 是否包含目录
     * @param useAbsolutePath 是否使用绝对路径格式
     *
     * @note
     * 1. appender会直接拼接到路径字符串前，不添加任何路径分隔符
     * 2. 当useAbsolutePath=true时，physicalPath会被转换为绝对路径
     * 3. 结果路径格式为：appender + 处理后的路径字符串
     * 4. 写入results的路径使用正斜杠(/)
     */
    static void io_traverseImpl(
        const fs::path& physicalPath,
        std::vector<std::string>& results,
        const fs::path fixedRoot,
        int remainingDepth,
        const std::string& appender,
        bool includeFiles,
        bool includeDirs,
        bool useAbsolutePath
    );

    /**
     * @brief 遍历文件和目录
     * @param path 要遍历的路径
     * @param files 结果存储容器
     * @param traverseDepth 遍历深度（-1表示无限）
     * @param appender 固定前缀字符串
     * @param absolute 是否使用绝对路径
     *
     * @note 结果示例：appender + "/tmp/a.txt" 或 appender + "./tmp/a.txt"
     */
    static void io_traverseFiles(
        const std::string& path,
        std::vector<std::string>& files,
        int traverseDepth = 0,
        const std::string& appender = "",
        bool absolute = false
    );

    /// @brief 仅遍历文件（其他参数同io_traverseFiles）
    static void io_traverseFilesOnly(
        const std::string& path,
        std::vector<std::string>& files,
        int traverseDepth = 0,
        const std::string& appender = "",
        bool absolute = false
    );

    /// @brief 仅遍历目录（其他参数同io_traverseFiles）
    static void io_traverseFolders(
        const std::string& path,
        std::vector<std::string>& folders,
        int traverseDepth = 0,
        const std::string& appender = "",
        bool absolute = false
    );

    /// @brief 递归遍历文件（无限深度）
    static void io_traverseFilesRecursive(
        const std::string& path,
        std::vector<std::string>& files,
        const std::string& appender = "",
        bool absolute = false
    ) {
        io_traverseFiles(path, files , -1, appender, absolute);
    }

    /// @brief 递归遍历目录（无限深度）
    static void io_traverseFoldersRecursive(
        const std::string& path,
        std::vector<std::string>& folders,
        const std::string& appender = "",
        bool absolute = false
    ) {
        io_traverseFolders(path, folders, -1, appender, absolute);
    }
///other
    /** \brief returns a time formatted as string
     *
     * time!!!!
     * 时间!!!
     *
     * \return time as string,fmt: "YY-MM-DD HH:MM:SS" 返回字符串的时间，格式 "年年-月月-天天 时时:分分:秒秒"
     */
    static std::string ot_getTime();
    /** \brief format duration 格式化间隔时间
     * \param seconds 秒数
     * \return formatted string 格式化的字符串
     */
    static std::string ot_formatDuration(int secs);

///system
    /** \brief GetCPUId
     *
     * \return CPUId
     */
    static std::string sys_getCPUId();
    /** \brief get program memory usage(bytes) currently 获取程序目前内存使用情况(单位:B)
     * \return mem stats 内存使用情况
     */
    static ProgramMemUsage sys_getProgramMemoryUsage();
    /** \brief get global(bytes) 获取全局内存使用情况(单位:B)
     * \return usage
     */
    static GlobalMemUsage sys_getGlobalMemoryUsage();
    /** \brief enable virtual console in Windows 在Windows系统中开启虚拟终端支持（支持转义彩色文字）
     * \return none
     */
    static void sys_enableVirtualTerminal();

///data_string
    /** \brief unescaping strings 逆转义字符串
     * \param data
     * \return unescaped string
     */
    static std::string str_unescape(dstring in);
    //有返回值和没返回值的区别
    static void str_trim_nrt(std::string& str);
    static std::string str_trim_rt(std::string& str);
    /** \brief split strings as small tokens 分割字符串
     * \param source
     * \param a token
     * \param a storer
     */
    static void str_split(dstring source,const char separator,std::vector<std::string> & restorer);
    /** \brief split strings as small tokens 分割字符串
     * \param source
     * \param a token as string
     * \param a storer
     */
    static void str_split(dstring source,dstring separatorString,std::vector<std::string>& restorer);

///data_string_encoding
    /** \brief transcode 转码
     * \param ansi
     * \return utf8
     */
    static std::string str_encAnsiToUTF8(dstring strAnsi);
    /** \brief transcode 转码
     * \param utf8
     * \return ansi
     */
    static std::string str_encUTF8ToAnsi(dstring strUTF8);

    static std::string str_toUpper(dstring in);
    static std::string str_toLower(dstring in);

    Util& operator=(Util&) = delete;
    Util() = delete;
};
}
}
#endif // AAAA_UTIL_H_INCLUDED
