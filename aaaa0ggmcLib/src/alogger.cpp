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
THREAD_LOCAL std::string LogFactory::cachedStr = "";
std::mutex lot::Console::global_lock;

void Logger::flush(){
    for(auto & [_,target] : targets){
        target->flush();
    }
}

void Logger::closeLogFilter(const std::string& name){
    auto result = filters.find(name);
    if(result != filters.end()){
        filters.erase(result);
    }
}

void Logger::closeLogOutputTarget(const std::string& name){
    auto result = targets.find(name);
    if(result != targets.end()){
        targets.erase(result);
    }
}

Logger::~Logger(){
    flush();
    #ifdef _WIN32
    DeleteCriticalSection(&lock);
    #elif __linux__
    pthread_mutex_destroy(&lock);
    pthread_mutexattr_destroy(&mutex_attr);
    #endif // __linux__
}

Logger::Logger(int extra,bool v){
    if(v && (instance == NULL)){
        instance = this;
    }
    showExtra = extra;

    #ifdef _WIN32
    InitializeCriticalSection(&lock);
    #elif __linux__
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr,PTHREAD_MUTEX_RECURSIVE);

    pthread_mutex_init(&lock,&mutex_attr);
    #endif // __linux__
}

std::optional<Logger*> Logger::getStaticLogger(){
    if(instance)return {instance};
    else return std::nullopt;
}

void Logger::setStaticLogger(Logger* l){
    instance = l;
}

int Logger::getShowExtra(){return showExtra;}

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
    bool keepMessage = true;

    for(auto & [_,ft] : filters){
        if(ft->enabled)keepMessage &= ft->pre_filter(level,msg);
    }
    if(!keepMessage)return;

    std::string msg_copy = msg;
    timeHeader.clear();
    restOut.clear();

    makeExtraContent(timeHeader,restOut,head,"",showExtra,false);
    ///Filter
    for(auto &[_,ft] : filters){
        if(ft->enabled)keepMessage &= ft->filter(level,msg_copy,timeHeader,restOut);
    }
    if(!keepMessage)return;

    {
        ///Output
        for(auto &[_,target] : targets){
            CriticalLock cs(lock);
            if(target->enabled)target->write(level,msg_copy,timeHeader,restOut,showExtra);
        }
    }
}

///Enabled 0:false 1:true -1:can't find
int Logger::getLogOutputTargetStatus(const std::string& name){
    auto res = targets.find(name);
    if(res == targets.end())return -1;
    return res->second->enabled;
}
int Logger::getLogFilterStatus(const std::string& name){
    auto res = filters.find(name);
    if(res == filters.end())return -1;
    return res->second->enabled;
}

void Logger::setLogOutputTargetStatus(const std::string& name,bool v){
    auto res = targets.find(name);
    if(res == targets.end())return;
    res->second->enabled = v;
}

void Logger::setLogFilterStatus(const std::string& name,bool v){
    auto res = filters.find(name);
    if(res == filters.end())return;
    res->second->enabled = v;
}

void Logger::appendLogOutputTarget(const std::string &name,std::shared_ptr<LogOutputTarget> target){
    targets.insert_or_assign(name,target);
}

void Logger::appendLogFilter(const std::string & name,std::shared_ptr<LogFilter> filter){
    filters.insert_or_assign(name,filter);
}

void Logger::setShowExtra(int mode){this->showExtra = mode;}

void Logger::makeExtraContent(std::string &tm,std::string& rest,dstring head,dstring showTypeStr,int mode,bool addLg){
    if(mode == LOG_SHOW_NONE)return;
    if(mode & LOG_SHOW_TIME){
        tm.append("[");
        tm.append(Util::ot_getTime());
        tm.append("]");
    }
    if(addLg && mode & LOG_SHOW_TYPE){
        rest.append(showTypeStr);
    }
    if(mode & LOG_SHOW_HEAD && head.compare("")){
        rest.append("[");
        rest.append(head);
        rest.append("]");
    }
    if(mode & LOG_SHOW_ELAP){
        rest.append("[");
        rest.append(std::to_string(clk.getOffset()));
        rest.append("ms]");
    }
    if(mode & LOG_SHOW_PROC){
        rest.append("[PID:");
        rest.append(std::to_string(getpid()));
        rest.append("]");
    }
    if(mode & LOG_SHOW_THID){
        rest.append("[TID:");
        rest.append(std::to_string((int)pthread_self()));
        rest.append("]");
    }
    rest.append(":");
}

