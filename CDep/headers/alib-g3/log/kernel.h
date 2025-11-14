/** @file kernel.h
* @brief 与日志有关的函数库
* @author aaaa0ggmc
* @last-date 2025/04/04
* @date 2025/11/14 
* @version pre-4.0
* @copyright Copyright(C)2025
********************************
@par 修改日志:
<table>
<tr><th>时间       <th>版本         <th>作者          <th>介绍
<tr><td>2025-04-04 <td>3.1         <th>aaaa0ggmc    <td>添加doc
<tr><td>2025-04-04 <td>3.1         <th>aaaa0ggmc    <td>完成doc
<tr><td>2025-11-10 <td>pre-4.0     <th>aaaa0ggmc    <td>准备4.0阶段
<tr><td>2025-11-12 <td>alpha-4.0   <th>aaaa0ggmc    <td>4.0阶段核心完工，优化ing...
</table>
********************************
*/
/**
 * @todo 实现LogManager对log文件（文件夹）进行管理
 */
#ifndef ALOGGER2_H_INCLUDED
#define ALOGGER2_H_INCLUDED
#include <chrono>
#include <alib-g3/autil.h>
#include <alib-g3/aref.h>
#include <alib-g3/aclock.h>

#include <alib-g3/log/streamed_context.h>
#include <alib-g3/log/base_config.h>
#include <alib-g3/log/base_msg.h>
#include <alib-g3/log/base_mod.h>

#include <unordered_map>
#include <deque>
#include <thread>
#include <mutex>
#include <semaphore>
#include <memory_resource>
#include <memory>
#include <span>
#include <type_traits>
#include <iostream>

#ifndef CLOCK_MONOTONIC_COARSE
#ifndef CLOCK_REALTIME_COARSE
#define ALIB_DEF_CLOCK_SOURCE CLOCK_REALTIME
#else
#define ALIB_DEF_CLOCK_SOURCE CLOCK_REALTIME_COARSE
#endif
#else 
#define ALIB_DEF_CLOCK_SOURCE CLOCK_MONOTONIC_COARSE
#endif 

namespace alib::g3{
    /// @brief 最多可以有多少信号，默认最大值就行
    constexpr unsigned int semaphore_max_val = std::__semaphore_impl::_S_max;
    /// @brief Consumer构建的时候预留多少msg槽位（运行时可对齐到fetch_max_size的）
    constexpr unsigned int consumer_message_default_count = 128;
    /// @brief 日期字符串预留的空间，一般格式化没问题的
    constexpr unsigned int date_str_resize = 32;
    /// @brief 每个组合字符串预留的空间，1024可满足小内容日志，运行时也可以根据raw数据大小扩展
    constexpr unsigned int compose_str_resize = 1024;
    /// @brief 时钟源，简易能选择的话选择最快的MONOTIC_COARSE
    constexpr int time_clock_source = ALIB_DEF_CLOCK_SOURCE;

    /// @brief 默认的loglevel标识，纯粹方便使用的
    enum class LogLevel : int{
        Trace = 0, ///< Trace级别，基本上没什么业务意义
        Debug = 1, ///< Debug级别，一般用在Debug环境下
        Info  = 2, ///< Info 级别，普通日志信息
        Warn  = 3, ///< Warn 级别，出了点小问题
        Error = 4, ///< Error级别，问题大了点，能接受
        Fatal = 5  ///< Fatal级别，不能接受的超级大问题
    };

    /// @brief 日志核心，负责日志队列维护、转发
    struct DLL_EXPORT Logger{
        using targets_t = std::vector<std::shared_ptr<LogTarget>>;
        using filters_t = std::vector<std::shared_ptr<LogFilter>>;
    public:
        friend class LogFactory;

        /// L@brief ogger配置
        LoggerConfig config;       

        /// @brief 输出对象缓存
        targets_t targets;
        /// @brief 输出对象查找池
        std::unordered_map<std::string,RefWrapper<targets_t>> search_targets;
        /// @brief 过滤器缓存
        filters_t filters;
        /// @brief 过滤对象查找池
        std::unordered_map<std::string,RefWrapper<filters_t>> search_filters;

        /// @brief Header常量池，只增不减，鉴于LogFactory数目很少
        std::vector<std::string> header_pool;
        /// @brief 专门给消息池的内存池 
        std::pmr::polymorphic_allocator<LogMsg> msg_alloc;
        /// @brief 消息池的resource
        std::pmr::unsynchronized_pool_resource msg_buf;
        
        /// @brief 消息池
        std::pmr::deque<LogMsg> messages;
        /// @brief 消息池大小，减小size()调用啥的
        std::atomic<int> message_size;

