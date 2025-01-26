#define SIMPLE_SPD
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <malloc.h>
#include <alib-g3/alogger.h>
#include <alib-g3/autil.h>
#include <glm/gtc/type_ptr.hpp>

#define LOG_RESERVE_SIZE 512

using namespace alib::g3;

Logger * Logger::instance;
thread_local std::string LogFactory::cachedStr = "";

bool Logger::setOutputFile(dstring path){
    CriticalLock lock(cs);
    if(m_inited){
        log(LOG_ERROR,"You have already opened a log file!","ALib::Logger");
        return false;
    }
    if(splitFiles)ofs.open(generateFilePath(path,0),ios::out | ios::trunc);
    else ofs.open(path,ios::out | ios::trunc);
    m_inited = ofs.good();
    if(!m_inited){
        log(LOG_ERROR,"Cannot open the log file!","ALib::Logger");
        return false;
    }
    logFile = path;
    if(buffer.compare("")){
        ofs << buffer;
        currentFileBytes += buffer.size();
        buffer = "";
        trySwapLogFile();
    }
    return m_inited;
}
void Logger::flush(){
    if(output2c)std::cout.flush();
    if(!m_inited)return;
    ofs.flush();
}
void Logger::close(){
    if(!m_inited){
        log(LOG_ERROR,"You havnt inited ALib::Logger with setOutputFile!","ALib::Logger");
        return;
    }
    this->flush();
    ofs.close();
    m_inited = false;
}

Logger::~Logger(){
    ///Close
    if(output2c)std::cout.flush();
    if(m_inited)this->close();
    #ifdef _WIN32
    DeleteCriticalSection(&cs);
    #elif __linux__
    pthread_mutex_destroy(&cs);
    pthread_mutexattr_destroy(&mutex_attr);
    #endif // __linux__
}

Logger::Logger(bool otc,bool v,int lg){
    m_inited = false;
    if(v && (instance == NULL)){
        instance = this;
    }
    output2c = otc;
    mode = LOG_SH_BASIC;
    buffer = "";
    neon_color = -1;
    setLogVisibilities(lg);
    splitFiles = false;
    singleLogMaxBytes = 1024 * 1024;
    currentFileBytes = 0;
    splitIndex = 0;
    #ifdef _WIN32
    InitializeCriticalSection(&cs);
    #elif __linux__
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr,PTHREAD_MUTEX_RECURSIVE);

    pthread_mutex_init(&cs,&mutex_attr);
    #endif // __linux__
}

std::optional<Logger*> Logger::getStaticLogger(){
    if(instance)return {instance};
    else return std::nullopt;
}

void Logger::setStaticLogger(Logger* l){
    instance = l;
}

void Logger::setOutputToConsole(bool value){
    this->output2c = value;
}

bool Logger::getLoggerStatus(){
    return m_inited;
}

LogHeader Logger::generateHeader(int l){
    if(l&LOG_INFO)return {"[INFO]",0x0E};
    else if(l&LOG_CRITI)return {"[CRITICAL]",0x4f};
    else if(l&LOG_WARN)return {"[WARN]",0x09};
    else if(l&LOG_DEBUG)return {"[DEBUG]",0x0f};
    else if(l&LOG_ERROR)return {"[ERROR]",0xf4};
    else if(l&LOG_TRACE)return {"[TRACE]",0x0f};
    else return {"",0};
}

int Logger::getShowExtra(){return mode;}

int Logger::getLogVisibilities(){return showlg;}

CriticalLock::CriticalLock(LOCK & c){
    cs = &c;
    #ifdef _WIN32
    EnterCriticalSection(cs);
    #elif __linux__
    //cout << "Locked" << endl;
    pthread_mutex_lock(cs);
    #endif // _WIN32
}

CriticalLock::~CriticalLock(){
    #ifdef _WIN32
    LeaveCriticalSection(cs);
    #elif __linux__
    pthread_mutex_unlock(cs);
    #endif // _WIN32
}

