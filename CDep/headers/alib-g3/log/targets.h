/**
 * @file targets.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 内置的输出对象，目前支持控制台输出，文件输出，以及多文件输出
 * @version 0.1
 * @date 2026/01/16
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/11/27 
 */
#ifndef ALOG_PREFAB_TARGETS
#define ALOG_PREFAB_TARGETS
#include <alib-g3/log/kernel.h>
#include <alib-g3/adebug.h>
#include <functional>
#include <stdio.h>

#ifdef __linux__
#define __internal_alib_fw fwrite_unlocked
#define __internal_alib_ff fflush_unlocked
#define __internal_alib_pc putchar_unlocked
#else

#define __internal_alib_fw _fwrite_nolock
#define __internal_alib_ff _fflush_nolock
#define __internal_alib_pc _putchar_nolock
#endif

#define LOG_COLOR3(fore,back,style) alib::g3::lot::color(alib::g3::lot::Color::fore,alib::g3::lot::Color::back,alib::g3::lot::Style::style)
#define LOG_COLOR2(fore,back) alib::g3::lot::color(alib::g3::lot::Color::fore,alib::g3::lot::Color::back)
#define LOG_COLOR1(fore) alib::g3::lot::color(alib::g3::lot::Color::fore)

namespace alib::g3{
    namespace lot{
        constexpr const char * rotate_file_def_fmt = "log{1}.txt";

        /// @brief 标准与亮色颜色枚举
        enum class Color : uint8_t {
            None = 0,
            Black = 30, Red = 31, Green = 32, Yellow = 33, Blue = 34, Magenta = 35, Cyan = 36, Gray = 36, White = 37,
            LRed = 91, LGreen = 92, LYellow = 93, LBlue = 94, LMagenta = 95, LCyan = 96, LGray = 96, LWhite = 97
        };

        /// @brief 样式位掩码
        enum class Style : uint16_t {
            None = 0,
            Bold      = 1 << 0, Dim       = 1 << 1, Italic    = 1 << 2, Underline = 1 << 3,
            Blink     = 1 << 4, BlinkFast = 1 << 5, Reverse   = 1 << 6, Hidden    = 1 << 7
        };

        /// @brief 构建颜色操纵器，ID 为 0 时表示 RESET
        inline constexpr log_tag color(Color fg, Color bg = Color::None, Style style = Style::None,uint16_t cate_id = 0){
            if(fg == Color::None && bg == Color::None && style == Style::None){
                return log_tag(0,0);
            }
            
            uint8_t fg_val = static_cast<uint8_t>(fg);
            // 背景色在 ANSI 中通常是前景色 + 10 (例如 Red 31 -> BG_Red 41)
            uint8_t bg_val = (bg == Color::None) ? 0 : (static_cast<uint8_t>(bg) + 10);
            
            int64_t id = (static_cast<int64_t>(style) << 16) | (static_cast<int64_t>(bg_val) << 8) | fg_val;
            return log_tag(cate_id,id);
        }

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

            /// @brief 类别id
            short category_id;

            inline Console(short category_id = 0,ConsoleConfig c = ConsoleConfig()):cfg(c){
                this->category_id = category_id;
            }
            
            void write(LogMsg & msg) override;

