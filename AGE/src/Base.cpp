#include <AGE/Utils.h>
#include <alib-g3/alogger.h>
#include <stacktrace>
#include <iomanip>
#include <cxxabi.h>
#include <regex>

using namespace age;

static const std::string& demangle(const std::string& mangled) {
    static thread_local std::string demangled_name;  // 每个线程独立存储
    int status = 0;
    char* result = abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
    if (result) {
        demangled_name = result;
        free(result);
    } else {
        demangled_name = mangled;
    }

    //进行简写处理
    if(demangled_name.find("std::basic_") != std::string::npos) {
        demangled_name = std::regex_replace(
            demangled_name,
            std::regex("std::basic_string_view<char, std::char_traits<char>\\s*>"),
            "std::string_view"
        );
        demangled_name = std::regex_replace(
            demangled_name,
            std::regex("std::basic_string<char, std::char_traits<char>, std::allocator<char>\\s*>"),
            "std::string"
        );
        demangled_name = std::regex_replace(
            demangled_name,
            std::regex("std::basic_(i|o|f)?stream<char, std::char_traits<char>\\s*>"),
            "std::$1stream"
        );
        demangled_name = std::regex_replace(
            demangled_name,
            std::regex("std::basic_(stringbuf|regex|ios)<char(, std::char_traits<char>)?\\s*>"),
            "std::$1"
        );
    }
    return demangled_name;
}

std::pmr::unsynchronized_pool_resource Error::pool;
std::pmr::polymorphic_allocator<char> Error::alloc (&pool);
Error Error::def;

Error::Error(): infos(alloc){
    trigger = nullptr;
    limit = -1;
}

void Error::setTrigger(TriggerFunc fn){
    trigger = fn;
}

void Error::pushMessage(const ErrorInfo& info){
    infos.emplace_back(info.code,std::pmr::string(info.message,alloc));
    if(limit > 0 && infos.size() >= limit)infos.erase(infos.begin());
    if(trigger)trigger(infos[infos.size()-1]);
}

void Error::defTrigger(const ErrorInfopp& data){
    using namespace alib::g3;
    static Logger console_logger;
    static LogFactory lg("AGE",console_logger);
    static std::shared_ptr<lot::Console> console = std::make_shared<lot::Console>();
    static std::stringstream trace_data;
    [[maybe_unused]] static bool initeOnce = [&]{
        console_logger.appendLogOutputTarget("console",console);
        return true;
    }();
    
    
    trace_data.clear();
    trace_data << std::stacktrace::current();
    lg(LOG_ERROR) << "[" << data.code  << "]" << data.message << "Stacktrace:\n" << trace_data.str() << endlog;
}

void Error::setLimit(int32_t count){
    limit = count;
}

float age::getMonitorScale(){
    #ifdef _WIN32
        float scale = 1.0f; // 默认 100% 缩放
        HMONITOR hMonitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
        DEVICE_SCALE_FACTOR scaleFactor;
        if(SUCCEEDED(GetScaleFactorForMonitor(hMonitor, &scaleFactor))){
            scale = static_cast<float>(scaleFactor) / 100.0f; // 转换为浮点数 (例如 125 → 1.25f)
        }
    #elif defined(__linux__)
        scale = 1.0f;
    #endif
    return scale;
}