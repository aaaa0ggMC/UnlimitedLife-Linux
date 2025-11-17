/**
 * @file base_mod.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 基础module接口
 * @version 0.1
 * @date 2025/11/17
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/11/17 
 */
#ifndef ALOG_MOD_INCLUDED
#define ALOG_MOD_INCLUDED
#include <alib-g3/log/base_msg.h>
#include <concepts>
#include <type_traits>

namespace alib::g3{
    /// @brief 输出对象
    struct DLL_EXPORT LogTarget{
        /// @brief 用于toggle输出，一般不用管
        bool enabled;
       
        /// @brief 切换状态
        LogTarget& toggle(bool val){
            enabled = val;
            return *this;
        }

        /// @brief 默认target就是enabled 
        inline LogTarget(){
            enabled = true;
        }
        
        /// @brief 往某个地方写入消息
        /// @note  如果节约内存修改logmsg也不是不行
        virtual inline void write(
            LogMsg & msg
        ){}

        /// @brief 刷新数据，比如缓冲区啥的
        virtual inline void flush(){}
        
        /// @brief 关闭IO
        /// @note  析构函数会自动close
        virtual inline void close(){}

        /// @brief 自动关闭资源
        virtual ~LogTarget(){
            flush();
            close();
        }
    };

    /// @brief 日志过滤器，支持日志拒绝，日志修改啥的
    struct DLL_EXPORT LogFilter{
        /// @brief 标示是否启用
        bool enabled;

        /// @brief 切换状态
        LogFilter& toggle(bool val){
            enabled = val;
            return *this;
        }
        
        /// @brief 默认启用日志
        inline LogFilter(){
            enabled = true;
        }

        /// @brief  尝试过滤日志，可以修改
        /// @return true:日志可以被处理，false:日志应该被抛弃
        virtual inline bool filter(LogMsg & msg){
            return true;
        }

        /// @brief  尝试在push message时提前处理日志
        /// @param level_id     当前的输出级别
        /// @param raw_message  日志原始body
        /// @param cfg          当前的配置
        /// @return true:日志可以被处理，false:日志应该被抛弃
        virtual inline bool pre_filter(
            int level_id,
            std::string_view raw_message,
            const LogMsgConfig & cfg
        ){
            return true;
        }

        /// @brief 关闭过滤器，也许你可以处理一些什么释放啥的，虽然析构也不是不行
        /// @note  析构函数会自动close
        virtual inline void close(){}
 
        /// @brief 自动close对象
        virtual inline ~LogFilter(){
            close();
        }
    };

    template<class T> concept IsLogTarget = std::is_base_of_v<LogTarget,T>;
    template<class T> concept IsLogFilter = std::is_base_of_v<LogFilter,T>;
    template<class T> concept IsLogMod = IsLogTarget<T> || IsLogFilter<T>;

    /// @brief LogTargetGroup
    template<IsLogTarget... Ts> struct DLL_EXPORT LogTargetGroup : public LogTarget{
        using targets_t = std::tuple<Ts...>;
        targets_t targets;

        LogTargetGroup(targets_t && t):targets(std::move(t)){}
        LogTargetGroup(Ts&&... args):targets(std::forward<Ts>(args)...){}

        template<size_t N> inline void toggleTarget(bool val = true){
            std::get<N>(targets).enabled = val;
        }

        inline void toggleAll(bool val){
            std::apply([&val](auto &... t){
                ((t.enabled = val),...);
            },targets);
        }

        inline void write(LogMsg & msg) override {
            std::apply([&msg](auto &... t){
                ((t.enabled?t.write(msg):void()),...);
            },targets);
        }

        inline void flush() override {
            std::apply([](auto &... t){
                ((t.enabled?t.flush():void()),...);
            },targets);
        }
    };


    /// @brief LogFilterGroup
    template<IsLogFilter... Ts> struct DLL_EXPORT LogFilterGroup : public LogFilter{
        using filters_t = std::tuple<Ts...>;
        filters_t filters;

        LogFilterGroup(filters_t && t):filters(std::move(t)){}
        LogFilterGroup(Ts&&... args):filters(std::forward<Ts>(args)...){}

        template<size_t N> inline void toggleFilter(bool val = true){
            std::get<N>(filters).enabled = val;
        }

        inline void toggleAll(bool val){
            std::apply([&val](auto &... t){
                ((t.enabled = val),...);
            },filters);
        }

        inline bool filter(LogMsg & msg) override{
            bool ret = true;

            std::apply([&ret,&msg](auto &... t){
                ((ret = ret && (t.enabled?t.filter(msg):true)),...);
            },filters);
            return ret;
        }

        inline bool pre_filter(
            int level_id,
            std::string_view raw_message,
            const LogMsgConfig & cfg
        ) override{
            bool ret = true;
            std::apply([&ret,&level_id,&raw_message,&cfg](auto &... t){
                ((ret = ret && (t.enabled?t.pre_filter(level_id,raw_message,cfg):true)),...);
            },filters);
            return ret;
        }
    };
}

#endif