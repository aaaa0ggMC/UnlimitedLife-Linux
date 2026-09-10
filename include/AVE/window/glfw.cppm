module;
#include <AVE/config.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
export module ave.window:glfw;

import alib6;
import std;
import ave.ecode;

/// 一些细节实现
namespace ave::detail {
    // 获取主线程id
    inline const std::thread::id main_thread_id = std::this_thread::get_id();
    // 判断是否处于主线程，不处于无法进行构造
    inline bool is_main_thread(){
        return main_thread_id == std::this_thread::get_id();
    };
}

export namespace ave{
    struct AVE_API GLFWManager{
    private:
        inline static bool glfw_inited = false;
    public:
        /// 初始化后调用将不会受到县城拘束
        static bool init(
            alib6::ErrorWrapper ew = alib6::ErrorWrapper()
        ){
            if(glfw_inited) return true;
            if(!detail::is_main_thread()){
                ew.report(
                    ave_not_in_main_thread,
                    "Fatal Error: cannot initialize GLFW context in threads other than the main thread."
                );
                return false;
            }

            // 一些hint设置
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

            if(glfwInit() == GL_FALSE){
                // 初始化失败，获取错误
                if(ew){
                    const char * description;
                    int code = glfwGetError(&description);

                    ew.report(
                        ave_bad_glfw,
                        "Fatal Error: cannot initialize GLFW for {}.",
                        description
                    );
                }
                return false;
            }

            glfw_inited = true;
            return true;
        }

        static bool inited(){
            return glfw_inited;
        }

        static void terminate(){
            if(!glfw_inited) return;
            glfwTerminate();
            glfw_inited = false;
        }
    
    };

}