            inline void flush() override{
                {
                    std::lock_guard<std::mutex> lock(console_lock);
                    __internal_alib_ff(stdout);
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

    inline void Console::write(LogMsg & msg){
        static thread_local std::string s_buffer;
        s_buffer.clear();
        if(s_buffer.capacity() < 1024) s_buffer.reserve(1024);
        
        auto append_ansi_payload = [](std::string& buf, uint64_t payload){
            if(payload == 0){
                buf.append("\e[0m");
                return;
            }

            buf.append("\e[");
            bool first = true;

            // 处理 Style 位掩码 (Bits 16-31)
            uint16_t style = (payload >> 16) & 0xFFFF;
            for(int i = 0; i < 8; ++i){
                if(style & (1 << i)){
                    if(!first) buf.push_back(';');
                    buf.push_back((char)('1' + i));
                    first = false;
                }
            }

            // 处理背景色 (Bits 8-15)
            uint8_t bg = (payload >> 8) & 0xFF;
            if(bg){
                if(!first)buf.push_back(';');
                // 转换数字为字符串追加
                char bg_buf[4];
                int n = snprintf(bg_buf, sizeof(bg_buf), "%d", bg);
                buf.append(bg_buf, n);
                first = false;
            }

            // 处理前景色 (Bits 0-7)
            uint8_t fg = payload & 0xFF;
            if(fg){
                if(!first) buf.push_back(';');
                char fg_buf[4];
                int n = snprintf(fg_buf, sizeof(fg_buf), "%d", fg);
                buf.append(fg_buf, n);
            }
            buf.push_back('m');
        };

        if (!msg.cfg.disable_extra_information) {
            if (msg.cfg.gen_date) {
                s_buffer.append("[");
                if (cfg.date_color_schema) s_buffer.append(cfg.date_color_schema(msg));
                s_buffer.append(msg.sdate);
                if (cfg.date_color_schema) s_buffer.append("\e[0m");
                s_buffer.append("]");
            }

            if (msg.cfg.out_level && msg.cfg.level_cast) {
                s_buffer.append("[");
                if (cfg.level_color_schema) s_buffer.append(cfg.level_color_schema(msg));
                s_buffer.append(msg.cfg.level_cast(msg.level));
                if (cfg.level_color_schema) s_buffer.append("\e[0m");
                s_buffer.append("]");
            }

            if (msg.cfg.out_header && !msg.header.empty()) {
                s_buffer.append("[");
                if (cfg.head_color_schema) s_buffer.append(cfg.head_color_schema(msg));
                s_buffer.append(msg.header);
                if (cfg.head_color_schema) s_buffer.append("\e[0m");
                s_buffer.append("]");
            }

            if (msg.cfg.gen_time) {
                s_buffer.append("[");
                if (cfg.time_color_schema) s_buffer.append(cfg.time_color_schema(msg));
                char t_buf[32];
                int n = snprintf(t_buf, sizeof(t_buf), "%.2lfms", msg.timestamp);
                s_buffer.append(t_buf, n);
                if (cfg.time_color_schema) s_buffer.append("\e[0m");
                s_buffer.append("]");
            }

            if (msg.cfg.gen_thread_id) {
                s_buffer.append("[");
                if (cfg.thread_id_color_schema) s_buffer.append(cfg.thread_id_color_schema(msg));
                char tid_buf[32];
                int n = snprintf(tid_buf, sizeof(tid_buf), "TID%lu", msg.thread_id);
                s_buffer.append(tid_buf, n);
                if (cfg.thread_id_color_schema) s_buffer.append("\e[0m");
                s_buffer.append("]");
            }
            s_buffer.append(":");
        }
        size_t last_pos = 0;
        std::string_view body_view = msg.body;

        if (cfg.body_color_schema) s_buffer.append(cfg.body_color_schema(msg));
        for(uint32_t i = 0; i < msg.tags.size(); ++i){
            auto& tag = msg.tags[i];
            if (tag.category == this->category_id) {
                uint64_t current_pos = tag.get();
                if(current_pos > last_pos && current_pos <= body_view.size()) {
                    s_buffer.append(body_view.substr(last_pos, current_pos - last_pos));
                }

                append_ansi_payload(s_buffer, tag.payload);
                last_pos = current_pos;
            }
        }

        if(last_pos < body_view.size()){
            s_buffer.append(body_view.substr(last_pos));
        }
        s_buffer.append("\e[0m\n");
        {
            std::lock_guard<std::mutex> lock(console_lock);
            __internal_alib_fw(s_buffer.data(), 1, s_buffer.size(), stdout);
        }
    }
}
#endif