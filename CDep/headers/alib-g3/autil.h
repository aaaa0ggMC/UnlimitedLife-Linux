/**@file autil.h
* @brief 工具库，提供IO、字符串、系统数据等静态函数
* @author aaaa0ggmc
* @date 2025-3-29
* @version 3.1
* @copyright Copyright(c)2025
********************************************************
@attention
目前版本为v3.1,仅仅支持C++,最好使用GCC以防符号链接出错
@par 修改日志:
<table>
<tr><th>时间       <th>版本         <th>作者          <th>介绍        
<tr><td>2025-3-29 <td>3.1          <th>aaaa0ggmc    <td>添加doc  
<tr><td>2025-3-30 <td>3.1          <th>aaaa0ggmc    <td>doc已写完
</table>
******************************************************** 
*/

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

//Platform Related
#ifdef _WIN32
//需要注意的是目前Windows版本由于使用msys2 ucrt64构建，因此threadlocal实现比较尴尬
#define THREAD_LOCAL 
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

//Colors 使用Deepseek生成
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

//256色模式
#define ACP_FG256(n) "\e[38;5;" #n "m"
#define ACP_BG256(n) "\e[48;5;" #n "m"

//RGB真彩色模式
#define ACP_FGRGB(r,g,b) "\e[38;2;" #r ";" #g ";" #b "m"
#define ACP_BGRGB(r,g,b) "\e[48;2;" #r ";" #g ";" #b "m"
#endif

#ifndef ALIB_TO_STRING_RESERVE_SIZE
#define ALIB_TO_STRING_RESERVE_SIZE 128   
#endif // ALIB_TO_STRING_RESERVE_SIZE


//错误代码
#define AE_SUCCESS 0
#define AE_FAILED -1

///aaaa0ggmcLib：一个主要给自己使用的综合库
namespace alib {
///第三代alib:更加优秀的api以及性能（相比于alib-g2)，主要拿来给自己使用。
namespace g3 {
//一个简称，对std::string&的引用进行简化
using dstring = const std::string&;
#ifdef _WIN32
//对于平台进行区分，MINGW用__int64,Linux用__int64_t
using mem_bytes = __int64;
#elif __linux__
using mem_bytes = __int64_t;
#endif // _WIN32

///用于高效转换为字符串，预计比std::to_string更加迅速(需不定义宏 ALIB_DISABLE_CPP20)，但是会动态占用内存
namespace ext_toString{
    #ifndef ALIB_DISABLE_CPP20
    //会自动扩充的FormatBuffer,按照C++标准似乎是双倍扩增，初始大小为 ALIB_TO_STRING_RESERVE_SIZE
    static inline THREAD_LOCAL std::string fmtBuf;
    [[maybe_unused]] static inline THREAD_LOCAL bool inited = []()->bool{
        fmtBuf.reserve(ALIB_TO_STRING_RESERVE_SIZE);
        return true;
    }();
    #endif // ALIB_DISABLE_CPP20

    /** @brief 对于可以直接返回的类型直接返回降低额外占用，一般用于模板
     *  @param[in] v 需要parse的变量
     *  @return 原样输出
     */
    inline const std::string& toString(const std::string& v){
        return v;
    }