std::string Logger::makeMsg(int level,dstring msg,dstring head,bool ends){
    using namespace std;
    static thread_local string rout;
    [[maybe_unused]] static thread_local int simp_init = []{
        rout.reserve(LOG_RESERVE_SIZE);
        return 0;
    }();

    rout.clear();
    makeExtraContent(rout,rout,head,"MakeMSG",showExtra,true);
    rout.append(msg);
    rout.append(ends?"\n":"");
    return rout;
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

LogFactory& LogFactory::operator()(int logLevel){
    defLogType = logLevel;
    return *this;
}

LogFactory& LogFactory::operator<<(glm::vec1 data){
    cachedStr += "(";
    cachedStr += std::to_string(data.x);
    cachedStr += ")";
    return *this;
}

LogFactory& LogFactory::operator<<(glm::vec2 data){
    cachedStr += "(";
    cachedStr += std::to_string(data.x);
    cachedStr += ",";
    cachedStr += std::to_string(data.y);
    cachedStr += ")";
    return *this;
}

LogFactory& LogFactory::operator<<(glm::vec3 data){
    cachedStr += "(";
    cachedStr += std::to_string(data.x);
    cachedStr += ",";
    cachedStr += std::to_string(data.y);
    cachedStr += ",";
    cachedStr += std::to_string(data.z);
    cachedStr += ")";
    return *this;
}

LogFactory& LogFactory::operator<<(glm::vec4 data){
    cachedStr += "(";
    cachedStr += std::to_string(data.x);
    cachedStr += ",";
    cachedStr += std::to_string(data.y);
    cachedStr += ",";
    cachedStr += std::to_string(data.z);
    cachedStr += ",";
    cachedStr += std::to_string(data.w);
    cachedStr += ")";
    return *this;
}

LogFactory& LogFactory::operator<<(glm::quat data){
    cachedStr += "(";
    cachedStr += std::to_string(data.x);
    cachedStr += ",";
    cachedStr += std::to_string(data.y);
    cachedStr += ",";
    cachedStr += std::to_string(data.z);
    cachedStr += ",";
    cachedStr += std::to_string(data.w);
    cachedStr += ")";
    return *this;
}

LogFactory& LogFactory::operator<<(glm::mat2 data){
    //按列->按行 ChatGPT告诉我默认按列的，具体是不是按列我不知道
    glm::mat2 tp = glm::transpose(data);
    const float * dt = glm::value_ptr(tp);
    cachedStr += "[(";
    cachedStr += std::to_string(dt[0]);
    cachedStr += ",";
    cachedStr += std::to_string(dt[1]);
    cachedStr += "),(";
    cachedStr += std::to_string(dt[2]);
    cachedStr += ",";
    cachedStr += std::to_string(dt[3]);
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
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += "),(";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += "),(";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ")]";
    }
    return *this;
}

LogFactory& LogFactory::operator<<(std::ostream& (*manip)(std::ostream&)) {
    operator<<(endlog);
    return *this;
}


