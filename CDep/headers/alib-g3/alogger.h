///Not c linkage
#ifndef ALOGGER_H_INCLUDED
#define ALOGGER_H_INCLUDED
#include <memory>
#include <string>
#include <fstream>
#include <optional>
#include <alib-g3/aclock.h>
#include <alib-g3/autil.h>
#ifdef __linux__
#include <pthread.h>
#define LOCK pthread_mutex_t
#elif _WIN32
#define LOCK CRITICAL_SECTION
#endif // __linux__

#ifndef ALIB_DISABLE_GLM_EXTENSIONS
#include <glm/glm.hpp>
#include <glm/ext/quaternion_common.hpp>
#endif // ALIB_DISABLE_GLM_EXTENSIONS

#define LOG_TRACE 0x00000001
#define LOG_DEBUG 0x00000010
#define LOG_INFO  0x00000100
#define LOG_WARN  0x00001000
#define LOG_ERROR 0x00010000
#define LOG_CRITI 0x00100000
#define LOG_OFF   0x01000000

#define LOG_FULL (LOG_TRACE | LOG_DEBUG | LOG_INFO | LOG_WARN | LOG_ERROR | LOG_CRITI)
#define LOG_RELE (LOG_INFO | LOG_ERROR | LOG_CRITI)

#define LOG_SHOW_TIME 0x00000001
#define LOG_SHOW_TYPE 0x00000010
#define LOG_SHOW_ELAP 0x00000100
#define LOG_SHOW_THID 0x00001000
#define LOG_SHOW_HEAD 0x00010000
#define LOG_SHOW_PROC 0x00100000

#define LOG_SH_BASIC (LOG_SHOW_TIME | LOG_SHOW_TYPE | LOG_SHOW_ELAP | LOG_SHOW_HEAD)
#define LOG_SH_FULL  (LOG_SH_BASIC | LOG_SHOW_THID | LOG_SHOW_PROC)

namespace alib{
namespace g3{

    struct DLL_EXPORT LogHeader{
        const char * str;
        int color;
    };

    struct DLL_EXPORT CriticalLock{
        LOCK * cs;
        CriticalLock(LOCK &);
        ~CriticalLock();
    };

    class DLL_EXPORT Logger{
    private:
        friend class LogFactory;
        bool output2c;
        bool m_inited;
        bool splitFiles;
        unsigned long singleLogMaxBytes;
        unsigned int splitIndex;
        unsigned long currentFileBytes;
        int mode;
        int showlg;
        std::ofstream ofs;
        std::string buffer;
        std::string logFile;
        Clock clk;
        LOCK cs;
        #ifdef __linux__
        pthread_mutexattr_t mutex_attr;
        #endif // __linux__
        static Logger * instance;
    public:
        int neon_color;

        Logger(bool outputToConsole = true,bool setInstanceIfNULL = true,int logVisibilities = LOG_FULL);
        ~Logger();

        void log(int level,dstring content,dstring head);
        void flush();
        void close();

        bool setOutputFile(dstring path);
        void setOutputToConsole(bool outputToConsole);
        void setShowExtra(int show);
        void setLogVisibilities(int visibleLogs);
        ///Minium: one message
        void setSplitFiles(bool v);
        ///Only truly works when split files is on,min 1
        void setSingleFileMaxSize(unsigned long maxBytes);

        ///Return: has relevant log file to write in
        bool getLoggerStatus();
        int getShowExtra();
        int getLogVisibilities();
        bool getSplitFiles();
        unsigned long getSingleFileMaxSize();
        std::string getCurrentLogFile();

        void trySwapLogFile();

        static std::string generateFilePath(const std::string & origin,int index);

        std::string makeMsg(int level,dstring data,dstring head,bool ends = true);
        static LogHeader generateHeader(int level);
        static void setStaticLogger(Logger*);
        static std::optional<Logger*> getStaticLogger();
    };


    struct DLL_EXPORT LogEnd{};
    void DLL_EXPORT endlog(LogEnd);
    typedef void (*EndLogFn)(LogEnd);


    class DLL_EXPORT LogFactory{
    private:
        std::string head;
        static thread_local std::string cachedStr;
        Logger* i;
        int defLogType;
    public:
        LogFactory(dstring head,Logger& lg);

        void log(int level,dstring msg);
        void info(dstring msg);
        void error(dstring msg);
        void critical(dstring msg);
        void debug(dstring msg);
        void trace(dstring msg);
        void warn(dstring msg);

        //it's composed
        void setContentColor(int color);
        int getContentColor();

        LogFactory& operator()(int logType = LOG_INFO,int content_color = -1);

        ///Multithread is not supported below!!!!
        LogFactory& operator<<(dstring data);

        LogFactory& operator<<(int data);
        LogFactory& operator<<(float data);
        LogFactory& operator<<(double data);
        LogFactory& operator<<(long data);
        LogFactory& operator<<(unsigned int data);
        LogFactory& operator<<(unsigned long data);
        LogFactory& operator<<(char data);

        ///end the log
        LogFactory& operator<<(EndLogFn fn);

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

    };
}
}

#endif // ALOGGER_H_INCLUDED
