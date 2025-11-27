/**
 * @file manipulator.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 一些基本的操作符
 * @version 0.1
 * @date 2025/11/27
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/11/27 
 */
#ifndef ALOG2_MANIP_INCLUDED
#define ALOG2_MANIP_INCLUDED
#include <alib-g3/autil.h>
#include <source_location>

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

    /// @brief format标识，nullptr清空fmt，长期保持
    /// @note  清空fmt会带来更快的格式化
    struct log_fmt{
        /// @brief 格式化字符串
        std::string_view fmt_str;

        log_fmt(std::string_view s = ""):fmt_str(s){}
    };

    /// @brief 临时的format标志，最好只用来输出基础数据类型
    /// @note  清空fmt会带来更快的格式化
    struct log_tfmt{
        /// @brief 格式化字符串
        std::string_view fmt_str;

        log_tfmt(std::string_view s = ""):fmt_str(s){}
    };

    /// @brief 单条日志设置是否展示header
    struct log_header{
        /// @brief 将在manipulate中设置
        const bool val;

        inline log_header(bool ival = true):val(ival){}

        inline void manipulate(LogMsgConfig & cfg) const{
            cfg.out_header = val;
        }
    };

    /// @brief 单条日志设置是否展示level
    struct log_level{
        /// @brief 将在manipulate中设置
        const bool val;

        inline log_level(bool ival = true):val(ival){}

        inline void manipulate(LogMsgConfig & cfg) const{
            cfg.out_level = val;
        }
    };

    /// @brief 单条日志设置是否展示date
    struct log_date{
        /// @brief 将在manipulate中设置
        const bool val;

        inline log_date(bool ival = true):val(ival){}

        inline void manipulate(LogMsgConfig & cfg) const{
            cfg.gen_date = val;
        }
    };

    /// @brief 单条日志设置是否展示level
    struct log_time{
        /// @brief 将在manipulate中设置
        const bool val;

        inline log_time(bool ival = true):val(ival){}

        inline void manipulate(LogMsgConfig & cfg) const{
            cfg.gen_time = val;
        }
    };

    /// @brief 单条日志设置是否展示thread_id
    struct log_tid{
        /// @brief 将在manipulate中设置
        const bool val;

        inline log_tid(bool ival = true):val(ival){}

        inline void manipulate(LogMsgConfig & cfg) const{
            cfg.gen_thread_id = val;
        }
    };
}

#endif