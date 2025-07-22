/** @file Window.h
 * @brief 窗口
 * @author aaaa0ggmc
 * @start-date 2025-6-11
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
#ifndef AGE_H_WIN
#define AGE_H_WIN
#include <AGE/Utils.h>
#include <AGE/VAO.h>
#include <AGE/VBO.h>
#include <AGE/Input.h>

#include <GL/gl.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <optional>
#include <alib-g3/aclock.h>
#include <glm/glm.hpp>

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


    class Window;
    template<class T> concept Drawable = requires(T t,Window & w, GLuint instanceCount, PrimitiveType type){
        {t.draw(w,instanceCount,type)}; // || 
    };

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
        /// 用于处理绑定 Actually,It can be replaced by glfwSetWindowUserPointer & glfwGetWindowUserPointer,however,my impl isnt that bad
        static BinderArray binderArray;
        /// 窗口的SID
        std::string_view sid;
        /// 限制fps
        alib::g3::RateLimiter fpsLimiter;
        /// 绑定便于访问
        Binder binder { binderArray };
        /// OnResize
        std::function<void(Window&,int nw,int nh)> m_onResize;
        std::function<void(Window&,age::KeyWrapper wrapper)> m_onKey;


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

        inline GLFWwindow* getSystemHandle(){
            return window;
        }

        /// 激活窗口的GL上下文
        inline void makeCurrent(){
            current = this;
            glfwMakeContextCurrent(window);
        }

        inline void setFramerateLimit(float fps){
            fpsLimiter.reset(fps);
        }

        inline glm::vec2 getSize(){
            int width, height;
            glfwGetWindowSize(window, &width, &height);
            return {width, height};
        }

        inline void setKeyCallback(std::function<void(Window&,age::KeyWrapper wrapper)> func){
            m_onKey = func;
        }

        inline void removeKeyCallback(){
            m_onKey = nullptr;
        }

        inline void setWindowSizeCallback(std::function<void(Window&,int nw,int nh)> func){
            m_onResize = func;
        }

        inline void removeWindowSizeCallback(){
            m_onResize = nullptr;
        }

        inline glm::vec2 getFrameBufferSize(){
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            return {width, height};
        }

        inline void clear(GLuint r = 0,GLuint g = 0,GLuint b = 0,GLuint a = 255,GLuint target = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT){
            ScopedWindow scp(this);
            glClearColor(r,g,b,a);
            glClear(target);
        }

        [[deprecated("use drawArray instead...")]]
        inline void draw(PrimitiveType type,GLuint startIndex,GLint count,GLuint instanceCount = 1,VAO vao = VAO::null()){
            drawArray(type,startIndex,count,instanceCount,vao);
        }

        /// this function uses glDrawArrays when instanceCount == 1
        inline void drawArray(PrimitiveType type,GLuint startIndex,GLint count,GLuint instanceCount = 1,VAO vao = VAO::null()){
            if(!instanceCount)return;
            std::optional<VAO::ScopedVAO> scp = std::nullopt;
            if(vao.getId() != 0){
                scp.emplace(vao);
            }
            if(instanceCount == 1)glDrawArrays(static_cast<GLenum>(type),startIndex,count);
            else glDrawArraysInstanced(static_cast<GLenum>(type),startIndex,count,instanceCount);
        }

        inline void setTitle(std::string_view title){
            glfwSetWindowTitle(window,title.data());
        }

        template<Drawable T> inline void draw(const T & data,GLuint instanceCount = 1,PrimitiveType type = PrimitiveType::Triangles){
            data.template draw<Window>(*this,instanceCount,type);
        }

        /**
         * @brief 绘制带索引的元素
         * @param type 图元类型
         * @param indice_count 索引数量，输入0并且namedIndices不为空可以让函数自行推导
         * @param namedIndices 指定indice是什么而非使用 GL_ELEMENT_ARRAY_BUFFER，为空使用GL_ELEMENT_ARRAY_BUFFER
         */
        inline void drawElements(PrimitiveType type,size_t indice_count,GLuint instanceCount = 1,const std::vector<int>& namedIndices = {},VAO vao = VAO::null()){
            if(!instanceCount)return;
            const int * ptr;
            size_t count = indice_count;
            if(namedIndices.size() == 0){
                ptr = nullptr;
            }else {
                ptr = namedIndices.data();
                if(count)count = namedIndices.size();
            }
            std::optional<VAO::ScopedVAO> scp = std::nullopt;
            if(vao.getId() != 0){
                scp.emplace(vao);
            }
            if(instanceCount == 1)glDrawElements(static_cast<GLenum>(type),count,GL_UNSIGNED_INT,ptr);
            else glDrawElementsInstanced(static_cast<GLenum>(type),count,GL_UNSIGNED_INT,ptr,instanceCount);
        }

        /// true: enable ; false: diable
        void setStyle(WinStyle styles,bool operation = AGE_Enable);

    };

    /** @struct CreateWindowInfo
     * @brief 创建窗口的结构
     */
    struct AGE_API CreateWindowInfo{
        std::string sid;///< 窗口的字符串id

        std::string windowTitle;///< 窗口标题
        int width;///< 窗口宽度
        int height;///< 窗口高度
        int x; ///< x坐标
        int y; ///< y坐标

        float fps; ///< fps限制，小于等于0等于没有限制

        WinStyle style; ///< 窗口的样式

        /// 监视器
        std::optional<GLFWmonitor*> moniter;
        /// 共享
        std::optional<Window*> share;

        static std::pair<int,int> ScreenPercent(float px,float py,int *x,int * y);
        /// @brief 保持比例
        /// @param tokeep 
        /// @param tochange 
        /// @param a 分子
        /// @param b 分母
        static inline void KeepRatio(int & tokeep,int & tochange,float a,float b){
            tochange = tokeep * b / a;
        }

        CreateWindowInfo();
    };
}
#endif
