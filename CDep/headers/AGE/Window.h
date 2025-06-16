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
#include <algorithm>
#include <cstdlib>
#include <string>
#include <optional>
#include <AGE/Base.h>
#include <alib-g3/aclock.h>

namespace age{
    enum class WinStyle: uint32_t {
        FollowGLFW =            0b10000000000000000000000000000000,///< 使用当前GLFW默认设置（位31
        Resizable  =            0b00000000000000000000000000000001,///< 可调整大小（位0）,
        Visible    =            0b00000000000000000000000000000010, ///< 窗口可见（位1）
        Decorated  =            0b00000000000000000000000000000100,///< 有标题栏/边框（位2）
        Focused    =            0b00000000000000000000000000001000,///< 初始获得焦点（位3）
        AutoIconify=            0b00000000000000000000000000010000,///< 全屏时失去焦点自动最小化（位4）
        Floating   =            0b00000000000000000000000000100000,///< 置顶窗口（位5）
        Maximized  =            0b00000000000000000000000001000000,///< 初始最大化（位6）
        CenterCursor =          0b00000000000000000000000010000000,///< 光标初始居中（位7）
        TransparentFrameBuffer =0b00000000000000000000000100000000,///< 透明帧缓冲（位8）
        FocusOnShow  =          0b00000000000000000000001000000000,///< 显示时获取焦点（位9）
        ScaleToMonitor =        0b00000000000000000000010000000000 ///< 根据显示器缩放内容（位10）
    };

    inline constexpr WinStyle operator |(WinStyle a,WinStyle b){
        return (WinStyle)((uint32_t)(a) | (uint32_t)(b));
    }

    inline constexpr WinStyle operator ~(WinStyle a){
        return (WinStyle)(~(uint32_t)(a));
    }

    inline constexpr WinStyle operator &(WinStyle a,WinStyle b){
        return (WinStyle)((uint32_t)(a) & (uint32_t)(b));
    }

    inline constexpr WinStyle operator ^(WinStyle a,WinStyle b){
        return (WinStyle)((uint32_t)(a) ^ (uint32_t)(b));
    }

    inline constexpr WinStyle operator +(WinStyle a,WinStyle b){
        return (WinStyle)((uint32_t)(a) | (uint32_t)(b));
    }

    inline constexpr WinStyle operator -(WinStyle a,WinStyle b){
        return (WinStyle)((uint32_t)(a) & ~(uint32_t)(b));
    }

    inline constexpr WinStyle operator +=(WinStyle & a,WinStyle b){
        a = (WinStyle)((uint32_t)(a) | (uint32_t)(b));
        return a;
    }

    inline constexpr WinStyle operator -=(WinStyle & a,WinStyle b){
        a = (WinStyle)((uint32_t)(a) & ~(uint32_t)(b));
        return a;
    }

    inline constexpr bool ws_hasFlag(WinStyle styles,WinStyle base){
        return ((uint32_t)(styles) & (uint32_t)(base));
    }

    inline constexpr WinStyle WinStylePresetNormal = WinStyle::Resizable | WinStyle::Visible | WinStyle::Decorated | WinStyle::Focused | WinStyle::Floating | WinStyle::AutoIconify;
    inline constexpr WinStyle WinStylePresetBorderless = WinStyle::Visible | WinStyle::Focused | WinStyle::Floating;
    inline constexpr WinStyle WinStylePresetUltility = WinStyle::Visible | WinStyle::Decorated | WinStyle::Floating;
    inline constexpr WinStyle WinStylePresetFullScreen = WinStyle::Visible | WinStyle::Maximized | WinStyle::AutoIconify;

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
        std::string sid;
        /// 限制fps
        alib::g3::RateLimiter fpsLimiter;
        /// 切换current
        Window * s_current;

        Window();

        struct ScopedWindow{
        public:
            Window * old;
            inline ScopedWindow(Window * c){
                old = (c==Window::current)?NULL:Window::current;
                if(old)c->makeCurrent();
            }

            inline ~ScopedWindow(){
                if(old)old->makeCurrent();
            }

            ScopedWindow(const ScopedWindow&) = delete;
            ScopedWindow& operator=(const ScopedWindow&) = delete;
        };

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

        inline void clear(GLuint r = 0,GLuint g = 0,GLuint b = 0,GLuint a = 255,GLuint target = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT){
            ScopedWindow scp(this);
            glClearColor(r,g,b,a);
            glClear(target);
        }

        /// true means negate false means apply
        void setStyle(WinStyle styles,bool operation = AGE_Apply);

    };

    /** @struct CreateWindowInfo
     * @brief 创建窗口的结构
     */
    struct AGE_API CreateWindowInfo{
        std::string sid;///< 窗口的字符串id

        std::string windowTitle;///< 窗口标题
        unsigned int width;///< 窗口宽度
        unsigned int height;///< 窗口高度
        int x; ///< x坐标
        int y; ///< y坐标

        float fps; ///< fps限制，小于等于0等于没有限制

        WinStyle style; ///< 窗口的样式

        /// 监视器
        std::optional<GLFWmonitor*> moniter;
        /// 共享
        std::optional<Window*> share;

        CreateWindowInfo();
    };
}
#endif
