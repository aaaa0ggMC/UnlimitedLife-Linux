module;
#include <AVE/config.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <alib6/debug.h>
export module ave.window:window;

import ave.context;
import alib6;
import alib6;
import :glfw;

export namespace ave {

    struct AVE_API CreateWindowInfo{
        Context & ctx;

        /// 一般都很小，而且只搞一次
        std::string title = "Hello from AVE";
        alib6::u32 width = 1920;
        alib6::u32 height = 1080;

        mutable alib6::ErrorWrapper ew = {};
    };

    struct AVE_API Window {
    private:
        GLFWwindow * window { nullptr };
    public:
        Window() = default;
        /// 支持懒人构建
        inline Window(const CreateWindowInfo & ci){
            create(ci);
        }
        /// 如果忘了，也给你擦屁股
        inline ~Window(){ destroy(); }

        /// 提供移动
        Window(Window && win):window(win.window){
            win.window = nullptr;
        }
        void operator=(Window && win){
            if(&win == this)return;
            panic_debug(window != nullptr, "Cannot move window to an object that has a window already.");
            if(window != nullptr) [[unlikely]] {
                destroy();
            }
            
            window = win.window;
            win.window = nullptr;
        }
        Window& operator=(const Window &) = delete;
        Window(const Window &) = delete;

    
        bool create(const CreateWindowInfo & ci){
            GLFWManager::init();

            if(window){
                ci.ew.report(
                    ave_already_created,
                    "Error: window has been created already."
                );
                return false;
            }

            if(!detail::is_main_thread()){
                ci.ew.report(
                    ave_not_in_main_thread,
                    "Fatal Error: cannot create window in threads other than the main thread."
                );
                return false;
            }

            /// TODO: Styles
            // 一些hint设置
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

            /// TODO: support parent & monitor 
            // 这里需要copy出null terminated
            window = glfwCreateWindow(
                ci.width, 
                ci.height, 
                ci.title.c_str() , 
                nullptr, nullptr
            );

            return true;
        }

        inline void destroy() noexcept {
            if(window){
                glfwDestroyWindow(window);
                window = nullptr;
            }
        }

        /// 这里是一些简单的glfw封装
        inline bool should_close() noexcept { return glfwWindowShouldClose(window); }
        inline GLFWwindow * get_system_handle() noexcept { return window; }
        inline static void poll_events() noexcept { glfwPollEvents(); }
    };

}