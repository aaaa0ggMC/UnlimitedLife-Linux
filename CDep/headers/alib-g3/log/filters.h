#ifndef ALOG2_PREFAB_TARGETS
#define ALOG2_PREFAB_TARGETS
#include <alib-g3/log/kernel.h>
#include <alib-g3/adebug.h>

namespace alib::g3::lof{

    struct DLL_EXPORT CustomLevelBlocker : public LogFilter{
        /// @brief 用户自定义的判断函数
        using CustomFN = std::function<bool(int)>;

        /// @brief 用户定义的
        CustomFN should_keep;

        /// @brief 初始化，理论上要求传入的不能为NULL
        CustomLevelBlocker(CustomFN ishould_keep){
            panic_debug(should_keep != nullptr,"The function you passed shouldn't be null!");
            if(ishould_keep)should_keep = ishould_keep;
            else{
                // 搞一个默认函数，并且禁用当前Filter
                should_keep = [](int){return true;};
                toggle(false);
            }
        }

        inline bool pre_filter(
            int level,
            std::string_view raw_message,
            const LogMsgConfig & cfg) override
        {
            return should_keep(level);
        }
    };

}

#endif