        /// @brief 锁，目前还是相信std::mutex的力量
        std::mutex msg_lock;
        /// @brief 信号量，用来实现异步休眠
        std::counting_semaphore<semaphore_max_val> msg_semaphore;
        /// @brief 线程池，用来管理consumer
        std::vector<std::thread> consumers; 
        
        /// @brief 背压阈值
        uint64_t back_pressure_threshold;
        /// @brief 线程能继续运行的flag
        bool logger_not_on_destroying;
        /// @brief 缓存的开始时间
        timespec start_time;

        /// @brief 初始化consumer线程
        void setup_consumer_threads();
        /// @brief Consumer的运行函数
        void consumer_func();

        /// @brief  获取消息链条，不需要clear target
        /// @return 填充了多少项，用于遍历target
        /// @note   注意target的size不会改变，因此遍历必须依赖返回值
        size_t fetch_messages(std::vector<LogMsg> & target);

        /// @brief LogFactory通过这个注册一个header，其自身不持有防止悬垂
        std::string_view register_header(std::string_view val);

        /// @brief 安全地移除一个module
        /// @tparam T         Module类型：LogTarget / LogFilter
        /// @param key        Module存储的key值
        /// @param st         Module对应的索引表
        /// @param container  Module对应的存储容器
        /// @return 操作是否成功
        template<CanAccessItem T> bool safe_remove_mod(
            std::string_view key,
            std::unordered_map<std::string,RefWrapper<T>> & st,
            T & container
        );

        /// @brief 内部处理，会直接调用std::move高效交换数据
        bool push_message_pmr(int level,std::pmr::string & body,LogMsgConfig & cfg);
    public:
        /// @brief 字符串数据池
        std::pmr::polymorphic_allocator<char> msg_str_alloc;
        /// @brief 可能涉及多线程同时操作&分配，因此需要sync，无奈之举
        std::pmr::synchronized_pool_resource msg_str_buf;
        
        /// @brief 初始化内存池，配置......
        Logger(const LoggerConfig & cfg = LoggerConfig());

        /// @brief 刷新所有target
        void flush_targets();
        /// @brief 强制当前线程处理所有在队列的消息，并flush_targets
        void flush();

        /// @brief 将信息写入targets
        /// @param autoflush  是否自动在某位刷新targets,如果span比较小就不建议
        /// @note  注意，为了速度，msg可能会被写入！
        void write_messages(std::span<LogMsg> msg,bool autoflush = true);
        /// @brief  加入一条消息
        /// @note   这里的话感觉不可避免地涉及一次数据copy
        /// @return 操作是否成功
        bool push_message(int level,std::string_view body,LogMsgConfig & cfg);

        /// @brief  添加一个新的mod
        /// @tparam T       继承自LogTarget/LogFilter的类
        /// @param name     该module的名字
        /// @param ...args  对应T构造函数需要的参数
        /// @return 已创建对象的shared_ptr
        template<class T,class... Args> std::shared_ptr<T> 
            append_mod(std::string_view name,Args&&... args);
        /// @brief 移除对应的module
        /// @tparam T LogTarget/LogFilter
        /// @param name 移除的组件的名字
        /// @return 是否移除成功
        template<class T = LogTarget> bool
            remove_mod(std::string_view name);
        /// @brief 清除所有过滤器
        void clear_filter();
        /// @brief 清除所有输出对象
        void clear_target();
        /// @brief 清空所有module，包含targets和filter
        void clear_mod();

        /// @brief 终止线程，清理资源
        ~Logger();
    };

    /// @brief 提供对日志的快捷输出
    struct DLL_EXPORT LogFactory{
        /// @brief 绑定的Logger对象
        Logger& logger;
        /// @brief 头信息，用来在日志中标识logger的
        std::string_view header;
        /// @brief 配置文件
        LogMsgConfig cfg;
        /// @brief 使用<<时未指定level采用的默认数值
        int default_level;

        /// @brief 初始化logger
        /// @param binded     绑定到的日志核心
        /// @param aheader    头信息
        /// @param def_level  默认输出级别
        /// @param cfg        对应配置文件
        LogFactory(
            Logger & binded,
            std::string_view aheader = "",
            int def_level = 0, 
            const LogMsgConfig cfg = LogMsgConfig()
        );