LogFactory& LogFactory::operator<<(glm::mat4 data){
    //按列->按行 ChatGPT告诉我默认按列的，具体是不是按列我不知道
    glm::mat4 tp = glm::transpose(data);
    const float * dt = glm::value_ptr(tp);
    {
        unsigned int i = 0;
        cachedStr += "[(";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += "),(";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += "),(";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += "),(";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += std::to_string(dt[i++]);
        cachedStr += ",";
        cachedStr += std::to_string(dt[i++]);
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


///Targets
void LogOutputTarget::write(int logLevel,const std::string & message,const std::string& timeHeader,const std::string& restContent,int ss){}
void LogOutputTarget::flush(){}
void LogOutputTarget::close(){}
LogOutputTarget::LogOutputTarget(){
    ///Nice!
    ///cout << "INited" << endl;
    enabled = true;
}
LogOutputTarget::~LogOutputTarget(){
    ///Nice it will be called!
    //cout << "Called" << endl;
    flush();
    close();
}

namespace alib::g3::lot{
    LogHeader getHeader(int l){
        if(l&LOG_INFO)return {"[INFO]",ACP_YELLOW};
        else if(l&LOG_CRITI)return {"[CRITICAL]",ACP_WHITE ACP_BG_LRED};
        else if(l&LOG_WARN)return {"[WARN]",ACP_BLUE};
        else if(l&LOG_DEBUG)return {"[DEBUG]",ACP_CYAN};
        else if(l&LOG_ERROR)return {"[ERROR]",ACP_RED};
        else if(l&LOG_TRACE)return {"[TRACE]",ACP_WHITE};
        else return {"",0};
    }

    void Console::write(int logLevel,const std::string & message,const std::string& timeHeader,const std::string& restContent,int ss){
        LogHeader header = getHeader(logLevel);
        {
            std::lock_guard<std::mutex> lk(global_lock);
            if(timeHeader.compare(""))std::cout << timeHeader;
            if(ss & LOG_SHOW_TYPE)Util::io_printColor(header.str,header.color);
            std::cout << restContent;
            if(neon != NULL)Util::io_printColor(message,neon);
            else std::cout << message;
            std::cout << "\n";
        }
    }

    void Console::flush(){
        std::cout.flush();
    }

    void Console::close(){}

    void Console::setContentColor(const char * col){
        neon = col;
    }

    void SingleFile::write(int logLevel,const std::string & message,const std::string& timeHeader,const std::string& restContent,int ss){
        if(!nice)return;
        LogHeader header = getHeader(logLevel);
        if(timeHeader.compare(""))ofs << timeHeader;
        if(ss & LOG_SHOW_TYPE)ofs << header.str;
        ofs << restContent << message << "\n";
    }

    void SingleFile::flush(){
        if(nice)ofs.flush();
    }

    void SingleFile::close(){
        if(nice)ofs.close();
        nice = false;
    }

    void SingleFile::open(const std::string & path){
        if(nice)return;
        ofs.open(path);
        if(ofs.bad())return;
        nice = true;
    }

    SingleFile::SingleFile(const std::string& path){
        nice = false;
        open(path);
    }

    void SplittedFiles::write(int logLevel,const std::string & message,const std::string& timeHeader,const std::string& restContent,int ss){
        if(!nice)return;
        LogHeader header = getHeader(logLevel);
        if(timeHeader.compare("")){
            ofs << timeHeader;
            currentBytes += timeHeader.size();
        }
        if(ss & LOG_SHOW_TYPE){
            ofs << header.str;
            currentBytes += strlen(header.str);
        }
        ofs << restContent << message << "\n";
        currentBytes += restContent.size() + message.size() + 1;
        testSwapFile();
    }

    void SplittedFiles::flush(){
        if(nice)ofs.flush();
    }

    void SplittedFiles::close(){
        if(nice)ofs.close();
        nice = false;
        splitIndex = 0;
    }

    void SplittedFiles::testSwapFile(){
        if(currentBytes >= maxBytes){
            splitIndex++;
            currentBytes = 0;
            ofs.close();
            nice = false;
            open(generateFilePath(path,splitIndex));
        }
    }

    void SplittedFiles::open(const std::string & path){
        if(nice)return;
        ofs.open(path);
        if(ofs.bad())return;
        nice = true;
    }

    SplittedFiles::SplittedFiles(const std::string& path,unsigned int maxBytes){
        nice = false;
        splitIndex = 0;
        this->maxBytes = maxBytes;
        this->path = path;
        open(generateFilePath(path,splitIndex));
    }

    std::string SplittedFiles::generateFilePath(const std::string & in,int index){
        auto result = in.find_last_of('.');
        unsigned int pos = (result == std::string::npos)?in.length():(unsigned int)(result);
        std::string ext = in.substr(pos);/// 1234.ext --> ext=".ext"
        std::string pre = in.substr(0,pos);///pre = "1234"
        return pre + " - " + std::to_string(index) + ext;
    }

    unsigned int SplittedFiles::getCurrentIndex(){
        return splitIndex;
    }
}

LogFilter::LogFilter(){
    enabled = true;
}

LogFilter::~LogFilter(){}
bool LogFilter::pre_filter(int,dstring){return true;}
bool LogFilter::filter(int,std::string&,std::string&,std::string&){return true;}

namespace alib::g3::lgf{

    LogLevel::LogLevel(int sl){
        showLevels = sl;
    }

    bool LogLevel::pre_filter(int logLevel,const std::string& originMessage){
        return logLevel & showLevels;
    }

    KeywordsBlocker::KeywordsBlocker(const std::vector<std::string> & kw){
        keywords = kw;
    }

    bool KeywordsBlocker::pre_filter(int logLevel,const std::string& originMessage){
        for(const auto & v : keywords){
            if(originMessage.find(v) != std::string::npos){
                return true;
            }
        }
        return false;
    }

    KeywordsReplacerMono::KeywordsReplacerMono(bool useChar,char ch,const std::string & replacement,const std::vector<std::string> & keys){
        keywords = keys;
        this->useChar = useChar;
        this->ch = ch;
        this->replacement = replacement;
    }

    bool KeywordsReplacerMono::filter(int logLevel,std::string & mainContent,std::string & timeHeader,std::string & extraContent){
        auto pos = std::string::npos;
        const std::string * tg;
        while(true){
            tg = NULL;
            pos = std::string::npos;
            for(const auto & v : keywords){
                pos = mainContent.find(v);
                if(pos != std::string::npos){
                    tg = &v;
                    break;
                }
            }
            if(pos == std::string::npos)return true;
            if(useChar)mainContent.replace(pos,tg->size(),tg->size(),ch);
            else{
                std::string value = mainContent.substr(0,pos);
                std::string rest = mainContent.substr(pos + tg->size());

                mainContent.reserve(value.size() + replacement.size() + rest.size() + 5);
                mainContent.clear();
                mainContent.append(value).append(replacement).append(rest);
            }
        }
        return true;
    }
}