    /** @brief 用于匹配各种可以通过std::format(C++20 +)或者std::to_string转化的数据
     *  @param[in] v 需要parse的变量
     *  @return 经过转化后的值（这里性能并不是很高）
     *  @par 例子:
     *  @code
     * template<class... T> void ezprint(T... args){
     *     //这里使用了Cpp的折叠语法
     *     (std::cout << ... << alib::g3::ext_toString::toString(args)) << std::endl;
     * }
     * ezprint(1,2,3,"Hello",5,6);
     *  @endcode
     *  @par 输出
     *  @code
     * 123Hello56
     *  @endcode
     *  @see alib::g3::LogFactory
     */
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

/** @struct ProgramMemUsage
 *  @brief 程序使用的内存信息
 */
struct DLL_EXPORT ProgramMemUsage {
    mem_bytes memory;///<内存占用
    mem_bytes virtualMemory;///<虚拟内存占用
};

/** @struct GlobalMemUsage
 *  @brief 系统全局内存占用
 */
struct DLL_EXPORT GlobalMemUsage {
    //In linux,percent = phyUsed / phyTotal
    unsigned int percent;///<占用百分比,详情有注意事项 @attention Linux上等于 physicalUsed/physicalTotal,Windows上使用API提供的dwMemoryLoad
    mem_bytes physicalTotal;///<物理内存总值
    mem_bytes virtualTotal;///<虚拟内存总值，Linux上暂时固定为0
    mem_bytes physicalUsed;///<物理内存占用值
    mem_bytes virtualUsed;///<虚拟内存占用值，Linux上暂时固定为0
    mem_bytes pageTotal;///<页面文件总值，Linux上为SwapSize
    mem_bytes pageUsed;///<页面文件占用值,Linux上为SwapUsed
};

/** @struct CPUInfo
 *  @brief 获取CPU的信息，目前比较简陋
 */
struct DLL_EXPORT CPUInfo {
    std::string CpuID; ///<CPU的型号
    CPUInfo();///<构造函数中使用Util中函数自动初始化值
};

/** @class Util
 *  @brief 工具类
 *
 *  @note 该类包含 io sys ot str 四个维度，稍微简化了一下C++的功能
 */
class DLL_EXPORT Util {
public:
    /** @defgroup autil_io Util工具类的io函数
     *  @{
     *  @brief 涵盖控制台，文件操作
     */
    /** @brief 输出带颜色的字符串
     *  @return 类似printf的返回值
     *  @param[in] message 输出的信息
     *  @param[in] color 颜色，可以和ACP_XXX配合
     *  @note 需要注意的事，目前printColor比较废（由于Windows上可以开启virtual terminal processing从而识别转义颜色字符）
     *  @warning 保留，但是api已经发生变化，原本第二个参数是int的
     *
     *  @par 例子:
     *  @code
     *  Util::io_printColor("你好",ACP_RED ACP_BG_GREEN);//输出你好，前景颜色红色，背景颜色绿色
     *  @endcode
     */
    static int io_printColor(dstring message,const char * color);
    /** @brief 获取文件大小，使用direct.h，比fstream::seekg&tellg要快
     *  @param[in] filePath 文件路径
     *  @return 文件大小
     *
     *  @warning 有时候返回值可能比fstream获取的快一点
     */
    static long io_fileSize(dstring filePath);
    /** @brief 读取文件信息，使用ifstream读取剩下的内容
     *  @param[in] reader 输入流
     *  @param[out] storer 内容
     *  @return 返回读取的字符数
     */
    static int io_readAll(std::ifstream & reader,std::string & out);
    /** @brief 读取文件，使用路径
     *  @todo 进行错误处理
     *  @param[in] path 路径
     *  @param[out] storer 全部内容
     *  @return 执行结果
     *  - -1 文件读取失败
     *  - >=0 读取的大小
     */
    static int io_readAll(dstring path,std::string & out);
    /** @brief 写入数据
     *  @param[in] file 文件地址
     *  @param[in] data 数据
     *  @return 执行结果
     *  - AE_FAILED -1 IO失败
     *  - >=0 写入的大小
     */
    static int io_writeAll(dstring path,dstring data);
    /** @brief 检查文件或者文件夹（目录）存在与否
     *  @param path 文件或者文件夹的地址
     *  @return
     *  - true 文件存在
     *  - false 文件不存在
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
    /** @} autil_io
     */

    /** @defgroup autil_other Util中杂项函数
     *  @{
     *  @brief 包括时间获取，转化......
     */
    /** @brief 将时间转变为特定格式的字符串
     *  @note 实际上就是用了sprintf...作为一个保留函数
     *  @note 时间就是力量!!!
     *  @return 返回字符串的时间，格式 "年年-月月-天天 时时:分分:秒秒"
     */
    static std::string ot_getTime();
    /** @brief format duration 格式化间隔时间
     *  @param secs 秒数
     *  @return 格式化的字符串，格式： XXy XXd XXh XXm XXs
     */
    static std::string ot_formatDuration(int secs);
    /** @} autil_other
     */

