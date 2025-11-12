#ifndef ALOG_PREFAB_TARGETS
#define ALOG_PREFAB_TARGETS
#include <alib-g3/alogger_base.h>
#include <alib-g3/adebug.h>
#include <stdio.h>

namespace alib::g3{
    namespace lot{
        /// @brief 控制台的配置文件
        struct DLL_EXPORT ConsoleConfig{
            using ColorSchemaFn = std::string_view(*)(const LogMsg & msg); 
            /// @brief 默认的级别函数
            static std::string_view default_level_color_schema(const LogMsg & msg);
            
            /// @brief 控制台输出头颜色方案
            ColorSchemaFn head_color_schema {nullptr};
            /// @brief 控制台主体颜色方案
            ColorSchemaFn body_color_schema {nullptr};
            /// @brief 控制台时间颜色方案
            ColorSchemaFn time_color_schema {nullptr};
            /// @brief 控制台日期颜色方案
            ColorSchemaFn date_color_schema {nullptr};
            /// @brief 控制台线程id颜色方案
            ColorSchemaFn thread_id_color_schema {nullptr};
            /// @brief 控制台级别颜色方案，存在默认函数
            ColorSchemaFn level_color_schema {default_level_color_schema};
        };

        /// @brief 预制菜里的控制台输出
        struct DLL_EXPORT Console : public LogTarget{
            /// @brief 全局锁，防止输出错乱
            static std::mutex console_lock;

            /// @brief 控制台配置
            ConsoleConfig cfg;

            inline Console(ConsoleConfig c = ConsoleConfig()):cfg(c){}
            
            inline void write(
                LogMsg & msg
            ) override {
                static std::string_view reset_color = "\e[0m"; // 复原颜色
                // 在flush中刷新
                static auto fast_print = [](ConsoleConfig::ColorSchemaFn fn,LogMsg & msg){
                    if(fn){
                        auto ptr = fn(msg);
                        if(!ptr.empty()){
                            fwrite_unlocked(ptr.data(),sizeof(decltype(ptr)::value_type),ptr.size(),stdout);
                            return true;
                        }
                    }
                    return false;
                };
                static auto reset_color = [](){
                    fwrite_unlocked(reset_color.data(),sizeof(decltype(reset_color)::value_type),reset_color.size(),stdout);
                };
                {
                    std::lock_guard<std::mutex> lock(console_lock);
                    if(msg.cfg->gen_date){
                        putchar_unlocked('[');
                        bool val = fast_print(cfg.date_color_schema,msg);
                        fwrite_unlocked(msg.sdate.data(),sizeof(decltype(msg.sdate)::value_type),msg.sdate.size(),stdout);
                        if(val)reset_color();
                        putchar_unlocked(']');
                    }
                    if(msg.cfg->out_level){
                        putchar_unlocked('[');
                        bool val = fast_print(cfg.level_color_schema,msg);
                        auto ls = msg.cfg->level_cast(msg.level);
                        fwrite_unlocked(ls.data(),sizeof(decltype(ls)::value_type),ls.size(),stdout);
                        if(val)reset_color();
                        putchar_unlocked(']');
                    }
                    if(msg.cfg->out_header && header.data() != nullptr && header.size()){
                        putchar_unlocked('[');
                        bool val = fast_print(cfg.head_color_schema,msg);
                        fwrite_unlocked(msg.header.data(),sizeof(decltype(msg.header)::value_type),msg.header.size(),stdout);
                        if(val)reset_color();
                        putchar_unlocked(']');
                    }
                    if(msg.cfg->gen_time){
                        std::string_view use_color;
                        if(cfg.time_color_schema && !(use_color = cfg.time_color_schema(msg)).empty()){
                            printf("[%s%.2lfms%s]",use_color.data(),msg.timestamp,reset_color.data());
                        }else{
                            printf("[%.2lfms]",msg.timestamp);
                        }
                    }
                    if(msg.cfg->gen_thread_id){
                        std::string_view use_color;
                        if(cfg.thread_id_color_schema && !(use_color = cfg.thread_id_color_schema(msg)).empty()){
                            printf("[%sTID%lu%s]",use_color.data(),msg.thread_id,reset_color.data());
                        }else{
                            printf("[TID%lu]",msg.thread_id);
                        }
                    }
                    putchar_unlocked(":");
                    fwrite_unlocked(msg.body.data(),sizeof(decltype(msg.body)::value_type),msg.body.size(),stdout);
                    putchar_unlocked('\n');
                }
            }

            inline void flush() override{
                {
                    std::lock_guard<std::mutex> lock(console_lock);
                    fflush_unlocked(stdout);
                }
            }
        };
        /// @brief 单文件固定输出
        struct DLL_EXPORT File : public LogTarget{
            FILE* file {nullptr};
            std::string currently_open;

            inline void open_file(std::string_view fp){
                if(file){
                    fclose(file);
                    file = nullptr;
                }
                currently_open = fp; 
                file = fopen(currently_open.c_str(),"w");
                panicf_debug(!file,"Cannot open file {}!",fpath);
            }

            inline File(std::string_view fpath){
                open_file(fpath);
            }

            inline void write(LogMsg & msg) override{
                auto p = msg.gen_composed();
                fwrite(p.data(),sizeof(decltype(p)::value_type),p.size(),file);
            }

            inline void flush() override{
                fflush(file);
            }

            inline void close() override{
                if(file){
                    fclose(file);
                }
            }
        };
    };
};

//// inline实现 ////
namespace alib::g3{
    inline std::string_view ConsoleConfig::default_level_color_schema(const LogMsg & msg){
        switch(msg.level){
        case 0: // Trace采用灰色
            return "\e[36m";
        case 1: // Debug采用白色
            return "\e[37m";
        case 2: // Info采用黄色
            return "\e[33m";
        case 3: // Warn采用蓝色
            return "\e[34m";
        case 4: // Error采用红色
            return "\e[31m";
        case 5: // Fatal白底红字
            return "\e[31;47m";
        default:
            return "";
        }
    }
}
#endif