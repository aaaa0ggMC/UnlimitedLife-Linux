#ifndef ALOG_PREFAB_TARGETS
#define ALOG_PREFAB_TARGETS
#include <alib-g3/log/kernel.h>
#include <alib-g3/adebug.h>
#include <stdio.h>

namespace alib::g3{
    namespace lot{
        constexpr const char * rotate_file_def_fmt = "log{1}.txt";

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
            
            void write(LogMsg & msg) override;

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
            bool need_close {true};

            inline void open_file(std::string_view fp){
                if(file){
                    if(need_close)fclose(file);
                    file = nullptr;
                }
                currently_open = fp; 
                file = fopen(currently_open.c_str(),"w");
                panicf_debug(!file,"Cannot open file {}!",fp);
            }

            inline File(std::string_view fpath){
                open_file(fpath);
            }

            inline File(FILE * f){
                file = f;
                need_close = false;
            }

            inline void write(LogMsg & msg) override{
                auto p = msg.gen_composed();
                fwrite(p.data(),sizeof(decltype(p)::value_type),p.size(),file);
            }

            inline void flush() override{
                fflush(file);
            }

            inline void close() override{
                if(need_close && file){
                    fclose(file);
                }
            }
        };

        struct DLL_EXPORT RotateFile;  
        /// @brief 轮转文件的配置
        struct DLL_EXPORT RotateFileConfig{
            using IOFailedCallbackFN = std::function<void(std::string_view,RotateFile &)>;
            /// @brief 文件的格式，一些参数：{0}:当前时间戳 {1}:rotate index，默认"log{1}.txt"
            std::string filepath_fmt;
            /// @brief 轮换时的大小(Bytes)，默认4MB，0表示不检测
            unsigned long int rotate_size;
            /// @brief 轮换时经过的时间（ms），小于等于0表示不检测，默认不检测(-1)
            long int rotate_time;
            /// @brief 当打开文件失败后的通知，默认为空
            IOFailedCallbackFN failed_open_fn;

            /// @brief 初始化输出对象
            inline RotateFileConfig(
                std::string_view fmt = rotate_file_def_fmt,
                unsigned int ro_size = 4 * 1024 * 1024,
                long int ro_time = -1,
                IOFailedCallbackFN fn = nullptr
            ){
                filepath_fmt = fmt;
                rotate_size = ro_size;
                rotate_time = ro_time;
                failed_open_fn = fn;
            }
        };

        /// @brief 轮转文件
        struct DLL_EXPORT RotateFile : public LogTarget{
        private:
            FILE * f {nullptr};
            int rotate_index { 0 };
            bool noneed_update {false};
            uint64_t bytes_written {0};
            std::string current_fp_delayed;
            RotateFileConfig config;
            timespec last_expire {-1,-1};

            inline bool expires(){
                timespec tm;
                clock_gettime(time_clock_source,&tm);
                // 大小超了
                bool opt = config.rotate_size && (bytes_written >= config.rotate_size);
                defer{
                    if(opt){
                        last_expire = tm;
                    }
                };
                if(opt)return true;
                // 时间超了
                if(last_expire.tv_nsec < 0){
                    // 尚未初始化
                    last_expire = tm;
                    opt = false;
                }else opt = config.rotate_time > 0 && (((tm.tv_sec - last_expire.tv_sec) * 1000 + (tm.tv_nsec - last_expire.tv_nsec) / 1'000'000)
                    >= config.rotate_time);
                if(opt)return true;
                return false;
            }

        public:
            inline std::string_view get_current_filepath(){
                if(noneed_update)return current_fp_delayed;
                noneed_update = true;
                current_fp_delayed.clear();

                time_t curtime = time(0);

                std::vformat_to(std::back_inserter(current_fp_delayed),
                config.filepath_fmt,std::make_format_args(curtime,rotate_index));
                return current_fp_delayed;
            }

            inline void flush() override{
                if(f)fflush(f);
            }

