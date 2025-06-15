/** @file Window.h
 * @brief 窗口
 * @author aaaa0ggmc
 * @date 2025-6-11
 * @version 3.1
 * @copyright copyright(C)2025
 ********************************
 @ ***par 修改日志:                      *
 <table>
 <tr><th>时间       <th>版本         <th>作者          <th>介绍
 <tr><td>2025-6-11 <td>3.1          <th>aaaa0ggmc    <td>v0.1
 </table>
 ********************************
 */
#ifndef AGE_WIN
#define AGE_WIN
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>
#include <optional>
#include <AGE/Base.h>
#include <alib-g3/aclock.h>

namespace age{
    /** @struct Window
     * @brief 窗口管理
     */
    class AGE_API Window{
    private:
        friend class Application;
        /// 内部的window
        GLFWwindow *window;
        /// 静态变量
        static Window * current;
        /// 窗口的SID
        std::string SID;
        /// 限制fps
        alib::g3::RateLimiter fpsLimiter {120};
        /// 切换current
        Window * s_current;

    public:
        /// 设置 interval
        inline static void setSwapInterval(int value){
            glfwSwapInterval(value);
        }

        inline static void pollEvents(){
            glfwPollEvents();
        }

        inline bool shouldClose(){
           return glfwWindowShouldClose(window)!=0;
        }

        inline void display(){
            glfwSwapBuffers(window);
            fpsLimiter.wait();
        }

        /// 激活窗口的GL上下文
        inline void makeCurrent(){
            current = this;
            glfwMakeContextCurrent(window);
        }

        inline void setFramerateLimit(float fps){
            fpsLimiter.reset(fps);
        }

        inline void clear(GLuint r = 0,GLuint g = 0,GLuint b = 0){
            swapCurrent();
            glClearBufferfi(GL_COLOR,r,g,b);
            restoreCurrent();
        }

        inline void swapCurrent(){
            if(current != this){
                s_current = current;
                makeCurrent();
            }
        }

        inline void restoreCurrent(){
            if(s_current){
                s_current->makeCurrent();
                s_current = NULL;
            }
        }

    };

    /** @struct CreateWindowInfo
     * @brief 创建窗口的结构
     */
    struct AGE_API CreateWindowInfo{
        std::string SID;///< 窗口的字符串id

        std::string windowTitle;///< 窗口标题
        unsigned int width;///< 窗口宽度
        unsigned int height;///< 窗口高度
        int x; ///< x坐标
        int y; ///< y坐标

        float fps; ///< fps限制，小于等于0等于没有限制

        /// 监视器
        std::optional<GLFWmonitor*> moniter;
        /// 共享
        std::optional<Window*> share;
    };
}
#endif
