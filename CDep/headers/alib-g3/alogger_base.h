/** @file alogger_base.h
* @brief 与日志有关的函数库
* @author aaaa0ggmc
* @last-date 2025/04/04
* @date 2025/11/11 
* @version pre-4.0
* @copyright Copyright(C)2025
********************************
@par 修改日志:
<table>
<tr><th>时间       <th>版本         <th>作者          <th>介绍
<tr><td>2025-04-04 <td>3.1         <th>aaaa0ggmc    <td>添加doc
<tr><td>2025-04-04 <td>3.1         <th>aaaa0ggmc    <td>完成doc
<tr><td>2025-11-10 <td>pre-4.0     <th>aaaa0ggmc    <td>准备4.0阶段
</table>
********************************
*/
/** @todo Logger添加appendConsole appendFile ...实现简化（不然有点麻烦）
 * @todo 实现LogManager对log文件（文件夹）进行管理
 */
#ifndef ALOGGER2_H_INCLUDED
#define ALOGGER2_H_INCLUDED
#include <alib-g3/autil.h>
#include <alib-g3/aref.h>
#include <alib-g3/aclock.h>
#include <alib-g3/alogger_streamed_context.h>

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

namespace alib::g3{
    constexpr unsigned int semaphore_max_val = 4096;
    constexpr unsigned int consumer_message_default_count = 64;
    constexpr unsigned int date_str_resize = 32;
    constexpr unsigned int compose_str_resize = 1024;

    enum class LogLevel : int{
        Trace = 0,
        Debug,
        Info,
        Warn,
        Error,
        Fatal
    };

    struct DLL_EXPORT LogMsgConfig{
        using LevelCastFn = const char* (*)(int);
        
        bool gen_thread_id;
        bool gen_time;
        bool gen_date;

        bool out_header;
        bool out_level;

        LevelCastFn level_cast;

        inline static const char * default_level_cast(int level_in){
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

        inline LogMsgConfig(){
            gen_thread_id = false;
            gen_time = true;
            gen_date = true;
            out_header = true;
            out_level = true;

            level_cast = default_level_cast;
        }
    };

    struct DLL_EXPORT LogMsg{
    private:
        friend class Logger;
        bool m_nice_one;
        bool generated;
    public:
        // Header其实就是每个LogFactory中的数据，也就是说你要保证LogFactory不悬垂，而正常情况下谁会把LogFactory创建之后立马删了？
        // 不过为了防止出现这种神人情况，这里引用的是Logger中的字符串常量池，不会销毁（因为LogFactory数量顶多几百）
        std::string_view header;
        std::pmr::string body;
        uint64_t thread_id;
        double timestamp;
        LogMsgConfig * cfg;
        int level;

        static thread_local std::string sdate;
        static thread_local std::string stime;
        static thread_local std::string scomposed;

        inline LogMsg(const std::pmr::polymorphic_allocator<char> &__a,LogMsgConfig & c)
        :body(__a)
        ,cfg(&c){
            m_nice_one = true;
            generated = false;
        }

        inline void build_on_producer(Clock & clk){
            if(cfg->gen_thread_id){
                thread_id =
                static_cast<uint64_t>(
                    std::hash<std::thread::id>{}(std::this_thread::get_id())
                );
            }
            if(cfg->gen_time){
                timestamp = clk.getAllTime();
            }
        }

        inline void build_on_consumer(){
            [[maybe_unused]] thread_local bool inited = [&]{
                stime.reserve(date_str_resize);
                sdate.resize(date_str_resize);
                return false;
            }();
            if(cfg->gen_date){
                sdate.clear();
                time_t rawtime;
                struct tm ptminfo;
                time(&rawtime);
                localtime_r(&rawtime,&ptminfo);
                snprintf(sdate.data(),date_str_resize,"%02d-%02d-%02d %02d:%02d:%02d",
                        ptminfo.tm_year + 1900, ptminfo.tm_mon + 1, ptminfo.tm_mday,
                        ptminfo.tm_hour, ptminfo.tm_min, ptminfo.tm_sec);
            }
        }

