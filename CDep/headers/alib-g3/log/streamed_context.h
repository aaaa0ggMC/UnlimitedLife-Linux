/**
 * @file streamed_context.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 流式输出处控制，主持write_to_log(类函数&全局函数)编译期注入
 * @version 0.1
 * @date 2026/01/15
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/11/27 
 */
#ifndef ALOG_STREAMED_CONTEXT_INCLUDED
#define ALOG_STREAMED_CONTEXT_INCLUDED
#include <alib-g3/autil.h>
#include <alib-g3/log/base_config.h>
#include <alib-g3/log/manipulator.h>
#include <alib-g3/adebug.h>
#include <format>

#ifndef ALIB_DISABLE_GLM_EXTENSIONS
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>
#endif

namespace alib::g3{
    /// @brief 基础类型使用std::format，包含嵌套容器（如果你的编译器不支持需要自己实现下面的canforward）
    template<class T> concept GoUniversal = std::formattable<T,char>;
    /// @brief 对于能够forward的对象进行forward。forward优先级：外部 > 成员函数
    template<class T> concept CanForward = requires(T && t,std::pmr::string & target){
        t.write_to_log(target);
    } || requires(T && t,std::pmr::string & target){
        write_to_log(target,t);
    };
    /// @brief LogMsgConfig的操作器，满足这个接口就可以直接处理信息了
    template<class T> concept CanManipulate = requires(T && t,LogMsgConfig & c){
        t.manipulate(c);
    };

