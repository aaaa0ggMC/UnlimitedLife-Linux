/** @file alogger.h
* @brief 与日志有关的函数库
* @author aaaa0ggmc
* @date 2025-4-04
* @version 3.1
* @copyright Copyright(C)2025
********************************
@par 修改日志:
<table>
<tr><th>时间       <th>版本         <th>作者          <th>介绍
<tr><td>2025-4-04 <td>3.1          <th>aaaa0ggmc    <td>添加doc
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

#ifdef __linux__ //你知道为什么abi支持我只在linux才搞吗，因为windows......编译报错了，但是我又希望保留特性
//#ifndef ALIB_DISABLE_TEMPLATES
//#include <cxxabi.h>
//#endif // ALIB_DISABLE_TEMPLATES
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
#define LOG_FULL (LOG_TRACE | LOG_DEBUG | LOG_INFO | LOG_WARN | LOG_ERROR | LOG_CRITI)
#define LOG_RELE (LOG_INFO | LOG_ERROR | LOG_CRITI | LOG_WARN)

//输出的附加信息
#define LOG_SHOW_TIME 0x00000001 ///<时间，格式 [yyyy-dd]
#define LOG_SHOW_TYPE 0x00000010 ///<显示日志级别
#define LOG_SHOW_ELAP 0x00000100 ///<显示日志的offset
#define LOG_SHOW_THID 0x00001000 ///<显示线程id
#define LOG_SHOW_HEAD 0x00010000 ///<显示头信息(一般用于只是来源)，比如 ALIB4
#define LOG_SHOW_PROC 0x00100000
#define LOG_SHOW_NONE 0x00000000

#define LOG_SHOW_BASIC (LOG_SHOW_TIME | LOG_SHOW_TYPE | LOG_SHOW_ELAP | LOG_SHOW_HEAD)
#define LOG_SHOW_FULL  (LOG_SHOW_BASIC | LOG_SHOW_THID | LOG_SHOW_PROC)

namespace alib{
namespace g3{
    struct DLL_EXPORT LogHeader{
        const char * str;
        const char * color;
    };

    struct DLL_EXPORT CriticalLock{
        LOCK * cs;
        CriticalLock(LOCK &);
        ~CriticalLock();
    };

    struct DLL_EXPORT LogOutputTarget{
    public:
        bool enabled;
        LogOutputTarget();

        virtual void write(int logLevel,const std::string & message,const std::string& timeHeader,const std::string& restContent,int showExtra);
        virtual void flush();
        virtual void close();
        ///will call flush() & close() automatically
        virtual ~LogOutputTarget();
    };

    namespace lot{
        LogHeader DLL_EXPORT getHeader(int level);

        struct DLL_EXPORT Console : LogOutputTarget{
            const char * neon { NULL };

            void write(int logLevel,const std::string & message,const std::string& timeHeader,const std::string& restContent,int showExtra);

            void setContentColor(const char * color);

            void flush();

            void close();
        };

        struct DLL_EXPORT SingleFile : LogOutputTarget{
            SingleFile(const std::string & path);
            void write(int logLevel,const std::string & message,const std::string& timeHeader,const std::string& restContent,int showExtra);
            void flush();
            void close();
            void open(const std::string & path);
        private:
            std::ofstream ofs;
            bool nice;
        };

        struct DLL_EXPORT SplittedFiles : LogOutputTarget{
            unsigned int maxBytes;

            SplittedFiles(const std::string & path,unsigned int maxBytes);

            void write(int logLevel,const std::string & message,const std::string& timeHeader,const std::string& restContent,int showExtra);
            void flush();
            void close();
            void open(const std::string & path);

            unsigned int getCurrentIndex();
            static std::string generateFilePath(const std::string & in,int index);
        private:
            void testSwapFile();

            unsigned int splitIndex;
            unsigned int currentBytes;
            std::ofstream ofs;
            std::string path;
            bool nice;
        };
    }

    struct DLL_EXPORT LogFilter{
        bool enabled;

        LogFilter();
        ///true:keep this log;false:discard the whole log content
        virtual bool filter(int logLevel,std::string & mainContent,std::string & timeHeader,std::string & extraContent);
        virtual bool pre_filter(int logLevel,const std::string& originMessage);
        virtual ~LogFilter();
    };

    namespace lgf{
        struct DLL_EXPORT LogLevel : LogFilter{
            int showLevels;

            LogLevel(int showLevels = LOG_RELE);

            bool pre_filter(int logLevel,const std::string& originMessage);
        };

        struct DLL_EXPORT KeywordsBlocker : LogFilter{
            std::vector<std::string> keywords;

            KeywordsBlocker(const std::vector<std::string>& keywords);

            bool pre_filter(int logLevel,const std::string& originMessage);
        };

        struct DLL_EXPORT KeywordsReplacerMono : LogFilter{
            std::vector<std::string> keywords;
            bool useChar;
            char ch;
            std::string replacement;

            //useChar: true usechar,false use string
            KeywordsReplacerMono(bool useChar,char ch,const std::string & replacement,const std::vector<std::string> & keys);

            bool filter(int logLevel,std::string & mainContent,std::string & timeHeader,std::string & extraContent);
        };

    }

    class DLL_EXPORT Logger{
    private:
        friend class LogFactory;
        int showExtra;
        Clock clk;
        #ifdef __linux__
        pthread_mutexattr_t mutex_attr;
        #endif // __linux__
        LOCK lock;
        static Logger * instance;
    public:
        std::unordered_map<std::string,std::shared_ptr<LogOutputTarget>> targets;
        std::map<std::string,std::shared_ptr<LogFilter>> filters;

        Logger(int showExtra = LOG_SHOW_BASIC,bool setInstanceIfNULL = true);
        ~Logger();

        void log(int level,dstring content,dstring head);
        void flush();

        void setShowExtra(int showMode);

        void appendLogOutputTarget(const std::string &name,std::shared_ptr<LogOutputTarget> target);
        void appendLogFilter(const std::string & name,std::shared_ptr<LogFilter> filter);

        void closeLogOutputTarget(const std::string& name);
        void closeLogFilter(const std::string& name);

        ///Enabled 0:false 1:true -1:can't find
        int getLogOutputTargetStatus(const std::string& name);
        int getLogFilterStatus(const std::string& name);

        void setLogOutputTargetStatus(const std::string& name,bool enabled);
        void setLogFilterStatus(const std::string& name,bool enabled);

        int getShowExtra();

        //ends:end line
        std::string makeMsg(int level,dstring data,dstring head,bool ends = true);
        void makeExtraContent(std::string&time,std::string&rest,dstring head,dstring showTypeStr,int showExtra,bool addLg);
        static void setStaticLogger(Logger*);
        static std::optional<Logger*> getStaticLogger();
    };


    struct DLL_EXPORT LogEnd{};
    void DLL_EXPORT endlog(LogEnd);
    typedef void (*EndLogFn)(LogEnd);

    class DLL_EXPORT LogFactory{
    private:
        std::string head;
        Logger* i;
        int defLogType;
        bool showContainerName;
    public:
        static THREAD_LOCAL std::string cachedStr;

        LogFactory(dstring head,Logger& lg);

        void log(int level,dstring msg);
        void info(dstring msg);
        void error(dstring msg);
        void critical(dstring msg);
        void debug(dstring msg);
        void trace(dstring msg);
        void warn(dstring msg);

        void setShowContainerName(bool v);

        LogFactory& operator()(int logType = LOG_INFO);

        ///end the log
        LogFactory& operator<<(EndLogFn fn);
        //std::endl
        LogFactory& operator<<(std::ostream& (*manip)(std::ostream&));

        #ifndef ALIB_DISABLE_GLM_EXTENSIONS
        LogFactory& operator<<(glm::vec1 data);
        LogFactory& operator<<(glm::vec2 data);
        LogFactory& operator<<(glm::vec3 data);
        LogFactory& operator<<(glm::vec4 data);
        LogFactory& operator<<(glm::mat4 data);
        LogFactory& operator<<(glm::mat3 data);
        LogFactory& operator<<(glm::mat2 data);
        LogFactory& operator<<(glm::quat data);
        #endif // ALIB_DISABLE_GLM_EXTENSIONS

        ///For more support,use templates
        template<class T> std::string demangleTypeName(const char * mangledName){
            /*int status;
            std::unique_ptr<char,void(*)(void*)> result(
                abi::__cxa_demangle(mangledName,nullptr,nullptr,&status),
                std::free
            );
            return (status==0)?result.get():mangledName;
*/
	return mangledName;
        }


        #define VALUE_FIX(TESTV) \
                {/*有没有const修饰一样---似乎又不行了*/\
                    if(typeid(TESTV) == typeid(std::string) || typeid(TESTV) == typeid(char *) || typeid(TESTV) == typeid(const char *)){\
                        cachedStr += "\"";\
                    }else if(typeid(TESTV) == typeid(char)){\
                        cachedStr += "\'";\
                    }\
                }

        //std::vector<Type,Allocator>
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

        //std::map<Key,Value,Allocator>
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

        ///std::unordered_map
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

        ///std::tuple
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
        template<class Tuple> struct tuple_show <Tuple,1>{
            static void show(const Tuple&t,LogFactory&lg){
                std::string & cachedStr = lg.cachedStr;
                const auto& vl = std::get<0>(t);
                VALUE_FIX(decltype(vl));
                lg.operator<<(vl);
                VALUE_FIX(decltype(vl));
            }
        };

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

        ///The last one to match
        template<class T>
            LogFactory& operator<<(const T& t){
            cachedStr += alib::g3::ext_toString::toString(t);
            return *this;
        }
    };
}
}

#endif // ALOGGER_H_INCLUDED