        inline std::string_view gen_composed(){
            [[maybe_unused]] thread_local bool inited = [&]{
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
                "[%s]",cfg->level_cast(level));
            if(cfg->out_header && header.data() != nullptr && header.size())beg += snprintf(scomposed.data() + beg,scomposed.size() - beg,
                "[%s]",header.data());
            if(cfg->gen_time)beg += snprintf(scomposed.data() + beg,scomposed.size() - beg,
                "[%.2lfms]",timestamp);
            if(cfg->gen_thread_id)beg += snprintf(scomposed.data() + beg,scomposed.size() - beg,
                "[TID%lu]",thread_id);
            scomposed.resize(beg);
            scomposed.append(":");
            scomposed += body;
            generated = true;

            return scomposed;
        }

        inline void move_msg(LogMsg&& msg){
            header = msg.header;
            generated = msg.generated;
            m_nice_one = msg.m_nice_one;
            thread_id = msg.thread_id;
            body = std::move(msg.body);
            timestamp = msg.timestamp;
            cfg = msg.cfg;
            level = msg.level;
        }

        inline LogMsg& operator=(LogMsg&& msg){
            move_msg(std::forward<LogMsg>(msg));
            return *this;
        }

        inline LogMsg(LogMsg&& msg){
            move_msg(std::forward<LogMsg>(msg));
        }
    };

    struct DLL_EXPORT LogTarget{
        bool enabled; ///< 用于toggle输出，一般不用管
       
        inline LogTarget(){
            enabled = true;
        }
        
        /// @note 如果节约内存修改logmsg也不是不行
        virtual inline void write(
            LogMsg & msg
        ){}

        /** @brief 刷新数据一般文件系统,网络需要
         * @see ~LogOutputTarget()
         */
        virtual inline void flush(){}
        
        /** @brief 关闭IO
         *  @see ~LogOutputTarget()
         */
        virtual inline void close(){}

        /// @note 这里会自动帮你 flush() 与 close(),注意哦！！
        virtual ~LogTarget(){
            flush();
            close();
        }
    };

    struct DLL_EXPORT LogFilter{
        bool enabled;///< 标示是否启用
        
        inline LogFilter(){
            enabled = true;
        }

        virtual inline bool filter(
            LogMsg & msg
        ){
            return true;
        }

        virtual inline bool pre_filter(
            int level_id,
            std::string_view raw_message,
            LogMsgConfig & cfg
        ){
            return true;
        }
 
        virtual inline ~LogFilter(){}
    };

    struct DLL_EXPORT LoggerConfig{

        /// 0 means that the current thread is both producer & consumer
        unsigned int consumer_count;
        unsigned int fetch_message_count_max;

        static inline LoggerConfig default_cfg(){
            LoggerConfig cfg;

            // 默认就是单consumer的形式
            cfg.consumer_count = 1;
            cfg.fetch_message_count_max = consumer_message_default_count;

            return cfg;
        }
    };

    struct DLL_EXPORT Logger{
        using targets_t = std::vector<std::shared_ptr<LogTarget>>;
        using filters_t = std::vector<std::shared_ptr<LogFilter>>;
    private:
        friend class LogFactory;

        /// 输出对象缓存
        targets_t targets;
        /// 输出对象查找池
        std::unordered_map<std::string,RefWrapper<targets_t>> search_targets;
        /// 过滤器缓存
        filters_t filters;
        /// 过滤对象查找池
        std::unordered_map<std::string,RefWrapper<filters_t>> search_filters;

        /// 专门给消息池的内存池 
        /// @TODO 后面改成monotic试试
        std::pmr::polymorphic_allocator<LogMsg> msg_alloc;
        std::pmr::unsynchronized_pool_resource msg_buf;
        /// 消息池
        std::pmr::deque<LogMsg> messages;

        /// 计时器，用来显示从Logger创建以来运行的时间
        Clock clk;
        /// 锁，目前还是相信std::mutex的力量
        std::mutex msg_lock;
        /// 信号量，用来实现异步休眠
        std::counting_semaphore<semaphore_max_val> msg_semaphore;
        /// 线程池，用来管理consumer
        std::vector<std::thread> consumers;
        /// 线程能继续运行的flag
        bool logger_not_on_destroying;
        
        /// Logger配置
        LoggerConfig config;

        /// Header常量池，只增不减，鉴于LogFactory数目很少
        std::vector<std::string> header_pool;

        /// 初始化consumer线程
        void setup_consumer_threads();
        void consumer_func();
        size_t fetch_messages(std::vector<LogMsg> & target);

