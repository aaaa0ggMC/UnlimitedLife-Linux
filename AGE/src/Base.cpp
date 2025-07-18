#include <AGE/Utils.h>
#include <alib-g3/alogger.h>

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
    infos.emplace_back(info.code,std::pmr::string(info.message,alloc));
    if(limit > 0 && infos.size() >= limit)infos.erase(infos.begin());
    if(trigger)trigger(infos[infos.size()-1]);
}

void Error::defTrigger(const ErrorInfopp& data){
    using namespace alib::g3;
    static Logger console_logger;
    static LogFactory lg("AGE",console_logger);
    static std::shared_ptr<lot::Console> console = std::make_shared<lot::Console>();
    [[maybe_unused]] static bool initeOnce = [&]{
        console_logger.appendLogOutputTarget("console",console);
        return true;
    }();

    lg(LOG_ERROR) << "[" << data.code  << "]" << data.message << endlog;
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