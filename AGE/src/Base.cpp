#include <AGE/Utils.h>
#include <alib5/alogger.h>
#include <stacktrace>
#include <GLFW/glfw3.h>

using namespace age;

std::pmr::unsynchronized_pool_resource Error::pool;
std::pmr::polymorphic_allocator<char> Error::alloc (&pool);
Error Error::def;

void Error::checkOpenGLError(){
    Error &err = Error::def;
    GLint errc = glGetError();

    while(errc != GL_NO_ERROR){
        const char* errStr = (const char *)gluErrorString(errc);
        if(errStr){
            err.pushMessage({errc, errStr});
        }else{
            err.pushMessage({errc, "(GL_INTERNAL_ERROR)"});
        }
        errc = glGetError();  // 继续检查下一个错误
    }
}

Error::Error(): infos(alloc){
    trigger = nullptr;
    limit = -1;
}

void Error::setTrigger(TriggerFunc fn){
    trigger = fn;
}

void Error::pushMessage(const ErrorInfo& info){
    infos.emplace_back(info.code,std::pmr::string(info.message,alloc),info.level);
    if(limit > 0 && infos.size() >= limit)infos.erase(infos.begin());
    if(trigger)trigger(infos[infos.size()-1]);
}

static void fast_replace_all(std::string& s, const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.length(), to);
        pos += to.length();
    }
}

void Error::defTrigger(const ErrorInfopp& data){
    using namespace alib5;
    static Logger console_logger;
    static LogFactory lg(console_logger,LogFactoryConfig("AGE"));
    [[maybe_unused]] static bool initeOnce = [&]{
        console_logger.append_mod<lot::Console>("console");
        return true;
    }();

    if(data.level == LogLevel::Error || data.level == LogLevel::Fatal){
        lg(data.level) << "[" << data.code  << "]" << data.message << "Stacktrace:" << 
        alib5::detail::simplify_stacktrace(std::stacktrace::current())
        << endlog;
    }else{
        lg(data.level) << "[" << data.code  << "]" << data.message << endlog;
    }
}

void Error::setLimit(int32_t count){
    limit = count;
}

float age::getMonitorScale(){
    float scale = 1.0f; // 默认 100% 缩放
    #ifdef _WIN32
        HMONITOR hMonitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
        DEVICE_SCALE_FACTOR scaleFactor;
        if(SUCCEEDED(GetScaleFactorForMonitor(hMonitor, &scaleFactor))){
            scale = static_cast<float>(scaleFactor) / 100.0f; // 转换为浮点数 (例如 125 → 1.25f)
        }
    #elif defined(__linux__)
        glfwInit();
        glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &scale, &scale);
    #endif
    return scale;
}