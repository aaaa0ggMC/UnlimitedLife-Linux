#include <AGE/Base.h>
#include <alib-g3/alogger.h>

using namespace age;

Error::Error(){
    trigger = nullptr;
}

void Error::setTrigger(TriggerFunc fn){
    trigger = fn;
}

void Error::pushMessage(const ErrorInfo& info){
    infos.emplace_back(info.code,std::string(info.message));
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
