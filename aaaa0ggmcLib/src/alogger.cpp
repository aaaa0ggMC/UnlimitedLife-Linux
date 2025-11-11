#include <alib-g3/alogger.h>

using namespace alib::g3;

thread_local std::string LogMsg::sdate;
thread_local std::string LogMsg::stime;
thread_local std::string LogMsg::scomposed;
std::mutex lot::Console::console_lock;


void Logger::setup_consumer_threads(){
    // 创建runner
    for(size_t i = 0;i < config.consumer_count;++i){
        consumers.emplace_back(&Logger::consumer_func,this);
    }
}

void Logger::consumer_func(){
    // 靠fetch message来填充
    std::vector<LogMsg> messages;

    while(logger_not_on_destroying){
        msg_semaphore.acquire();
        size_t count = fetch_messages(messages);
        // 没取出一个，说明可能虚假唤醒了
        if(!count)continue;
        // 输出数据
        write_messages(std::span(messages).subspan(0,count));
    }
}

void Logger::write_messages(std::span<LogMsg> msgs){
    for(auto & msg : msgs){
        if(!msg.m_nice_one)continue;
        for(auto & filter : filters){
            if(!filter->enabled)continue;
            msg.m_nice_one = filter->filter(msg);
            if(!msg.m_nice_one)break;
        }
    }
    for(size_t i = 0;i < msgs.size();++i){
        auto& t = msgs[i];
        if(!t.m_nice_one)continue;
        
        t.build_on_consumer();
        for(auto& target : targets){
            if(!target->enabled)continue;
            target->write(t);
        }
    }
    // flush targets
    for(auto& target : targets){
        if(!target->enabled)continue;
        target->flush();
    }
}

size_t Logger::fetch_messages(std::vector<LogMsg> & target){
    static LogMsgConfig default_fetch_cfg;
    // 一次性拿完
    size_t i = 0;
    // 对target进行扩容
    while(target.size() < config.fetch_message_count_max){
        // 这里的deault_fetch_cfg就是个占位符，防止悬垂啥的
        target.emplace_back(msg_str_alloc,default_fetch_cfg);
    }
    {
        std::lock_guard<std::mutex> lock(msg_lock);

        size_t cur_size = messages.size();
        if(!cur_size){
            return 0;
        }
        size_t fetch_size = config.fetch_message_count_max<cur_size?
                                config.fetch_message_count_max:cur_size;
        for(;i < fetch_size;++i){
            /// @NOTE 加message请往后面加
            auto & msg = messages.front();
            // 调用move构造从而降低调用
            target[i] = std::move(msg);
            messages.pop_front();
        }
        return fetch_size;
    }
}

Logger::~Logger(){
    logger_not_on_destroying = false;
    // 等待终止线程
    for(int i = 0;i < consumers.size();++i){
        msg_semaphore.release(consumers.size() - i + 1);
        if(consumers[i].joinable())consumers[i].join();
    }

    flush();
}

void Logger::flush(){
    static thread_local std::vector<LogMsg> msgs;
    while(true){
        size_t count = fetch_messages(msgs);
        // 似乎出错了啥的
        if(!count)break;
        write_messages(std::span(msgs).subspan(0,count));
    }
}

bool Logger::push_message_pmr(int level,std::pmr::string & body,LogMsgConfig & cfg){
    // pre filter
    for(auto & filter : filters){
        if(!filter->enabled)continue;
        if(!filter->pre_filter(level,body,cfg))return false;
    }
    LogMsg msg(msg_alloc,cfg);

    msg.level = level;
    msg.body = std::move(body);
    msg.build_on_producer(clk);
     
    {
        std::lock_guard<std::mutex> lock(msg_lock);
        messages.emplace_back(std::move(msg));
    }

    msg_semaphore.release();
    // 不存在consumer自己消化
    /// @TODO 加入back pressure机制，这段代码可以复用
    if(!config.consumer_count && messages.size() >= config.fetch_message_count_max){
        static thread_local std::vector<LogMsg> msgs;
        size_t count = fetch_messages(msgs);
        // 没取出一个，说明可能算错了啥的，基本不会发生
        if(count)write_messages(std::span(msgs).subspan(0,count));
    }
    return true;
}

std::string_view Logger::register_header(std::string_view val){
    if(val.compare("")){
        auto it = std::find(header_pool.begin(),header_pool.end(),val);
        if(it != header_pool.end())return *it;
        return header_pool.emplace_back(val);
    }else return "";
}


void endlog(LogEnd){}