        std::string_view register_header(std::string_view val);

        template<CanAccessItem T> bool safe_remove_mod(
            std::string_view key,
            std::unordered_map<std::string,RefWrapper<T>> & st,
            T & container
        ){
            auto it = st.find(std::string(key));
            if(it == st.end()){
                return false;
            }
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

        // 内部处理，会直接调用std::move高效交换数据
        bool push_message_pmr(int level,std::pmr::string & body,LogMsgConfig & cfg);
    public:
        /// 字符串数据池
        std::pmr::polymorphic_allocator<char> msg_str_alloc;
        // 可能涉及多线程同时操作&分配，因此需要sync，无奈之举
        std::pmr::synchronized_pool_resource msg_str_buf;
        
        Logger(const LoggerConfig & cfg = LoggerConfig::default_cfg())
        :msg_buf()
        ,msg_alloc(&msg_buf)
        ,messages(msg_alloc)
        ,msg_str_buf()
        ,msg_str_alloc(&msg_str_buf)
        ,msg_semaphore(0){
            config = cfg;
            logger_not_on_destroying = true;
            setup_consumer_threads();
        }

        /// @note 注意，为了速度，msg可能会被写入！
        void write_messages(std::span<LogMsg> msg);

        /// @note 这里的话感觉不可避免地涉及一次数据copy
        bool push_message(int level,std::string_view body,LogMsgConfig & cfg){
            std::pmr::string str(msg_str_alloc);
            str.assign(body);
            return push_message_pmr(level,str,cfg);
        }

        /// 解决数据残留问题
        void flush();

        inline void clear_mod(){
            search_filters.clear();
            search_filters.clear();
            targets.clear();
            filters.clear();
        }

        template<class T = LogTarget> inline bool remove_mod(std::string_view name){
            static_assert(std::is_same_v<LogTarget,T> || std::is_same_v<LogFilter,T>,
                "T must be one of LogTarget or LogFilter!");
            if constexpr(std::is_same_v<LogTarget,T>){
                return safe_remove_mod(name,search_targets,targets);
            }else{
                return safe_remove_mod(name,search_filters,filters);
            }
        }

        template<class T,class... Args> inline std::shared_ptr<T> append_mod(std::string_view name,Args&&... args){
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

        ~Logger();
    };

    struct DLL_EXPORT LogFactory{
        Logger& logger;
        std::optional<std::pmr::string> cache_str;
        std::string_view header;
        LogMsgConfig cfg;
        int default_level;


        inline LogFactory(
            Logger & binded,
            std::string_view aheader = "",
            int def_level = 0, 
            const LogMsgConfig cfg = LogMsgConfig()
        ):logger(binded){
            if(aheader.compare("")){
                header = binded.register_header(aheader);
            }else header = "";
            this->cfg = cfg;
            default_level = def_level;
        }

        /// 信息转发
        inline bool log(int level,std::string_view message){
            return logger.push_message(level,message,cfg);
        }
        inline bool log(LogLevel level,std::string_view message){
            return log(static_cast<int>(level),message);
        }
        /// std::vformat_to转发
        template<class... Args> inline bool log(int level,std::string_view fmt,Args&&... args){
            std::pmr::string str (logger.msg_str_alloc);

            std::vformat_to(std::back_inserter(str),fmt,std::make_format_args(std::forward<Args>(args)...));
            return logger.push_message_pmr(level,str,cfg);
        }
        template<class... Args> inline bool log(LogLevel level,std::string_view fmt,Args&&... args){
            return log(static_cast<int>(level),fmt,std::forward<Args>(args)...);
        }

        inline bool log_pmr(int level,std::pmr::string & pmr_data){
            return logger.push_message_pmr(level,pmr_data,cfg);
        }

        inline StreamedContext<LogFactory> operator()(){
            return StreamedContext<LogFactory>(default_level,*this);
        }

        inline StreamedContext<LogFactory> operator()(int spec_level){
            return StreamedContext<LogFactory>(spec_level,*this);
        }
        
        inline StreamedContext<LogFactory> operator()(LogLevel spec_level){
            return StreamedContext<LogFactory>(static_cast<int>(spec_level),*this);
        }

        template<class T> inline StreamedContext<LogFactory> operator<<(T && t){
            return StreamedContext<LogFactory>(default_level,*this) << t;
        } 
    };
};

#endif