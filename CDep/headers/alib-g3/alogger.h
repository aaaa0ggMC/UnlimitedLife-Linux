/** @file alogger.h
* @brief 与日志有关的函数库
* @author aaaa0ggmc
* @last-date 2025/04/04
* @date 2025/11/02 
* @version 3.1
* @copyright Copyright(C)2025
********************************
@par 修改日志:
<table>
<tr><th>时间       <th>版本         <th>作者          <th>介绍
<tr><td>2025-4-04 <td>3.1          <th>aaaa0ggmc    <td>添加doc
<tr><td>2025-4-04 <td>3.1          <th>aaaa0ggmc    <td>完成doc
</table>
********************************
*/
/** @todo Logger添加appendConsole appendFile ...实现简化（不然有点麻烦）
 * @todo 实现LogManager对log文件（文件夹）进行管理
 */
#ifndef ALOGGER_H_INCLUDED
#define ALOGGER_H_INCLUDED
#include <memory>
#include <string>
#include <fstream>
#include <unordered_map>
#include <map>
#include <optional>
#include <alib-g3/aclock.h>
#include <alib-g3/autil.h>
#include <mutex>

#ifdef __linux__ //你知道为什么abi支持我只在linux才搞吗，因为windows......编译报错了，但是我又希望保留特性
#ifndef ALIB_DISABLE_TEMPLATES
#include <cxxabi.h>
#endif // ALIB_DISABLE_TEMPLATES
#endif

#ifdef __linux__
#include <pthread.h>
#define LOCK pthread_mutex_t
#elif _WIN32
#define LOCK CRITICAL_SECTION
#endif // __linux__

//对glm的支持
#ifndef ALIB_DISABLE_GLM_EXTENSIONS
#include <glm/glm.hpp>
#include <glm/ext/quaternion_common.hpp>
#endif // ALIB_DISABLE_GLM_EXTENSIONS

//输出的各种级别
#define LOG_TRACE 0x00000001 ///<简简单单的输出
#define LOG_DEBUG 0x00000010 ///<调试信息
#define LOG_INFO  0x00000100 ///<比较重要的信息
#define LOG_WARN  0x00001000 ///<警告信息
#define LOG_ERROR 0x00010000 ///<错误信息
#define LOG_CRITI 0x00100000 ///<致命错误

#define LOG_OFF   0x01000000 ///<不要输出
#define LOG_FULL (LOG_TRACE | LOG_DEBUG | LOG_INFO | LOG_WARN | LOG_ERROR | LOG_CRITI) ///<完全版本
#define LOG_RELE (LOG_INFO | LOG_ERROR | LOG_CRITI | LOG_WARN) ///<Release级别日志

//输出的附加信息
#define LOG_SHOW_TIME 0x00000001 ///<时间，格式 [yyyy-mm-dd hh:mm:ss]
#define LOG_SHOW_TYPE 0x00000010 ///<显示日志级别
#define LOG_SHOW_ELAP 0x00000100 ///<显示日志的offset
#define LOG_SHOW_THID 0x00001000 ///<显示线程id
#define LOG_SHOW_HEAD 0x00010000 ///<显示头信息(一般用于只是来源)，比如 ALIB4
#define LOG_SHOW_PROC 0x00100000 ///<进程id
#define LOG_SHOW_NONE 0x00000000 ///<占位符，什么也不显示

//组合拳
#define LOG_SHOW_BASIC (LOG_SHOW_TIME | LOG_SHOW_TYPE | LOG_SHOW_ELAP | LOG_SHOW_HEAD) ///<基本显示
#define LOG_SHOW_FULL  (LOG_SHOW_BASIC | LOG_SHOW_THID | LOG_SHOW_PROC) ///<全部显示

namespace alib{
namespace g3{
    /** @struct LogHeader
     *  @brief Log类别信息
     *  @todo 后期支持自定义
     */
    struct DLL_EXPORT LogHeader{
        const char * str;///<文字，如 INFO DEBUG TRACE...
        const char * color;///<颜色，使用 ACP_XXX
    };