            inline void try_open(){
                if(f && !expires())return;
                close();
                noneed_update = false;
                f = fopen(get_current_filepath().data(),"w");
                // 重置写入量
                panicf_debug(!f,"Cannot open file {}!",get_current_filepath());
                if(!f && config.failed_open_fn){
                    config.failed_open_fn(get_current_filepath(),*this);
                }else{
                    ++rotate_index;
                }
            }

            inline void close() override{
                if(f){
                    fclose(f);
                    bytes_written = 0;
                    f = nullptr;
                }
            }

            inline void write(LogMsg & msg) override{
                try_open();
                if(f){
                    auto p = msg.gen_composed();
                    bytes_written += fwrite(p.data(),sizeof(decltype(p)::value_type),p.size(),f);
                }
            }

            inline RotateFile(RotateFileConfig cfg = RotateFileConfig()){
                config = cfg;
                try_open();
            }

        };
    };
};

//// inline实现 ////
namespace alib::g3::lot{
    inline std::string_view ConsoleConfig::default_level_color_schema(const LogMsg & msg){
        switch(msg.level){
        case 0: // Trace采用白色
            return "\e[37m";
        case 1: // Debug采用紫色
            return "\e[35m";
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

    inline void Console::write(
        LogMsg & msg
    ){
        static std::string_view c_reset_color = "\e[0m"; // 复原颜色
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
            fwrite_unlocked(c_reset_color.data(),sizeof(decltype(c_reset_color)::value_type),c_reset_color.size(),stdout);
        };
        {
            std::lock_guard<std::mutex> lock(console_lock);
            if(!msg.cfg->disable_extra_information){
                if(msg.cfg->gen_date){
                    fwrite_unlocked("[",1,1,stdout);
                    bool val = fast_print(cfg.date_color_schema,msg);
                    fwrite_unlocked(msg.sdate.c_str(),sizeof(decltype(msg.sdate)::value_type),msg.sdate.size(),stdout);
                    if(val)reset_color();
                    fwrite_unlocked("]",1,1,stdout);
                }
                if(msg.cfg->out_level){
                    fwrite_unlocked("[",1,1,stdout);
                    bool val = fast_print(cfg.level_color_schema,msg);
                    auto ls = msg.cfg->level_cast(msg.level);
                    fwrite_unlocked(ls.data(),sizeof(decltype(ls)::value_type),ls.size(),stdout);
                    if(val)reset_color();
                    fwrite_unlocked("]",1,1,stdout);
                }
                if(msg.cfg->out_header && msg.header.data() != nullptr && msg.header.size()){
                    fwrite_unlocked("[",1,1,stdout);
                    bool val = fast_print(cfg.head_color_schema,msg);
                    fwrite_unlocked(msg.header.data(),sizeof(decltype(msg.header)::value_type),msg.header.size(),stdout);
                    if(val)reset_color();
                    fwrite_unlocked("]",1,1,stdout);
                }
                if(msg.cfg->gen_time){
                    std::string_view use_color;
                    if(cfg.time_color_schema && !(use_color = cfg.time_color_schema(msg)).empty()){
                        printf("[%s%.2lfms%s]",use_color.data(),msg.timestamp,c_reset_color.data());
                    }else{
                        printf("[%.2lfms]",msg.timestamp);
                    }
                }
                if(msg.cfg->gen_thread_id){
                    std::string_view use_color;
                    if(cfg.thread_id_color_schema && !(use_color = cfg.thread_id_color_schema(msg)).empty()){
                        printf("[%sTID%lu%s]",use_color.data(),msg.thread_id,c_reset_color.data());
                    }else{
                        printf("[TID%lu]",msg.thread_id);
                    }
                }
                putchar_unlocked(':');
            }
            fwrite_unlocked(msg.body.data(),sizeof(decltype(msg.body)::value_type),msg.body.size(),stdout);
            putchar_unlocked('\n');
        }
    }
}
#endif