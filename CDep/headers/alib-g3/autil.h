///This file is not of c linkage
#ifndef AAAA_UTIL_H_INCLUDED
#define AAAA_UTIL_H_INCLUDED
#include <string>
#include <vector>
#include <fstream>

#define ALIB_SUCCESS 0
#define ALIB_ERROR -1

///For windows support
#ifdef _WIN32
/*  To use this exported function of dll, include this header
 *  in your project.
 */
#include <windows.h>
#ifndef DLL_EXPORT
#ifdef BUILD_DLL
    #define DLL_EXPORT __declspec(dllexport)
#else
    #define DLL_EXPORT __declspec(dllimport)
#endif
#endif // BUILD_DLL

#elif __linux__

#include <unistd.h>
#ifndef DLL_EXPORT
#ifdef BUILD_DLL
    #define DLL_EXPORT
#else
    #define DLL_EXPORT
#endif
#endif // BUILD_DLL

#endif
// 前景颜色宏定义 (Windows 专用)
#define APCF_BLACK        0
#define APCF_BLUE         1
#define APCF_GREEN        2
#define APCF_CYAN         3
#define APCF_RED          4
#define APCF_MAGENTA      5
#define APCF_YELLOW       6
#define APCF_WHITE        7
#define APCF_GRAY         8
#define APCF_LIGHT_BLUE   9
#define APCF_LIGHT_GREEN  10
#define APCF_LIGHT_CYAN   11
#define APCF_LIGHT_RED    12
#define APCF_LIGHT_MAGENTA 13
#define APCF_LIGHT_YELLOW 14
#define APCF_BRIGHT_WHITE 15

// 背景颜色宏定义
#define APCB_BLACK        (0 << 4)
#define APCB_BLUE         (1 << 4)
#define APCB_GREEN        (2 << 4)
#define APCB_CYAN         (3 << 4)
#define APCB_RED          (4 << 4)
#define APCB_MAGENTA      (5 << 4)
#define APCB_YELLOW       (6 << 4)
#define APCB_WHITE        (7 << 4)
#define APCB_GRAY         (8 << 4)
#define APCB_LIGHT_BLUE   (9 << 4)
#define APCB_LIGHT_GREEN  (10 << 4)
#define APCB_LIGHT_CYAN   (11 << 4)
#define APCB_LIGHT_RED    (12 << 4)
#define APCB_LIGHT_MAGENTA (13 << 4)
#define APCB_LIGHT_YELLOW (14 << 4)
#define APCB_BRIGHT_WHITE (15 << 4)

namespace alib {
namespace ng {
using namespace std;
using dstring = const std::string&;
#ifdef _WIN32
using mem_bytes = __int64;
#elif __linux__
using mem_bytes = __int64_t;
#endif // _WIN32

/** \brief Program Memory 程序使用内存
 *  mem memory 内存
 *  vmem virtual memory 虚拟内存
 */
struct DLL_EXPORT MemTp {
    mem_bytes mem;
    mem_bytes vmem;
};
/** \brief Global Memory Usage 全局内存使用情况（不是(not)OpenGL!!!)
 * percent currently_being_used_mem/mem_all 内存使用百分比
 * phy_all capacity of physical mem 物理内存总量
 * vir_all ...
 * you can understand 你能看懂的
 */
struct DLL_EXPORT GlMem {
    unsigned int percent;
    mem_bytes phy_all;
    mem_bytes vir_all;
    mem_bytes phy_used;
    mem_bytes vir_used;
    mem_bytes page_all;
    mem_bytes page_used;
};

/** \brief GetCPUInfo 获取CPU信息
 * 目前只支持获取CPU Id
 */
struct DLL_EXPORT CPUInfo {
    std::string CpuID;
    CPUInfo();
};

/** \brief Utility 工具类
 * 这还有什么要讲的？？
 */
class DLL_EXPORT Util {
public:
///io
    //通过const常量引用支持const char*与std::string
    /** \brief print with colors 颜色输出
    *
    * print something with a custom color
    * 输出带自定义颜色的字符串
    *
    * \param what u want to print 你要打印什么
    * \param color 颜色
    * \return just like printf 和printf一样
    */
    static int io_printColor(dstring message,int color);
    /** \brief traverse files 遍历文件
    *
    * traverse all files in a folder(by default not include sub-folders)
    * 遍历一个文件夹下面的所有文件（默认不包括子文件夹）
    *
    * \param folder path 文件夹路径
    * \param a vector to store these file names 一个std::vector用于存放数据
    */
    static void io_traverseFiles(dstring path, std::vector<std::string>& files,int traverseDepth = 0,dstring appender = "");
    /** \brief get file size 获取文件大小
     *
     * get file size efficiently using direct.h (better than fstream::seekg&tellg[ChatGPT says])
     * 使用direct.h快速获取文件大小(比fstream::seekg&tellg快[ChatGPT说的]）
     *
     * \param file path 文件路径
     * \return file size 文件大小
     */
    static long io_fileSize(dstring filePath);
    /** \brief read a file
     *
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

///other
    /** \brief returns a time formatted as string
     *
     * time!!!!
     * 时间!!!
     *
     * \return time as string,fmt: "YY-MM-DD HH:MM:SS" 返回字符串的时间，格式 "年年-月月-天天 时时:分分:秒秒"
     */
    static string ot_getTime();
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
    static string sys_GetCPUId();
    /** \brief get program memory usage(bytes) currently 获取程序目前内存使用情况(单位:B)
     * \return mem stats 内存使用情况
     */
    static MemTp sys_getProgramMemoryUsage();
    /** \brief get global(bytes) 获取全局内存使用情况(单位:B)
     * \return usage
     */
    static GlMem sys_getGlobalMemoryUsage();

///data_string
    /** \brief unescaping strings 逆转义字符串
     * \param data
     * \return unescaped string
     */
    static std::string str_unescape(dstring in);
    //有返回值和没返回值的区别
    static void str_trim_nrt(std::string& str);
    static std::string str_trim_rt(std::string& str);
    /** \brief trim string 移除字符串前后空白字符
     * \param template<bool returnACopy = false> 自己看
     * \param string to be modified 要修改的string
     */
    template<bool returnACopy = false> static inline auto str_trim(std::string& str) {
        if constexpr(returnACopy)return str_trim_rt(str);
        else str_trim_nrt(str);
    }
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