    /** @struct CriticalLock
     *  @brief 锁，windows上使用CriticalSection,Linux上使用 recursive mutex
     * @note 这个不是什么致命锁，而是指windows上的CriticalSection,由于历史遗留问题比较 \n
     * 难改于是我索性不改了
     */
    struct DLL_EXPORT CriticalLock{
        LOCK * cs; ///<锁
        /** @brief 构造函数
         * @param[in] lock 这个函数只是帮忙上锁与解锁，因此你需要提供锁
         */
        CriticalLock(LOCK & lock);
        ~CriticalLock();
    };

    /** @struct LogOutputTarget
     * @brief 一个基类，用于描述日志文件的输出去向，一些预设在 alib::g3::lot 这个命名空间里面
     */
    struct DLL_EXPORT LogOutputTarget{
    public:
        bool enabled; ///<用于toggle输出，一般不用管
        LogOutputTarget();

        /** @brief 每当Logger接收到一个日志信息并经过Filter处理过后便会调用write
         * @param[in] logLevel 日志等级
         * @param[in] message 最终信息（敏感信息可以被filter过滤而被隐去）
         * @param[in] timeHeader 时间头，实际上现在不太需要（因为现在颜色处理用的是字符串ACP_XX而不是之前的数字代码），历史保存遗留
         *  @todo 也许可以删了提高性能
         * @param[in] restContent 除了时间头剩下的额外信息
         * @param[in] showExtra 信息用于进行选择性输出（主要是Console这个预设需要检测你是否开启了LOG_SHOW_TYPE然后带颜色地输出type）
         */
        virtual void write(int logLevel,const std::string & message,const std::string& timeHeader,const std::string& restContent,int showExtra);

        /** @brief 刷新数据一般文件系统,网络需要
         * @see ~LogOutputTarget()
         */
        virtual void flush();
        /** @brief 关闭IO
         *  @see ~LogOutputTarget()
         */
        virtual void close();
        /// @note 这里会自动帮你 flush() 与 close(),注意哦！！
        virtual ~LogOutputTarget();
    };

    ///一些LogOutputTarget预设
    namespace lot{
        ///用于获取header信息
        LogHeader DLL_EXPORT getHeader(int level);

        /** @struct Console
         * @brief 输出到控制台
         */
        struct DLL_EXPORT Console : LogOutputTarget{
            const char * neon { NULL };///<内容的颜色（不是头）
            static std::mutex global_lock; ///< 用于保持多线程下控制台信息绝对不会乱

            void setContentColor(const char * color);///<设置内容的颜色

            void write(int logLevel,const std::string & message,const std::string& timeHeader,const std::string& restContent,int showExtra) override;
            void flush() override;
            void close() override;
        };

        /** @struct SingleFile
         * @brief 输出到单个文件
         */
        struct DLL_EXPORT SingleFile : LogOutputTarget{
            ///构造函数，需要提供打开的文件
            SingleFile(const std::string & path);

            void open(const std::string & path);///<打开另外一个文件

            void write(int logLevel,const std::string & message,const std::string& timeHeader,const std::string& restContent,int showExtra) override;
            void flush() override;
            void close() override;
        private:
            std::ofstream ofs; ///<内部维系的ofstream
            bool nice; ///< 相当于ofs::good(),用于防止出现io错误
        };

        /** @struct SplittedFiles
         * @brief 分割文件（到一定大小后自动分割）
         * @warning 分割不一定准确！！！！如果有严格大小要求可以自己实现LogOutputTarget自己实现严格的
         * @todo 支持用户自己设置命名规则
         * @par 文件创建示例如下:
         * 我给的path为 1.txt
         * 那么创建的文件将为：
         * 1.txt
         * 1 - 1.txt
         * 1 - 2.txt
         * ...
         */
        struct DLL_EXPORT SplittedFiles : LogOutputTarget{
            unsigned int maxBytes; ///<单个文件理论最大大小 @note 为什么是理论??? \n 因为至少会保存一条信息（除非等于0）

            /** @brief 构造函数
             * @param[in] path 基本文件地址
             * @param[in] maxBytes 单个文件理论最大大小
             */
            SplittedFiles(const std::string & path,unsigned int maxBytes);
            /** @brief 获取现在的index数
             *  @return index值
             */
            unsigned int getCurrentIndex();