void Logger::log(int level,dstring msg,dstring head){
    using namespace std;
    static thread_local string timeHeader;
    static thread_local string restOut;
    [[maybe_unused]] static thread_local int simp_init = [&]{
        timeHeader.reserve(64);
        restOut.reserve(LOG_RESERVE_SIZE);
        return 0;
    }();
    if(level & LOG_OFF || !(showlg & level))return;
    LogHeader ist = generateHeader(level);

    timeHeader.clear();
    restOut.clear();

    if(mode & LOG_SHOW_TIME){
        timeHeader.append("[");
        timeHeader.append(Util::ot_getTime());
        timeHeader.append("]");
    }
    if(mode & LOG_SHOW_HEAD && head.compare("")){
        restOut.append("[");
        restOut.append(head);
        restOut.append("]");
    }
    if(mode & LOG_SHOW_ELAP){
        restOut.append("[");
        restOut.append(to_string(clk.getOffset()));
        restOut.append(" ms]");
    }
    if(mode & LOG_SHOW_PROC){
        restOut.append("[PID:");
        restOut.append(to_string(getpid()));
        restOut.append("]");
    }
    if(mode & LOG_SHOW_THID){
        restOut.append("[TID:");
        restOut.append(to_string((int)pthread_self()));
        restOut.append("]");
    }
    restOut.append(":");

    if(m_inited){
        CriticalLock lock(cs);
        const char * v = (mode & LOG_SHOW_TYPE)?(ist.str):("");
        ofs << timeHeader << v;
        ofs << restOut << msg << "\n";
        currentFileBytes += timeHeader.size() + strlen(v) + restOut.size() + msg.size() + 1;
        trySwapLogFile();
    }else{
        CriticalLock lock(cs);
        buffer += timeHeader;
        buffer += (mode & LOG_SHOW_TYPE)?ist.str:"";
        buffer += restOut;
        buffer += msg;
        buffer += "\n";
    }
    if(output2c){
        CriticalLock lock(cs);
        cout << timeHeader;
        if(mode & LOG_SHOW_TYPE)Util::io_printColor(ist.str,ist.color);
        cout << restOut;
        if(neon_color != -1){
            Util::io_printColor(msg,neon_color);
        }else cout << msg;
        cout << endl;
    }
}

void Logger::setShowExtra(int mode){this->mode = mode;}

void Logger::setLogVisibilities(int logs){showlg = logs;}

std::string Logger::makeMsg(int level,dstring & msg,dstring &head,bool ends){
    using namespace std;
    LogHeader ist = generateHeader(level);
    static thread_local string rout;
    [[maybe_unused]] static thread_local int simp_init = []{
        rout.reserve(LOG_RESERVE_SIZE);
        return 0;
    }();

    rout.clear();

    if(mode & LOG_SHOW_TIME){
        rout.append("[");
        rout.append(Util::ot_getTime());
        rout.append("]");
    }
    if(mode & LOG_SHOW_TYPE){
        rout.append(ist.str);
    }
    if(mode & LOG_SHOW_HEAD && head.compare("")){
        rout.append("[");
        rout.append(head);
        rout.append("]");
    }
    if(mode & LOG_SHOW_ELAP){
        rout.append("[");
        rout.append(to_string(clk.getOffset()));
        rout.append("ms]");
    }
    if(mode & LOG_SHOW_PROC){
        rout.append("[PID:");
        rout.append(to_string(getpid()));
        rout.append("]");
    }
    if(mode & LOG_SHOW_THID){
        rout.append("[TID:");
        rout.append(to_string((int)pthread_self()));
        rout.append("]");
    }
    rout.append(":");
    rout.append(msg);
    rout.append(ends?"\n":"");
    return rout;
}

void Logger::trySwapLogFile(){
    if(!splitFiles){
        //log(LOG_WARN,"You havent enabled split files!","ALib::Logger");
        return;
    }
    if(currentFileBytes >= singleLogMaxBytes){
        currentFileBytes = 0;
        ofs.close();
        ++splitIndex;
        ofs.open(generateFilePath(logFile,splitIndex),ios::out | ios::trunc);
        if(ofs.bad()){
            //log(LOG_WARN,"Cannot open a new log file!","ALib::Logger");
            m_inited = false;
            return;
        }
    }
}

std::string Logger::generateFilePath(const std::string& in,int index){
    auto result = in.find_last_of('.');
    unsigned int pos = (result == std::string::npos)?in.length():(unsigned int)(result);
    std::string ext = in.substr(pos);/// 1234.ext --> ext=".ext"
    std::string pre = in.substr(0,pos);///pre = "1234"
    return pre + " - " + std::to_string(index) + ext;
}

void Logger::setSplitFiles(bool v){
    splitFiles = v;
}

bool Logger::getSplitFiles(){
    return splitFiles;
}

void Logger::setSingleFileMaxSize(unsigned long maxBytes){
    singleLogMaxBytes = maxBytes?maxBytes:1;
}

unsigned long Logger::getSingleFileMaxSize(){
    return singleLogMaxBytes;
}

std::string Logger::getCurrentLogFile(){
    if(splitFiles){
        return generateFilePath(logFile,splitIndex);
    }else return logFile;
}

void LogFactory::info(dstring msg){i->log(LOG_INFO,msg,head);}
void LogFactory::error(dstring msg){i->log(LOG_ERROR,msg,head);}
void LogFactory::critical(dstring msg){i->log(LOG_CRITI,msg,head);}
void LogFactory::debug(dstring msg){i->log(LOG_DEBUG,msg,head);}
void LogFactory::trace(dstring msg){i->log(LOG_TRACE,msg,head);}
void LogFactory::warn(dstring msg){i->log(LOG_WARN,msg,head);}

LogFactory::LogFactory(dstring a,Logger & c){
    head = a;
    defLogType = LOG_TRACE;
    i = &c;
    cachedStr = "";
    cachedStr.reserve(LOG_RESERVE_SIZE);
    showContainerName = false;
}

