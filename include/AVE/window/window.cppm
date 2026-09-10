module;
#include <AVE/config.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
export module ave.window:window;

import ave.context;
import alib6;
import :glfw;

export namespace ave {

    struct AVE_API CreateWindowInfo{
        Context & ctx;

        std::string_view title = "Hello from AVE";
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
        Window(const CreateWindowInfo & ci){
            create(ci);
        }
        /// 如果忘了，也给你擦屁股
        ~Window(){ destroy(); }
    
        bool create(const CreateWindowInfo & ci){
            GLFWManager::init();

            if(window){
                ci.ew.report(
                    ave_window_already_created,
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

            /// TODO: support parent & monitor 
            // 这里需要copy出null terminated
            window = glfwCreateWindow(
                ci.width, 
                ci.height, 
                std::string(ci.title).c_str() , 
                nullptr, nullptr
            );

            return true;
        }

        void destroy() noexcept {
            if(window){
                glfwDestroyWindow(window);
                window = nullptr;
            }
        }

        /// 这里是一些简单的glfw封装
        bool should_close() noexcept { return glfwWindowShouldClose(window); }
        GLFWwindow * get_system_handle() noexcept { return window; }
        static void poll_events() noexcept { glfwPollEvents(); }
    };

}