            void write(int logLevel,const std::string & message,const std::string& timeHeader,const std::string& restContent,int showExtra) override;
            void flush() override;
            void close() override;

            void open(const std::string & path);///<重新指定一个基本地址

            ///生成文件名字的函数，以后会变成动态的
            static std::string generateFilePath(const std::string & in,int index);
        private:
            ///用于检测是否需要更换文件（主要是比对大小）
            void testSwapFile();

            unsigned int splitIndex; ///<文件的index
            unsigned int currentBytes;///<目前保存的字节总数，不一定为消息真实大小
            std::ofstream ofs;///<std::ofstream
            std::string path;///<基本文件地址
            bool nice;///<防止io错误
        };
    }

    /** @struct LogFilter
     * @brief 一个基类，日志过滤器，用于处理到来的日志信息
     */
    struct DLL_EXPORT LogFilter{
        bool enabled;///<标示是否启用

        LogFilter();
        /** @brief 进一步过滤信息，可以修改内容
         * @param[in] logLevel 日志等级
         * @param[in,out] mainContent 主要内容
         * @param[in,out] timeHeader 时间头
         * @param[in,out] extraContent 附加信息
         * @return 是否保留信息
         * - true 保留信息
         * - false 直接抛弃信息
         */
        virtual bool filter(int logLevel,std::string & mainContent,std::string & timeHeader,std::string & extraContent);
        /** @brief 初次过滤信息（一般用于舍弃极为敏感的日志）
         * @param[in] logLevel 日志等级
         * @param[in] originMessage 原始信息
         */
        virtual bool pre_filter(int logLevel,const std::string& originMessage);

        virtual ~LogFilter();
    };

    ///一些LogFilter预设
    namespace lgf{
        /** @struct LogLevel
         * @brief 对于不想要的LogLevel进行抛弃，实现了pre_filter
         */
        struct DLL_EXPORT LogLevel : LogFilter{
            int showLevels;///<想要的loglevel

            ///设置等级，默认为 LOG_RELE (Release级别日志)
            LogLevel(int showLevels = LOG_RELE);

            bool pre_filter(int logLevel,const std::string& originMessage) override;
        };

        /** @struct KeywordsBlocker
         *  @brief 对于出现敏感词的消息直接拒绝
         */
        struct DLL_EXPORT KeywordsBlocker : LogFilter{
            std::vector<std::string> keywords;///<敏感词列表

            /** @brief 设置敏感词
             *@par 例子:
             * @code
             *  Logger logger;
             *  LogFactory lg(logger,"123");
             *  ...//省略配置过程
             *  logger.appendLogFilter("hello",std::make_shared<lgf::KeywordsBlocker>({"123"}));
             *  ...//恭喜你，lg发送的消息全会被block（因为head为123）
             * @endcode
             */
            KeywordsBlocker(const std::vector<std::string>& keywords);

            bool pre_filter(int logLevel,const std::string& originMessage) override;
        };

        /** @struct KeywordsReplacerMono
         *  @brief 对于关键词用某个关键词进行替换，有两种模式 \n
         *  - 字符模式 使用单个字符填充 如 [secret] --> [*****]
         *  - 字符串模式 使用一个字符串填充 如 [secret] --> [谁叫你看敏感信息了？？？]
         */
        struct DLL_EXPORT KeywordsReplacerMono : LogFilter{
            std::vector<std::string> keywords;///<关键词列表
            bool useChar;///<是否使用字符
            char ch;///<用于替换的单个字符
            std::string replacement;///<用于替换的字符串

            /** @brief 构造函数
             * @param[in] useChar true使用字符模式 false使用字符串模式
             * @param[in] ch 替换字符
             * @param[in] replacement 替换字符串
             * @param[in] keys 关键词列表
             */
            KeywordsReplacerMono(bool useChar,char ch,const std::string & replacement,const std::vector<std::string> & keys);

            bool filter(int logLevel,std::string & mainContent,std::string & timeHeader,std::string & extraContent) override;
        };

    }

