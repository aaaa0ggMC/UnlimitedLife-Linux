#include <alib-g3/alogger.h>

using namespace alib::g3;

thread_local std::string LogMsg::sdate;
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
    // 由于这个不是thread_local的，因此能保证资源被正确析构，因此不需要clear
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

void Logger::write_messages(std::span<LogMsg> msgs,bool autoflush){
    if(!filters.empty()){
        for(auto & msg : msgs){
            if(!msg.m_nice_one)continue;
            for(auto & filter : filters){
                if(!filter->enabled)continue;
                msg.m_nice_one = filter->filter(msg);
                if(!msg.m_nice_one)break;
            }
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
    if(autoflush){
        flush_targets();
    }
}

size_t Logger::fetch_messages(std::vector<LogMsg> & target){
    static LogMsgConfig default_fetch_cfg;
    // 一次性拿完
    size_t i = 0;
    size_t fetch_size;
    // 对target进行扩容
    target.resize(config.fetch_message_count_max);
    {
        std::lock_guard<std::mutex> lock(msg_lock);

        size_t cur_size = messages.size();
        if(!cur_size){
            return 0;
        }
        fetch_size = config.fetch_message_count_max<cur_size?
                                config.fetch_message_count_max:cur_size;
        for(;i < fetch_size;++i){
            /// @NOTE 加message请往后面加
            auto & msg = messages.front();
            // 调用move构造从而降低调用
            target[i] = std::move(msg);
            messages.pop_front();
        }
        message_size.fetch_sub(fetch_size,std::memory_order::relaxed);
    }
    return fetch_size;
}

Logger::Logger(const LoggerConfig & cfg)
:msg_buf()
,msg_alloc(&msg_buf)
,messages(msg_alloc)
,msg_str_buf()
,msg_str_alloc(&msg_str_buf)
,msg_semaphore(0){
    config = cfg;
    logger_not_on_destroying = true;
    back_pressure_threshold = cfg.back_pressure_multiply * 
                cfg.fetch_message_count_max * cfg.consumer_count;
    clock_gettime(time_clock_source,&start_time);
    setup_consumer_threads();
}

Logger::~Logger(){
    flush();
    logger_not_on_destroying = false;
    // 等待终止线程
    for(int i = 0;i < consumers.size();++i){
        msg_semaphore.release(consumers.size() - i + 1);
        if(consumers[i].joinable())consumers[i].join();
    }
}

void Logger::flush(){
    static thread_local std::vector<LogMsg> msgs;
    while(true){
        size_t count = fetch_messages(msgs);
        // 似乎出错了啥的
        if(!count)break;
        write_messages(std::span(msgs).subspan(0,count),false);
    }
    msgs.clear();
    flush_targets();
}

bool Logger::push_message_pmr(int level,std::pmr::string & body,LogMsgConfig & cfg){
    // pre filter
    for(auto & filter : filters){
        if(!filter->enabled)continue;
        if(!filter->pre_filter(level,body,cfg))return false;
    }
    LogMsg msg(msg_alloc,cfg);
    size_t msg_sz = 0;

    msg.level = level;
    msg.body = std::move(body);
    msg.build_on_producer(start_time);
   
    if(config.consumer_count){ // 异步模式
        {
            std::lock_guard<std::mutex> lock(msg_lock);
            messages.emplace_back(std::move(msg));
            msg_sz = message_size.fetch_add(1,std::memory_order::relaxed) + 1;
        }
        msg_semaphore.release();

        bool should_digest = (config.enable_back_pressure && (msg_sz >= back_pressure_threshold));
        // 没有线程就别吃了
        if(should_digest){
            static thread_local std::vector<LogMsg> msgs;
            size_t count = fetch_messages(msgs);
            // 没取出一个，说明可能算错了啥的，基本不会发生
            if(count){
                write_messages(std::span(msgs).subspan(0,count));
                msgs.clear();
            }
        }
    }else{ // 同步模式
        // 不自动刷新从而节省性能
        write_messages(std::span(&msg,1),false);
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