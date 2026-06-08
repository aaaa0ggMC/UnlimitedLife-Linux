#ifndef AGE_MWINDOW_H_INCLUDED
#define AGE_MWINDOW_H_INCLUDED
#include <AGE/Window.h>
#include <AGE/Details/CSBuffer.h>

namespace age::manager{
    using namespace age::detail;

    struct AGE_API WindowManager{
    private:
        friend class Application;
        ConstantStringBuffer& csbuffer;
        /// @brief 窗口列表
        std::unordered_map<std::string_view,Window*> windows;
    public:
        WindowManager(ConstantStringBuffer & cb):csbuffer{cb}{}
        ~WindowManager();

        std::optional<Window*> create(const CreateWindowInfo & ci);
        std::optional<Window*> get(std::string_view sid);
        bool destroy(std::string_view sid);
        
        inline bool has(std::string_view sid){
            return windows.find(sid) != windows.end();
        }

        inline bool destroy(Window & win){
            return destroy(win.sid);
        }
    };

}

#endif