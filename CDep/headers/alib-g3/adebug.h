/**
 * @file adebug.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 目标是提供简单可用的错误判断，适合我这种一般不调试的人
 * @version 0.1
 * @date 2025/12/01
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/11/09 
 */
#ifndef ADEBUG_H_INCLUDED
#define ADEBUG_H_INCLUDED
#include <stacktrace>
#include <source_location>
#include <assert.h>
#include <format>

#ifndef ADEBUG_INTERNAL_PANIC_FMT
#define ADEBUG_INTERNAL_PANIC_FMT
// 关于索引
// {0}  最终合成的信息
// {1}  panic的文件
// {2}  panic的函数名字
// {3}  panic的行号
// {4}  panic的列号
// {5}  一坨stacktrace
constexpr const char * alib_g3_internal_panic_fmt = 
R"(Message  : {0}
Source   : {1}:{3}:{4}
Function : {2}
Stack    : 
{5}
)";
constexpr const char * alib_g3_internal_panic_if_fmt = "{}\nCondition: {}";
#endif

#ifndef ADEBUG_INTERNAL_PANIC_STACKTRACE_FN
#define ADEBUG_INTERNAL_PANIC_STACKTRACE_FN simplify_stacktrace
#endif

#ifndef ADEBUG_INTERNAL_PANIC_FORWARD
#define ADEBUG_INTERNAL_PANIC_FORWARD(X) 
#endif

#ifndef ADEBUG_INTERNAL_PANIC_ABORT
#define ADEBUG_INTERNAL_PANIC_ABORT std::abort() 
#endif

#ifndef ADEBUG_NO_OUTPUT
#include <iostream>
#endif

namespace alib::g3{
    namespace detail{
        inline std::string simplify_stacktrace(const decltype(std::stacktrace::current()) & st) {
            constexpr int skipc = 2;
            int index = -1;
            std::string ret = "";
            for(auto & entry : st){
                index++;
                if(index < skipc)continue;
                if(entry.source_file().empty() && entry.source_line() == 0)continue;
        #ifdef ALIB_TRACE_SKIP_CPP_NATIVE
        #ifndef ALIB_TRACE_SKIP_PROMPT
            #define ALIB_TRACE_SKIP_PROMPT "c++"
        #endif
                if(entry.source_file().find(ALIB_TRACE_SKIP_PROMPT) != std::string::npos)continue; // skip c++ stacktrace
        #endif
                ret +=  "  ";
                ret += std::to_string(index - skipc) + "# ";
                ret += entry.source_file();
                ret += ":";
                ret += std::to_string(entry.source_line());
                ret += " at ";
                ret += entry.description();
                ret += "\n";
        #ifdef AGE_TRACE_LOOSE
                ret += "\n";
        #endif
            }
            return ret;
        }

        inline void internal_panic(std::string_view msg,std::source_location sl){
            std::string final_str = std::format(alib_g3_internal_panic_fmt,msg,sl.file_name(),sl.function_name(),
                sl.line(),sl.column(),ADEBUG_INTERNAL_PANIC_STACKTRACE_FN(std::stacktrace::current()));
        #ifndef ADEBUG_NO_OUTPUT
            std::cerr << final_str << std::endl;
        #endif
            ADEBUG_INTERNAL_PANIC_FORWARD(final_str);
            ADEBUG_INTERNAL_PANIC_ABORT;
        }

    };


    template<class... Args> inline void VPanicf(std::string_view fmt,
        std::source_location source,Args&&...args){
        std::string message = std::vformat(fmt,std::make_format_args(args...));
        detail::internal_panic(message,source);
    }

    template<class... Args> inline void Panicf(const std::format_string<Args...> & fmt,
        std::source_location source,Args&&...args){
        std::string message = std::format(fmt,std::forward<Args>(args)...);
        detail::internal_panic(message,source);
    }


    template<class Arg> inline void Panic(Arg && arg,
        // 为了保证顶层只有两个栈空间从而能简化顶层stack
        std::source_location source = std::source_location::current()){
        std::string message = std::format("{}",std::forward<Arg>(arg));
        detail::internal_panic(message,source);
    }
};

#ifndef ADEBUG_DISABLE_TOOLKIT_MACROS
#define panic(ARG) alib::g3::Panic(ARG,std::source_location::current())
#define panicf(STR,...) alib::g3::Panicf(STR,std::source_location::current(),##__VA_ARGS__)
#define vpaincf(STR,...) alib::g3::VPanicf(STR,std::source_location::current(),##__VA_ARGS__)

#ifndef ADEBUG_DISABLE_PASS_CONDITION_STRING
#define vpanicf_if(COND,STR,...) do {if(COND){vpanicf(STR,__VA_ARGS__,#COND);}} while(0)
#define panic_if(COND,ARG) do {if(COND){panicf(alib_g3_internal_panic_if_fmt,ARG,#COND);}} while(0)
#define panicf_if(COND,STR,...) do {if(COND){panicf(STR,__VA_ARGS__,#COND);}} while(0)
#else
#define vpanicf_if(COND,STR,...) do {if(COND){vpanicf(STR,__VA_ARGS__);}} while(0)
#define panic_if(COND,ARG) do {if(COND){panic(ARG);}} while(0)
#define panicf_if(COND,STR,...) do {if(COND){panicf(STR,__VA_ARGS__);}} while(0);
#endif

#ifdef NDEBUG
/// Note that the expression you input must have no side-effects!
#define panic_debug(will_be_removed_in_release_mode_cond,ARG)
#define panicf_debug(will_be_removed_in_release_mode_cond,ARG,...)
#define vpanicf_debug(will_be_removed_in_release_mode_cond,ARG,...)
#else
/// Note that the expression you input must have no side-effects!
#define panic_debug(will_be_removed_in_release_mode_cond,ARG) panic_if(will_be_removed_in_release_mode_cond,ARG)
#define panicf_debug(will_be_removed_in_release_mode_cond,STR,...) panicf_if(will_be_removed_in_release_mode_cond,STR,__VA_ARGS__)
#define vpanicf_debug(will_be_removed_in_release_mode_cond,STR,...) vpanicf_if(will_be_removed_in_release_mode_cond,STR,__VA_ARGS__)
#endif
#endif

#endif