    /// @brief 流式输出核心
    /// @tparam LogFactory 类CRTP写法与LogFactory解耦，理论上只能接上LogFactory
    template<class LogFactory> struct StreamedContext{
        /// @brief 持有的LogFactory对象
        LogFactory & factory;
        /// @brief 流式输出内部缓存的字符串，考虑到刚好可以直接转发给Logger，因此每个context一份
        std::pmr::string cache_str;
        /// @brief 日志级别
        int level;
        /// @brief 当前Context是否是启用的，如果Level不满足LogFactory需求其会被标记为invalid，进而不组合&上传日志
        bool context_valid;
        /// @brief 格式化字符串，用后就清除
        std::string_view fmt_str;
        /// @brief 格式化是否是临时的
        bool fmt_tmp;
        /// @brief 配置信息
        LogMsgConfig msg_cfg;
        /// @brief TAG信息
        LogCustomTag tags[log_custom_tag_count];
        uint32_t current_index = 0;
        

        // 禁止拷贝，允许移动
        StreamedContext(const StreamedContext&) = delete;
        StreamedContext& operator=(const StreamedContext&) = delete;
        StreamedContext(StreamedContext&&) = default;
        StreamedContext& operator=(StreamedContext&&) = default;

        /// @brief 初始化字符串用的内存池以及level
        inline StreamedContext(int level,LogFactory & fac,bool valid = true)
        :factory(fac)
        ,cache_str(factory.logger.msg_str_alloc)
        ,msg_cfg(fac.cfg.msg){
            this->level = level;
            fmt_str = "";
            fmt_tmp = false;
            context_valid = valid;
        }

        /// @brief  上传日志
        /// @return 是否上传成功
        /// @note   StreamedContext设计出来就是用于局部构造的，因此upload后就失效了
        inline bool upload(){
            // 拒绝上传空的日志
            if(context_valid && !cache_str.empty())return factory.log_pmr(level,cache_str,msg_cfg,tags,current_index);
            return false;
        }

        /// @brief 防止<<通用匹配过于通用而设计的
        template<class T>
        StreamedContext&& write(T && t){
            if(!context_valid)return std::move(*this);
            if(fmt_str.empty())std::format_to(std::back_inserter(cache_str),"{}",t);
            else{
                std::vformat_to(std::back_inserter(cache_str),fmt_str.data(),
                    std::make_format_args(t)
                );
                if(fmt_tmp){
                    fmt_str = "";
                }
            }
            return std::move(*this);
        }

        /// @brief 注入式的manipulator
        template<CanManipulate T>
        StreamedContext&& operator<<(T && t){
            if(!context_valid)return std::move(*this);
            t.manipulate(msg_cfg);
            return std::move(*this);
        }

        /// @brief 格式化基础类型
        template<GoUniversal T>
        StreamedContext&& operator<<(T && t){
            return write<T>(std::forward<T>(t));
        }

        /// @brief 格式化可转发类型
        template<CanForward T>
        StreamedContext&& operator<<(T && t){
            if(!context_valid)return std::move(*this);
            // 是的，孩子们，你可以通过这个实现非侵入式的覆写了
            if constexpr(requires(T&&t,std::pmr::string&val){write_to_log(val,t);}){
                write_to_log(cache_str,t);
            }else{
                t.write_to_log(cache_str);
            }
            return std::move(*this);
        }

        /// @brief 支持endlog终止日志
        inline bool operator<<(EndLogFn fn){
            return upload();
        }

        /// @brief 支持std::endl终止日志
        inline bool operator<<(std::ostream& (*manip)(std::ostream&)){
            return upload();
        }

        // 孩子们，你觉得下面的terminate method还是人类吗？

        /// @brief YetAnotherTerminateMethod
        inline bool operator<<(LogEnd){
            return upload();
        }

        /// @brief YetAnotherAnotherTerminateMethod
        inline bool operator|(LogEnd){
            return upload();
        }

        /// @brief 支持插入自定义的信息
        inline StreamedContext&& operator<<(const log_tag & t){
            panic_debug(current_index >= log_custom_tag_count,"There are too many custom tags!");
            if(!context_valid || current_index >= log_custom_tag_count)return std::move(*this);
            LogCustomTag & tag = tags[current_index++];
            tag.id  = t.id;
            tag.set(cache_str.size());
            return std::move(*this);
        }

        /// @brief 支持设置格式化，需要保证局部不悬垂（应该没人会在一句话就把数据删除了吧）
        inline StreamedContext&& operator<<(const log_fmt & fmt){
            if(!context_valid)return std::move(*this);
            fmt_str = fmt.fmt_str.data();
            fmt_tmp = false;
            return std::move(*this);
        }

        /// @brief 支持设置格式化，需要保证局部不悬垂（应该没人会在一句话就把数据删除了吧）
        inline StreamedContext&& operator<<(const log_tfmt & fmt){
            if(!context_valid)return std::move(*this);
            fmt_str = fmt.fmt_str.data();
            fmt_tmp = true;
            return std::move(*this);
        }

        #ifndef ALIB_DISABLE_GLM_EXTENSIONS
        /// @brief 格式化glm的向量
        template<int N,class T,enum glm::qualifier Q> 
            inline StreamedContext&& operator<<(const glm::vec<N,T,Q> & v){
            if(!context_valid)return std::move(*this);
            std::span<const T,N> value(glm::value_ptr(v),N);
            return (*this) << value;
        }
        /// @brief 格式化glm的矩阵
        template<int M,int N,class T,enum glm::qualifier Q> 
            inline StreamedContext&& operator<<(const glm::mat<M,N,T,Q> & v){
            if(!context_valid)return std::move(*this);
            std::span<const T,N> data (glm::value_ptr(v),M*N);
            cache_str.append("{");
            for(int m = 0;m < M;++m){
                (*this) << data.subspan(m*N,N);
                if(m+1 != M)cache_str.append(" , ");
            }
            cache_str.append("}");
            return std::move(*this);
        }
        /// @brief 格式化glm的四元数
        template<class T,enum glm::qualifier Q> 
            inline StreamedContext&& operator<<(const glm::qua<T,Q> & v){
            if(!context_valid)return std::move(*this);
            std::span<const T,4> data (glm::value_ptr(v),4);
            return (*this) << data;
        }
        #endif
    };
}

#endif