    /** @struct Logger
     * @brief 日志核心类
     */
    class DLL_EXPORT Logger{
    private:
        friend class LogFactory;
        int showExtra;///<额外显示的东西
        Clock clk;///<内部的计时器
        #ifdef __linux__
        pthread_mutexattr_t mutex_attr; ///<为了设置recursive,linux平台需要这个
        #endif // __linux__
        LOCK lock;///<锁
        static Logger * instance;///<静态实例（可用于alib错误输出)
    public:
        std::unordered_map<std::string,std::shared_ptr<LogOutputTarget>> targets;///<输出的对象列表
        std::map<std::string,std::shared_ptr<LogFilter>> filters;///<过滤器列表

        /** @brief 构造函数
         * @param[in] showExtra 显示的额外信息，默认为 LOG_SHOW_BASIC
         * @param[in] setInstanceIfNULL 如果 Logger::instance为NULL,那么就把instance设置为该logger,默认为true
         */
        Logger(int showExtra = LOG_SHOW_BASIC,bool setInstanceIfNULL = true);
        ~Logger();

        /** @brief 输出信息，LogFactory实际上用的就是这个
         * @param[in] level 日志级别
         * @param[in] content 内容
         */
        void log(int level,dstring content,dstring head);

        void flush();///<对每个logtarget进行刷新

        /** @brief 设置额外信息
         * @param[in] showMode 额外信息 LOG_SHOW_XXX
         */
        void setShowExtra(int showMode);

        /** @brief 获取额外信息
         * @return 额外信息数据
         */
        int getShowExtra();

        /** @brief 添加LogTarget
         * @param[in] name 用于获取的名字
         * @param[in] target target对象,使用shared_ptr防止出现内存错误
         */
        void appendLogOutputTarget(const std::string &name,std::shared_ptr<LogOutputTarget> target);


        /** @brief 添加LogFilter
         * @param[in] name 用于获取的名字
         * @param[in] filter filter对象,使用shared_ptr防止出现内存错误
         */
        void appendLogFilter(const std::string & name,std::shared_ptr<LogFilter> filter);


        /** @brief 关掉LogTarget
         * @param[in] name 名字
         */
        void closeLogOutputTarget(const std::string& name);

        /** @brief 关掉LogFilter
         * @param[in] name 名字
         */
        void closeLogFilter(const std::string& name);

        /** @brief 获取OutputTarget状态
         * @return 状态
         * - 0 没有启用
         * - 1 正在启用
         * - -1 找不到了
         */
        int getLogOutputTargetStatus(const std::string& name);


        /** @brief 获取Filter状态
         * @return 状态
         * - 0 没有启用
         * - 1 正在启用
         * - -1 找不到了
         */
        int getLogFilterStatus(const std::string& name);

        /** @brief 设置OutputTarget状态
         * @param[in] enabled 启用状态 \n
         * - true 启用
         * - false 禁用
         */
        void setLogOutputTargetStatus(const std::string& name,bool enabled);

        /** @brief 设置Filter状态
         * @param[in] enabled 启用状态 \n
         * - true 启用
         * - false 禁用
         */
        void setLogFilterStatus(const std::string& name,bool enabled);

        //ends:end line
        /** @brief 生成一条message
         * @param[in] ends 是否加入换行符，默认为true
         */
        std::string makeMsg(int level,dstring data,dstring head,bool ends = true);
        /** @brief 输出额外数据 */
        void makeExtraContent(std::string&time,std::string&rest,dstring head,dstring showTypeStr,int showExtra,bool addLg);

        /** @brief 设置静态的logger
         * @param[in] logger logger的指针（比较危险了）
         * @todo 变得更加安全一点
         */
        static void setStaticLogger(Logger* logger);

        /** @brief 获取静态的logger
         * @return Logger的指针，使用 std::optional 保护
         * - std::nullopt 空的
         * - 其他 有内容（可能是悬垂的指针）
         */
        static std::optional<Logger*> getStaticLogger();
    };


    /** @brief 用于匹配LogFactory，没用实际作用 */
    struct DLL_EXPORT LogEnd{};
    /** @brief 用于表示日志结尾
     * @par 例子:
     * @code
     *  LogFactory lg...//省略
     *  lg << "你好" << endlog << "哈哈";
     *  lg << endlog;
     *  lg << endlog;
     * @endcode
     * 上面的代码产生了三段日志，前两段有信息最后一段空的
     */
    void DLL_EXPORT endlog(LogEnd);