        /// @brief 信息转发到Logger
        inline bool log(int level,std::string_view message){
            return logger.push_message(level,message,cfg);
        }
        /// @brief 信息转发到Logger，适配LogLevel
        inline bool log(LogLevel level,std::string_view message){
            return log(static_cast<int>(level),message);
        }
        /// @brief 支持多参数的转发
        template<class... Args> inline bool log(int level,std::string_view fmt,Args&&... args){
            std::pmr::string str (logger.msg_str_alloc);
            std::vformat_to(std::back_inserter(str),fmt,std::make_format_args(args...));
            return logger.push_message_pmr(level,str,cfg);
        }
        /// @brief 支持多参数的转发，静态版本
        template<class... Args> inline bool log_fast(int level,const std::format_string<Args...>& fmt,Args&&... args){
            std::pmr::string str (logger.msg_str_alloc);
            std::vformat_to(std::back_inserter(str),fmt.get(),std::make_format_args(args...));
            return logger.push_message_pmr(level,str,cfg);
        }
        /// @brief 支持多参数的转发，适配LogLevel
        template<class... Args> inline bool log(LogLevel level,std::string_view fmt,Args&&... args){
            return log(static_cast<int>(level),fmt,std::forward<Args>(args)...);
        }
        /// @brief 支持多参数的转发，静态版本，适配LogLevel
        template<class... Args> inline bool log(LogLevel level,const std::format_string<Args...>& fmt,Args&&... args){
            return log_fast(static_cast<int>(level),fmt,std::forward<Args>(args)...);
        }
        /// @brief 提供已经装载到对应内存的数据，这个时候直接move就行
        /// @note  pmr_data会失效！
        inline bool log_pmr(int level,std::pmr::string & pmr_data){
            return logger.push_message_pmr(level,pmr_data,cfg);
        }

        /// @brief 提供流式输出，这里采用默认的level
        inline StreamedContext<LogFactory> operator()(){
            return StreamedContext<LogFactory>(default_level,*this);
        }
        /// @brief 提供流式输出
        inline StreamedContext<LogFactory> operator()(int spec_level){
            return StreamedContext<LogFactory>(spec_level,*this);
        }
        /// @brief 提供流式输出，适配LogLevel
        inline StreamedContext<LogFactory> operator()(LogLevel spec_level){
            return StreamedContext<LogFactory>(static_cast<int>(spec_level),*this);
        }
        /// @brief 提供流式输出，采用默认的level,这里构造了亡值链，通过RVO减少一次copy
        template<class T> inline StreamedContext<LogFactory> operator<<(T && t){
            return StreamedContext<LogFactory>(default_level,*this) << t;
        } 
    };
};

//// inline实现 ////
namespace alib::g3{
    inline LogMsgConfig::LogMsgConfig(){
        gen_thread_id = false;
        gen_time = true;
        gen_date = true;
        out_header = true;
        out_level = true;
        disable_extra_information = false;
        level_cast = default_level_cast;
    }

    inline std::string_view LogMsgConfig::default_level_cast(int level_in){
        switch(level_in){
        case 0:
            return "TRACE";
        case 1:
            return "DEBUG";
        case 2:
            return "INFO ";
        case 3:
            return "WARN ";
        case 4:
            return "ERROR";
        case 5:
            return "FATAL";
        default:
            return "?????";
        }
    }

    inline LogMsg::LogMsg(const std::pmr::polymorphic_allocator<char> &__a,LogMsgConfig & c)
    :body(__a)
    ,cfg(&c){
        m_nice_one = true;
        generated = false;
    }

    inline void LogMsg::build_on_producer(const timespec & start){
        if(cfg->disable_extra_information)return;
        if(cfg->gen_thread_id){
            thread_id =
            static_cast<uint64_t>(
                std::hash<std::thread::id>{}(std::this_thread::get_id())
            );
        }
        if(cfg->gen_time){
            timespec spec;
            clock_gettime(time_clock_source,&spec);
            timestamp = (spec.tv_sec - start.tv_sec)*1000 + (spec.tv_nsec - start.tv_nsec)/1'000'000.0;
        }
    }

    inline void LogMsg::build_on_consumer(){
        if(cfg->disable_extra_information)return;
        [[maybe_unused]] thread_local static bool inited = [&]{
            sdate.resize(date_str_resize);
            return false;
        }();
        if(cfg->gen_date){
            static thread_local time_t old_time = 0;
            time_t rawtime;
            time(&rawtime);
            if(rawtime != old_time){
                struct tm ptminfo;
                localtime_r(&rawtime,&ptminfo);
                sdate.clear();
                sdate.resize(date_str_resize);
                sdate.resize(snprintf(sdate.data(),date_str_resize,"%02d-%02d-%02d %02d:%02d:%02d",
                    ptminfo.tm_year + 1900, ptminfo.tm_mon + 1, ptminfo.tm_mday,
                    ptminfo.tm_hour, ptminfo.tm_min, ptminfo.tm_sec));
               old_time = rawtime;
            }
        }
    }

