#ifndef ALOG_STREAMED_CONTEXT_INCLUDED
#define ALOG_STREAMED_CONTEXT_INCLUDED
#include <alib-g3/autil.h>

#ifndef ALIB_DISABLE_GLM_EXTENSIONS
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>
#endif

namespace alib::g3{
    struct DLL_EXPORT LogEnd{};
    typedef void (*EndLogFn)(LogEnd);
    void DLL_EXPORT endlog(LogEnd);
    
    template<class T> concept GoUniversal = std::formattable<T,char>;
    template<class T> concept CanForward = requires(T && t,std::pmr::string & target){
        t.write_to_log(target);
    } || requires(T && t,std::pmr::string & target){
        write_to_log(target,t);
    };

    template<class LogFactory> struct StreamedContext{
        LogFactory & factory;
        std::pmr::string cache_str;
        int level;

        inline StreamedContext(int level,LogFactory & fac)
        :factory(fac)
        ,cache_str(factory.logger.msg_str_alloc){
            this->level = level;
        }

        inline bool upload(){
            // 拒绝上传空的日志
            if(!cache_str.empty())return factory.log_pmr(level,cache_str);
            return false;
        }

        template<class T>
        StreamedContext&& write(T && t){
            std::format_to(std::back_inserter(cache_str),"{}",t);
            return std::move(*this);
        }

        template<GoUniversal T>
        StreamedContext&& operator<<(T && t){
            return write<T>(std::forward<T>(t));
        }

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

        // 支持endlog终止日志
        inline bool operator<<(EndLogFn fn){
            return upload();
        }

        // 支持std::endl终止日志
        inline bool operator<<(std::ostream& (*manip)(std::ostream&)){
            return upload();
        }

        #ifndef ALIB_DISABLE_GLM_EXTENSIONS
        template<int N,class T,enum glm::qualifier Q> 
            inline StreamedContext&& operator<<(const glm::vec<N,T,Q> & v){
            std::span<const T,N> value(glm::value_ptr(v),N);
            return (*this) << value;
        }
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
        template<class T,enum glm::qualifier Q> 
            inline StreamedContext&& operator<<(const glm::qua<T,Q> & v){
            std::span<const T,4> data (glm::value_ptr(v),4);
            return (*this) << data;
        }
        #endif

    };
}

#endif