void LogFactory::log(int l,dstring m){i->log(l,m,head);}

void LogFactory::setShowContainerName(bool v){
    showContainerName = v;
}

LogFactory& LogFactory::operator()(int logLevel,int nc){
    defLogType = logLevel;
    i->neon_color = nc;
    return *this;
}

LogFactory& LogFactory::operator<<(dstring data){
    cachedStr += data;
    return *this;
}

LogFactory& LogFactory::operator<<(int data){
    cachedStr += to_string(data);
    return *this;
}
LogFactory& LogFactory::operator<<(float data){
    cachedStr += to_string(data);
    return *this;
}
LogFactory& LogFactory::operator<<(double data){
    cachedStr += to_string(data);
    return *this;
}
LogFactory& LogFactory::operator<<(long data){
    cachedStr += to_string(data);
    return *this;
}

LogFactory& LogFactory::operator<<(unsigned int data){
    cachedStr += to_string(data);
    return *this;
}

LogFactory& LogFactory::operator<<(unsigned long data){
    cachedStr += to_string(data);
    return *this;
}

LogFactory& LogFactory::operator<<(char data){
    cachedStr += data;
    return *this;
}

LogFactory& LogFactory::operator<<(glm::vec1 data){
    cachedStr += "(";
    cachedStr += to_string(data.x);
    cachedStr += ")";
    return *this;
}

LogFactory& LogFactory::operator<<(glm::vec2 data){
    cachedStr += "(";
    cachedStr += to_string(data.x);
    cachedStr += ",";
    cachedStr += to_string(data.y);
    cachedStr += ")";
    return *this;
}

LogFactory& LogFactory::operator<<(glm::vec3 data){
    cachedStr += "(";
    cachedStr += to_string(data.x);
    cachedStr += ",";
    cachedStr += to_string(data.y);
    cachedStr += ",";
    cachedStr += to_string(data.z);
    cachedStr += ")";
    return *this;
}

LogFactory& LogFactory::operator<<(glm::vec4 data){
    cachedStr += "(";
    cachedStr += to_string(data.x);
    cachedStr += ",";
    cachedStr += to_string(data.y);
    cachedStr += ",";
    cachedStr += to_string(data.z);
    cachedStr += ",";
    cachedStr += to_string(data.w);
    cachedStr += ")";
    return *this;
}

LogFactory& LogFactory::operator<<(glm::quat data){
    cachedStr += "(";
    cachedStr += to_string(data.x);
    cachedStr += ",";
    cachedStr += to_string(data.y);
    cachedStr += ",";
    cachedStr += to_string(data.z);
    cachedStr += ",";
    cachedStr += to_string(data.w);
    cachedStr += ")";
    return *this;
}

LogFactory& LogFactory::operator<<(glm::mat2 data){
    //按列->按行 ChatGPT告诉我默认按列的，具体是不是按列我不知道
    glm::mat2 tp = glm::transpose(data);
    const float * dt = glm::value_ptr(tp);
    cachedStr += "[(";
    cachedStr += to_string(dt[0]);
    cachedStr += ",";
    cachedStr += to_string(dt[1]);
    cachedStr += "),(";
    cachedStr += to_string(dt[2]);
    cachedStr += ",";
    cachedStr += to_string(dt[3]);
    cachedStr += ")]";
    return *this;
}

LogFactory& LogFactory::operator<<(glm::mat3 data){
    //按列->按行 ChatGPT告诉我默认按列的，具体是不是按列我不知道
    glm::mat3 tp = glm::transpose(data);
    const float * dt = glm::value_ptr(tp);
    {
        unsigned int i = 0;
        cachedStr += "[(";
        cachedStr += to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += to_string(dt[i++]);
        cachedStr += "),(";
        cachedStr += to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += to_string(dt[i++]);
        cachedStr += "),(";
        cachedStr += to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += to_string(dt[i++]);
        cachedStr += ")]";
    }
    return *this;
}


LogFactory& LogFactory::operator<<(glm::mat4 data){
    //按列->按行 ChatGPT告诉我默认按列的，具体是不是按列我不知道
    glm::mat4 tp = glm::transpose(data);
    const float * dt = glm::value_ptr(tp);
    {
        unsigned int i = 0;
        cachedStr += "[(";
        cachedStr += to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += to_string(dt[i++]);
        cachedStr += "),(";
        cachedStr += to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += to_string(dt[i++]);
        cachedStr += "),(";
        cachedStr += to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += to_string(dt[i++]);
        cachedStr += "),(";
        cachedStr += to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += to_string(dt[i++]);
        cachedStr += ")]";
    }
    return *this;
}

void alib::g3::endlog(LogEnd){}

LogFactory& LogFactory::operator<<(EndLogFn fn){
    i->log(defLogType,cachedStr,head);
    defLogType = LOG_TRACE;
    cachedStr.clear();
    return * this;
}

void LogFactory::setContentColor(int color){
    i->neon_color = color;
}

int LogFactory::getContentColor(){
    return i->neon_color;
}
