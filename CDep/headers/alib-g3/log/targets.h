#ifndef ALOG_PREFAB_TARGETS
#define ALOG_PREFAB_TARGETS
#include <alib-g3/alogger_base.h>
#include <stdio.h>

namespace alib::g3{
    namespace lot{
        struct DLL_EXPORT Console : public LogTarget{
            static std::mutex console_lock;

            /// @note 如果节约内存修改logmsg也不是不行
            inline void write(
                LogMsg & msg
            ) override {
                // 在flush中刷新
                auto p = msg.gen_composed();
                {
                    std::lock_guard<std::mutex> lock(console_lock);
                    fwrite_unlocked(p.data(),sizeof(char),p.size(),stdout);
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
    };

};
#endif