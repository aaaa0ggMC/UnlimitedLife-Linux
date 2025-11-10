#include <alib-g3/alogger.h>
#include <iostream>

namespace alib::g3{
    namespace lot{
        struct DLL_EXPORT Console : public LogTarget{
            /// @note 如果节约内存修改logmsg也不是不行
            inline void write(
                LogMsg & msg
            ) override {
                // 在flush中刷新
                std::cout << msg.gen_composed() << "\n";
            }
        };
    };

};