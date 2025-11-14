#ifndef ALOG_STREAMED_CONTEXT_INCLUDED
#define ALOG_STREAMED_CONTEXT_INCLUDED
#include <alib-g3/autil.h>
#include <source_location>

#ifndef ALIB_DISABLE_GLM_EXTENSIONS
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>
#endif

namespace alib::g3{
    /// @brief 仅用来识别日志终止
    struct DLL_EXPORT LogEnd{};
    typedef void (*EndLogFn)(LogEnd);
    /// @brief 流式输出日志终止表示
    inline void DLL_EXPORT endlog(LogEnd){}

    struct DLL_EXPORT log_source{
        /// @brief 缓存的路径
        std::source_location loc;
        /// @brief 是否保存完整路径，默认true
        bool keep_full;
        
        log_source(bool kf = true,std::source_location cloc = std::source_location::current()):loc(cloc),keep_full{kf}{}

        template<class T> void write_to_log(T & str){
            std::string_view p = loc.file_name();
            if(!keep_full){
                auto pos = p.find_last_of("/");
                if(pos != std::string_view::npos && pos+1 < p.size()){
                    p = p.substr(pos+1);
                }
            }
            std::format_to(std::back_inserter(str),"[{}:{}:{} {}]",p,loc.line(),loc.column(),loc.function_name());
        }
    };
    
    /// @brief 基础类型使用std::format，包含嵌套容器（如果你的编译器不支持需要自己实现下面的canforward）
    template<class T> concept GoUniversal = std::formattable<T,char>;
    /// @brief 对于能够forward的对象进行forward。forward优先级：外部 > 成员函数
    template<class T> concept CanForward = requires(T && t,std::pmr::string & target){
        t.write_to_log(target);
    } || requires(T && t,std::pmr::string & target){
        write_to_log(target,t);
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

        // 禁止拷贝，允许移动
        StreamedContext(const StreamedContext&) = delete;
        StreamedContext& operator=(const StreamedContext&) = delete;
        StreamedContext(StreamedContext&&) = default;
        StreamedContext& operator=(StreamedContext&&) = default;

        /// @brief 初始化字符串用的内存池以及level
        inline StreamedContext(int level,LogFactory & fac)
        :factory(fac)
        ,cache_str(factory.logger.msg_str_alloc){
            this->level = level;
        }

        /// @brief  上传日志
        /// @return 是否上传成功
        /// @note   StreamedContext设计出来就是用于局部构造的，因此upload后就失效了
        inline bool upload(){
            // 拒绝上传空的日志
            if(!cache_str.empty())return factory.log_pmr(level,cache_str);
            return false;
        }

        /// @brief 防止<<通用匹配过于通用而设计的
        template<class T>
        StreamedContext&& write(T && t){
            std::format_to(std::back_inserter(cache_str),"{}",t);
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

        #ifndef ALIB_DISABLE_GLM_EXTENSIONS
        /// @brief 格式化glm的向量
        template<int N,class T,enum glm::qualifier Q> 
            inline StreamedContext&& operator<<(const glm::vec<N,T,Q> & v){
            std::span<const T,N> value(glm::value_ptr(v),N);
            return (*this) << value;
        }
        /// @brief 格式化glm的矩阵
        template<int M,int N,class T,enum glm::qualifier Q> 
            inline StreamedContext&& operator<<(const glm::mat<M,N,T,Q> & v){
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
            std::span<const T,4> data (glm::value_ptr(v),4);
            return (*this) << data;
        }
        #endif
    };
}

#endif