    /** @defgroup autil_sys Util中与系统有关的函数
     *  @{
     */
    /** @brief 获取CPU的型号信息
     *  @return 字符串形式的CPU型号
     */
    static std::string sys_getCPUId();
    /** @brief 获取程序目前内存使用情况(单位:B)
     *  @return 程序内存使用情况
     */
    static ProgramMemUsage sys_getProgramMemoryUsage();
    /** @brief  获取全局内存使用情况(单位:B)
     *  @return 全局内存使用情况
     */
    static GlobalMemUsage sys_getGlobalMemoryUsage();
    /** @brief 在Windows系统中开启虚拟终端支持（支持转义彩色文字,Linux上没用，因为基本上支持）
     */
    static void sys_enableVirtualTerminal();
    /** @} autil_sys
     */

    /** @defgroup autil_data_string Util中与字符串有关的函数
     *  @{
     *  @brief 包含逆转义，字符串trim,分割，转码（Linux上没用），大小转化等等
     */
    /** @brief 逆转义字符串
     *  @param[in] in 输入数值
     *  @return 结果
     *  @note 支持的转义语序: \\n \\\ \\v \\t \\r \\a (alarm) \\f \\b \\0 \\? \\" \n
     *      其他的直接原样不变
     *  @par
     *  <i>这个函数的年龄已经两年了......(2025/3/30)</i>
     */
    static std::string str_unescape(dstring in);
    /** @brief 删除字符串收尾的空白字符
     *  @param[in,out] str 输入的字符串,会被处理
     *  @note 支持删除的空白字符： "\\f\\v\\r\\t\\n "
     */
    static void str_trim_nrt(std::string& str);
    /** @brief 删除字符串收尾的空白字符(返回一个copy)
     *  @param[in,out] str 输入的字符串，会被处理
     *  @note 支持删除的空白字符： "\\f\\v\\r\\t\\n "
     *  @return 一个copy
     */
    static std::string str_trim_rt(std::string& str);
    /** @brief 分割字符串
     *  @param[in] source 源字符串
     *  @param[in] separator 分割字符
     *  @param[out] restorer push_back对象
     *
     *  @par 例子:
     *  @code
     *  LogFactory lgf;
     *  Util::str_split("hello123,123,meow",',',data);
     *  lgf << data << endlog;
     *  @endcode
     *  <i>结果</i> \n
     *  @code
     *  ["hello123","123","meow"]
     *  @endcode
     */
    static void str_split(dstring source,const char separator,std::vector<std::string> & restorer);
    /** @brief 分割字符串++
     *  @param[in] source 源字符串
     *  @param[in] separatorString 分割的字符串
     *  @param[out] restorer push_back对象
     *
     *  @par 例子:
     *  @code
     *  LogFactory lgf;
     *  Util::str_split("hello123,123,meow","123",data);
     *  lgf << data << endlog;
     *  @endcode
     *  <i>结果</i> \n
     *  @code
     *  ["hello",",",",","meow"]
     *  @endcode
     */
    static void str_split(dstring source,dstring separatorString,std::vector<std::string>& restorer);
    /** @brief 转码
     *  @param[in] strAnsi 本机编码字符串
     *  @return utf8字符串
     *  @note Linux上面直接返回原本的字符串
     */
    static std::string str_encAnsiToUTF8(dstring strAnsi);
    /** @brief 转码
     *  @param[in] strUTF8 utf8字符串
     *  @return 本机编码字符串
     *  @note Linux上面直接返回原本的字符串
     */
    static std::string str_encUTF8ToAnsi(dstring strUTF8);
    /** @brief 把字符串的所有字符全部转化为大写字符（如果可以的话）
     *  @param[in] in 输入
     *  @return 输出
     */
    static std::string str_toUpper(dstring in);
    /** @brief 把字符串的所有字符全部转化为小写字符（如果可以的话）
     *  @param[in] in 输入
     *  @return 输出
     */
    static std::string str_toLower(dstring in);
    /** @} autil_data_string
     */

    ///删除构造函数，纯纯作为一个工具类
    Util& operator=(Util&) = delete;
    Util() = delete;
};
}
}
#endif // AAAA_UTIL_H_INCLUDED