    inline std::string_view LogMsg::gen_composed(){
        if(cfg->disable_extra_information){body.push_back('\n');return body;}
        [[maybe_unused]] static thread_local bool inited = [&]{
            scomposed.clear();
            scomposed.resize(compose_str_resize);
            return false;
        }();
        // 只要保证按照LogMsg的顺序递归而不是lot，那么这个就是valid的
        if(generated)return scomposed;
        size_t beg = 0;

        scomposed.clear();
        scomposed.resize(scomposed.capacity());
        if(cfg->gen_date)beg += snprintf(scomposed.data() + beg,scomposed.size() - beg,
            "[%s]",sdate.c_str());
        if(cfg->out_level)beg += snprintf(scomposed.data() + beg,scomposed.size() - beg,
            "[%s]",cfg->level_cast(level).data());
        if(cfg->out_header && header.data() != nullptr && header.size())beg += snprintf(scomposed.data() + beg,scomposed.size() - beg,
            "[%s]",header.data());
        if(cfg->gen_time)beg += snprintf(scomposed.data() + beg,scomposed.size() - beg,
            "[%.2lfms]",timestamp);
        if(cfg->gen_thread_id)beg += snprintf(scomposed.data() + beg,scomposed.size() - beg,
            "[TID%lu]",thread_id);
        scomposed.resize(beg);
        scomposed.append(":");
        
        scomposed += body;
        scomposed.append("\n");
        generated = true;

        return scomposed;
    }

    inline void LogMsg::move_msg(LogMsg&& msg){
        header = msg.header;
        generated = msg.generated;
        m_nice_one = msg.m_nice_one;
        thread_id = msg.thread_id;
        body = std::move(msg.body);
        timestamp = msg.timestamp;
        cfg = msg.cfg;
        level = msg.level;
    }

    inline LoggerConfig::LoggerConfig(){
        // 默认就是单consumer的形式
        consumer_count = 1;
        fetch_message_count_max = consumer_message_default_count;
        back_pressure_multiply = 4; 
        enable_back_pressure = false;
    }

    template<CanAccessItem T> inline bool Logger::safe_remove_mod(
        std::string_view key,
        std::unordered_map<std::string,RefWrapper<T>> & st,
        T & container
    ){
        auto it = st.find(std::string(key));
        if(it == st.end()){
            return false;
        }
        // 旧的target不需要手动关闭，因为析构函数自动关闭了
        // 不懂为什么要先erase it，但是Gemini大王发话了还特别固执
        size_t cached_index = it->second.index;
        st.erase(it);
        // 后面的index都要减少一个
        for(auto& [key,val] : st){
            if(val.index > cached_index)val--;
        }
        container.erase(container.begin() + cached_index);
        return true;
    }

    inline void Logger::flush_targets(){
        // flush targets
        for(auto& target : targets){
            if(!target->enabled)continue;
            target->flush();
        }
    }

    inline bool Logger::push_message(int level,std::string_view body,LogMsgConfig & cfg){
        std::pmr::string str(msg_str_alloc);
        str.assign(body);
        return push_message_pmr(level,str,cfg);
    }

    inline void Logger::clear_filter(){
        search_filters.clear();
        filters.clear();
    }

    inline void Logger::clear_target(){
        search_targets.clear();
        targets.clear();
    }

    /// @brief 清空所有module，包含targets和filter
    inline void Logger::clear_mod(){
        clear_target();
        clear_filter();
    }

    template<class T,class... Args> inline std::shared_ptr<T> Logger::append_mod(std::string_view name,Args&&... args){
        static_assert(std::is_base_of_v<LogTarget,T> || std::is_base_of_v<LogFilter,T>,
            "T must be the derived class of LogTarget or LogFilter!");
        auto ptr = std::make_shared<T>(std::forward<Args>(args)...);
        
        if constexpr(std::is_base_of_v<LogTarget,T>){
            /// @todo 懒得理你
            auto it = search_targets.find(std::string(name));
            if(it == search_targets.end()){
                targets.push_back(ptr);
                search_targets.emplace(name,ref(targets,targets.size() - 1));
            }else{
                it->second = ptr;
            }
        }else{
            auto it = search_filters.find(std::string(name));
            if(it == search_filters.end()){
                filters.push_back(ptr);
                search_filters.emplace(name,ref(filters,filters.size() - 1));
            }else{
                it->second = ptr;
            }
        }
        return ptr;
    }

    template<class T> inline bool Logger::remove_mod(std::string_view name){
        static_assert(std::is_same_v<LogTarget,T> || std::is_same_v<LogFilter,T>,
            "T must be one of LogTarget or LogFilter!");
        if constexpr(std::is_same_v<LogTarget,T>){
            return safe_remove_mod(name,search_targets,targets);
        }else{
            return safe_remove_mod(name,search_filters,filters);
        }
    }

    inline LogFactory::LogFactory(
        Logger & binded,
        std::string_view aheader,
        int def_level, 
        const LogMsgConfig cfg
    ):logger(binded){
        if(aheader.compare("")){
            header = binded.register_header(aheader);
        }else header = "";
        this->cfg = cfg;
        default_level = def_level;
    }
}

#endif