    ///用于匹配
    typedef void (*EndLogFn)(LogEnd);

    /** @struct LogFactory
     * @brief 用于生成log的类，支持 <<
     * @note 支持 几乎所有基本类，至少支持 \n
     * std::string , std::vector , std::map , std::unordered_map , std::tuple\<...\> 支持叠加container
     */
    class DLL_EXPORT LogFactory{
    private:
        std::string head;///<头信息
        Logger* i;///<绑定的logger
        int defLogType;///<默认的log level级别
        bool showContainerName;///< << 输出时是否显示容器名字
    public:
        static THREAD_LOCAL std::string cachedStr;///<缓存的数据 @todo Windows上THREADLOCAL目前无效,也许需要处理

        /** @brief 初始化
         * @param[in] head 头信息
         * @param[in] lg 绑定的logger
         */
        LogFactory(dstring head,Logger& lg);

        /** @brief 基础输出
         * @param[in] level 等级
         * @param[in] msg 信息
         */
        void log(int level,dstring msg);
        ///log(LOG_INFO,...)
        void info(dstring msg);
        ///log(LOG_ERROR,...)
        void error(dstring msg);
        ///log(LOG_CRITI,...)
        void critical(dstring msg);
        ///log(LOG_DEBUG,...)
        void debug(dstring msg);
        ///log(LOG_TRACE,...)
        void trace(dstring msg);
        ///log(LOG_WARN,...)
        void warn(dstring msg);
        /** @brief 设置是否显示容器名字
         * @param[in] v 是否显示
         * - true 显示
         * - false 不显示
         */
        void setShowContainerName(bool v);

        /** @brief 用于设置等级，会覆盖
         * @example logfactory(LOG_ERROR) << ...
         */
        LogFactory& operator()(int logType = LOG_INFO);

        ///终止log
        LogFactory& operator<<(EndLogFn fn);
        //也支持std::endl来终止
        LogFactory& operator<<(std::ostream& (*manip)(std::ostream&));

        //glm支持
        #ifndef ALIB_DISABLE_GLM_EXTENSIONS
        /// x
        LogFactory& operator<<(glm::vec1 data);
        /// x y
        LogFactory& operator<<(glm::vec2 data);
        /// x y z
        LogFactory& operator<<(glm::vec3 data);
        /// x y z w
        LogFactory& operator<<(glm::vec4 data);
        /// (x y z w)(a b c d)(e f g h)(i j k l)
        LogFactory& operator<<(glm::mat4 data);
        /// (x y z)(a b c)(e f g)
        LogFactory& operator<<(glm::mat3 data);
        /// (a b)(c d)
        LogFactory& operator<<(glm::mat2 data);
        /// (x,y,z,w)
        LogFactory& operator<<(glm::quat data);
        #endif // ALIB_DISABLE_GLM_EXTENSIONS

        ///解码template的名字
        template<class T> std::string demangleTypeName(const char * mangledName){
#ifdef __linux__
            int status;
            std::unique_ptr<char,void(*)(void*)> result(
                abi::__cxa_demangle(mangledName,nullptr,nullptr,&status),
                std::free
            );
            return (status==0)?result.get():mangledName;
#else
            return mangledName;
#endif
        }


        #define VALUE_FIX(TESTV) \
                {/*有没有const修饰一样---似乎又不行了*/\
                    if(typeid(TESTV) == typeid(std::string) || typeid(TESTV) == typeid(char *) || typeid(TESTV) == typeid(const char *)){\
                        cachedStr += "\"";\
                    }else if(typeid(TESTV) == typeid(char)){\
                        cachedStr += "\'";\
                    }\
                }

