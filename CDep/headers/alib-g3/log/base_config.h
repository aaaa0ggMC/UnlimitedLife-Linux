#ifndef ALOG_BASECONFIG_INCLUDED
#define ALOG_BASECONFIG_INCLUDED
#include <alib-g3/autil.h>

namespace alib::g3{
    /// @brief 日志消息细节控制，用在LogFactory中，控制额外信息的输出
    struct DLL_EXPORT LogMsgConfig{
        /// @brief 可以通过这个形式的函数自定义某个数字对应的level显示
        using LevelCastFn = std::string_view (*)(int);

        /// @brief 是否禁用额外信息生成，一旦为true，下面的额外信息也都没有意义了，默认为false
        bool disable_extra_information;

        /// @brief 是否生成线程id,eg [TID424325]，默认为false
        bool gen_thread_id; 
        /// @brief 是否生成运行时间,eg [999.12ms]，默认为true
        bool gen_time;
        /// @brief 是否生成当前日期,eg [25-10-1 09:02:02]，默认为true
        bool gen_date;

        /// @brief 是否输出LogFactory的"head",eg "Test"，默认为 true
        bool out_header;
        /// @brief 是否输出当前日志级别，默认为 true
        bool out_level;
        /// @brief 用来转换level的函数，默认为 default_level_cast
        LevelCastFn level_cast;

        /// @brief 默认的level cast,适配LogLevel这个enum
        static std::string_view default_level_cast(int level_in);
        
        /// @brief 这个初始化了默认值
        LogMsgConfig();
    };

    /// @brief Logger的配置文件
    struct DLL_EXPORT LoggerConfig{
    public:
        /// @brief Consumer线程数量，0表示启动sync模式，否则为async，默认为1
        unsigned int consumer_count;
        /// @brief fetch_message一次性能取出信息的最大值，默认为 consumer_message_default_count
        unsigned int fetch_message_count_max;
        /// @brief 是否开启背压模式，默认为false（背压会出现消息不同步），如果为false，下面的内容失效
        bool enable_back_pressure;
        /// @brief 背压模式的阈值，阈值 = 下面的倍率 * 一次性能取出最大值 * 消费者数量，默认为 4
        /// @note  其实背压下相当于主线程也变成了一个消费者，每次push_message执行一次fetch
        unsigned int back_pressure_multiply;

        /// 构造默认配置
        LoggerConfig();
    };

    /// @brief LogFactory配置
    struct DLL_EXPORT LogFactoryConfig{
        /// @brief 这个用于在LogFactory阶段就直接pass掉调用，从而进一步剪枝叶
        /// @param[in] level 当前日志的级别
        /// @return true表示保留日志，false表示丢弃日志
        using LevelKeepFn = bool(*)(int);

        /// @brief 默认选择的level数值
        int def_level;
        /// @brief 用来提前剪枝的函数，默认为空
        LevelKeepFn level_should_keep;
        /// @brief 缓存的header，LogFactory回对这个再处理，使用Logger::register_header来防止悬垂
        std::string_view header;
        /// @brief 默认的消息配置信息
        LogMsgConfig msg;

        /// @brief 构造默认数值
        LogFactoryConfig(
            std::string_view iheader = "",
            int idef_level = 0,
            LevelKeepFn ilevel_should_keep = nullptr,
            const LogMsgConfig & msg_cfg = LogMsgConfig()
        );
    };
}

#endif