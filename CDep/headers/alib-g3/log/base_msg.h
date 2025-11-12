#ifndef ALOG_LOGMSG_INCLUDED
#define ALOG_LOGMSG_INCLUDED
#include <alib-g3/log/base_config.h>

namespace alib::g3{
    /// @brief 日志的每一条消息
    struct DLL_EXPORT LogMsg{
    private:
        friend class Logger;
        /// @brief 标识日志是否可用，会在filter阶段标记
        bool m_nice_one;
        /// @brief 标识当前日志是否已经生成了composed str从而减少构造
        bool generated;
    public:
        /// @brief 指向Logger中的header pool,保证不悬垂
        std::string_view header;
        /// @brief 最终合成的日志主要部分
        std::pmr::string body;
        /// @brief 日志指向的配置，需要保证指向始终可用
        LogMsgConfig * cfg;
        /// @brief 这条日志的级别
        int level;
        
        //// 附加生成信息 ////
        /// @brief 线程id
        uint64_t thread_id;
        /// @brief producer生成的时间戳
        double timestamp;

        //// 缓冲 ////
        /// @brief 用于缓冲日期数据
        static thread_local std::string sdate;
        /// @brief 用于缓冲启动时长数据
        static thread_local std::string stime;
        /// @brief 用户缓冲最终生成的数据
        static thread_local std::string scomposed;

        /// @brief 构造核心内容
        LogMsg(const std::pmr::polymorphic_allocator<char> &__a,LogMsgConfig & c);
        
        /// @brief Producer构造基础信息，如timestamp和tid
        void build_on_producer(Clock & clk);
        /// @brief Consumer合成字符串存储到当前结构
        /// @note  由于这个函数使用的是static threadlocal的变量 
        ///        因此必须保证对日志信息主遍历而不是对targets
        void build_on_consumer();
        
        /// @brief 生成组合数据，懒加载，在无其他日志插入的context中，第一次调用会构造，之后之前返回构造了的 
        std::string_view gen_composed();

        /// @brief 移动构造日志，主要处理为移动构造pmr对象，pmr对象最好保持allocator一致
        void move_msg(LogMsg&& msg);
        /// @brief 移动构造
        inline LogMsg& operator=(LogMsg&& msg){
            move_msg(std::forward<LogMsg>(msg));
            return *this;
        }
        /// @brief  移动构造
        inline LogMsg(LogMsg&& msg){
            move_msg(std::forward<LogMsg>(msg));
        }
    };
}

#endif