        ///支持 std::vector\<Type,Allocator\>
        template<template<class T,class Allocator> class Cont,class T,class A,typename = std::enable_if_t<std::is_same<Cont<T,A>,std::vector<T,A>>::value>>
            LogFactory& operator<<(const Cont<T,A> & cont){
            if(showContainerName)cachedStr += demangleTypeName<decltype(cont)>(typeid(decltype(cont)).name());
            auto endloc = &(*(--cont.end()));
            cachedStr += "[";
            bool old = showContainerName;
            showContainerName = false;
            for(const auto & v : cont){
                VALUE_FIX(T)
                operator<<(v);
                VALUE_FIX(T)
                if(&v != endloc)cachedStr += ",";
            }
            cachedStr += "]";
            showContainerName = old;
            return *this;
        }

        ///支持 std::map\<Key,Value,Allocator\>
        template<template<class K,class V,class Allocator> class Cont,class K,class V,class A,typename = std::enable_if_t<std::is_same<Cont<K,V,A>,std::map<K,V,A>>::value>>
            LogFactory& operator<<(const Cont<K,V,A> & cont){
            if(showContainerName)cachedStr += demangleTypeName<decltype(cont)>(typeid(decltype(cont)).name());
            auto endloc = &((--cont.end())->second);
            cachedStr += "{";
            bool old = showContainerName;
            showContainerName = false;
            for(const auto & [k,v] : cont){
                VALUE_FIX(K)
                operator<<(k);
                VALUE_FIX(K)
                cachedStr += ":";
                VALUE_FIX(V)
                operator<<(v);
                VALUE_FIX(V)
                if(&v != endloc)cachedStr += ",";
            }
            cachedStr += "}";
            showContainerName = old;
            return *this;
        }

        ///支持 std::unordered_map\<Key,Value,Hash,Predicate,Allocator\>
        template<template<class Key,class Value,class Hash,class Predicate,class Allocator> class Cont,class K,class V,class H,class P,class A>
            LogFactory& operator<<(const Cont<K,V,H,P,A>& cont){
            if(showContainerName)cachedStr += demangleTypeName<decltype(cont)>(typeid(decltype(cont)).name());
            cachedStr += "{";
            bool old = showContainerName;
            showContainerName = false;
            for(const auto & [k,v] : cont){
                VALUE_FIX(K)
                operator<<(k);
                VALUE_FIX(K)
                cachedStr += ":";
                VALUE_FIX(V)
                operator<<(v);
                VALUE_FIX(V)
                cachedStr += ",";
            }
            cachedStr.erase(--cachedStr.end());
            cachedStr += "}";
            showContainerName = old;
            return *this;
        }

        /** @struct tuple_show<Tuple,N>
         * @brief 用于支持std::tuple
         */
        template<class Tuple,size_t N> struct tuple_show{
            static void show(const Tuple&t,LogFactory&lg){
                tuple_show<Tuple,N - 1>::show(t,lg);
                std::string & cachedStr = lg.cachedStr;
                cachedStr += ",";
                const auto& vl = std::get<N -1>(t);
                VALUE_FIX(decltype(vl));
                lg.operator<<(vl);
                VALUE_FIX(decltype(vl));
            }
        };
        /** @struct tuple_show<Tuple,1>
         * @brief 最后一层匹配
         */
        template<class Tuple> struct tuple_show <Tuple,1>{
            static void show(const Tuple&t,LogFactory&lg){
                std::string & cachedStr = lg.cachedStr;
                const auto& vl = std::get<0>(t);
                VALUE_FIX(decltype(vl));
                lg.operator<<(vl);
                VALUE_FIX(decltype(vl));
            }
        };

        ///支持 std::tuple\<...\>
        template<template<typename... Elements> class Cont,typename... Eles,typename = std::enable_if_t<std::is_same<Cont<Eles...>,std::tuple<Eles...>>::value>>
            LogFactory& operator<<(const Cont<Eles...> & t){
            if(showContainerName)cachedStr += demangleTypeName<decltype(t)>(typeid(decltype(t)).name());
            bool old = showContainerName;
            showContainerName = false;
            cachedStr += "[";
            tuple_show<decltype(t),sizeof...(Eles)>::show(t,*this);
            cachedStr += "]";
            showContainerName = old;
            return *this;
        }

        ///匹配剩余的东西
        template<class T>
            LogFactory& operator<<(const T& t){
            cachedStr += alib::g3::ext_toString::toString(t);
            return *this;
        }
    };
}
}

#endif // ALOGGER_H_INCLUDED
