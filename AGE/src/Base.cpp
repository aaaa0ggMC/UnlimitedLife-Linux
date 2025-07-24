#include <AGE/Utils.h>
#include <alib-g3/alogger.h>
#include <stacktrace>
#include <iomanip>
#include <cxxabi.h>
#include <regex>
#include <GLFW/glfw3.h>

using namespace age;

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

static const std::string& simplify_desc(const std::string& s){
    static std::string ret;
    ret = s;

    // 快速替换（效率高）
    fast_replace_all(ret, "glm::vec<4, float, (glm::qualifier)0>", "vec4");
    fast_replace_all(ret, "glm::vec<3, float, (glm::qualifier)0>", "vec3");
    fast_replace_all(ret, "glm::vec<2, float, (glm::qualifier)0>", "vec2");
    fast_replace_all(ret, "glm::vec4", "vec4");
    fast_replace_all(ret, "age::light::uploaders::", "alu$");
    fast_replace_all(ret, "age::light::", "al$");
    fast_replace_all(ret, "age::", "a$");
    fast_replace_all(ret, "std::basic_string<char>", "std::string");
    fast_replace_all(ret, "std::function<void (vec4 const&)>", "std::function");
    fast_replace_all(ret, "std::__invoke_impl", "invoke");
    fast_replace_all(ret, "std::__invoke_result", "invoke_result");
    fast_replace_all(ret, "std::_Function_handler", "function_handler");

    // regex 替换（处理可变 lambda / tuple 等）
    static const std::vector<std::pair<std::regex, std::string>> regex_rules = {
        {std::regex("\\{lambda\\([^}]*\\)#(\\d+)\\}"), "λ#\\1"},
        {std::regex("operator\\(\\)<[^>]+>"), "operator()"},
        {std::regex("decltype\\(auto\\)"), "auto"},
        {std::regex("std::tuple<[^>]+>"), "tuple<>"},
        {std::regex("std::integer_sequence<[^>]+>"), "int_seq"},
        {std::regex("std::__apply_impl<[^>]+>"), "apply_impl"},
        {std::regex("std::__invoke<[^>]+>"), "invoke"},
        {std::regex("std::enable_if<[^>]+>"), "enable_if"},
        {std::regex("std::function<[^>]+>"), "std::function"},
    };

    for (const auto& [pattern, repl] : regex_rules) {
        ret = std::regex_replace(ret, pattern, repl);
    }

    return ret;
}

static void simplify_stacktrace(const decltype(std::stacktrace::current()) & st) {
    uint index = 0;
    for(auto & entry : st){
        if (entry.source_file().empty() && entry.source_line() == 0)continue;
        std::cout << "  ";
        alib::g3::Util::io_printColor((std::to_string(index) + "# "),ACP_RED);
        alib::g3::Util::io_printColor(simplify_desc(entry.description()),ACP_GRAY);
        std::cout << " at ";
        alib::g3::Util::io_printColor(entry.source_file(),ACP_GREEN);
        std::cout << ":";
        alib::g3::Util::io_printColor(std::to_string(entry.source_line()),ACP_YELLOW);
        std::cout << "\n";
#ifndef AGE_TRACE_COMPACT
        std::cout << "\n";
#endif
        index++;
    }
}

void Error::defTrigger(const ErrorInfopp& data){
    using namespace alib::g3;
    static Logger console_logger;
    static LogFactory lg("AGE",console_logger);
    static std::shared_ptr<lot::Console> console = std::make_shared<lot::Console>();
    static std::stringstream trace_data;
    int serverance = static_cast<int>(data.level);
    [[maybe_unused]] static bool initeOnce = [&]{
        console_logger.appendLogOutputTarget("console",console);
        return true;
    }();
    
    if(serverance == 0){
        serverance = LOG_ERROR; // for some code that doesnt init level
    }

    if(serverance == LOG_ERROR || serverance == LOG_CRITI){
        lg(serverance) << "[" << data.code  << "]" << data.message << "Stacktrace:" << endlog;
        simplify_stacktrace(std::stacktrace::current());
    }else{
        lg(serverance) << "[" << data.code  << "]" << data.message << endlog;
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