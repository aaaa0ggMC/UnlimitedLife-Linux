module;
#include <AVE/config.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <alib6/debug.h>
export module ave.window:window;

import ave.context;
import alib6;
import std;
import :glfw;

export namespace ave {

    enum class WindowStyle : alib6::u32 {
        FollowGLFW             = 1u << 31,
        Resizable              = 1u << 0,
        Visible                = 1u << 1,
        Decorated              = 1u << 2,
        Focused                = 1u << 3,
        AutoIconify            = 1u << 4,
        Floating               = 1u << 5,
        Maximized              = 1u << 6,
        CenterCursor           = 1u << 7,
        TransparentFramebuffer = 1u << 8,
        FocusOnShow            = 1u << 9,
        ScaleToMonitor         = 1u << 10,
        ScaleFramebuffer       = 1u << 11
    };

    inline constexpr WindowStyle operator|(WindowStyle lhs, WindowStyle rhs) {
        return static_cast<WindowStyle>(
            static_cast<alib6::u32>(lhs) | static_cast<alib6::u32>(rhs)
        );
    }

    inline constexpr WindowStyle operator&(WindowStyle lhs, WindowStyle rhs) {
        return static_cast<WindowStyle>(
            static_cast<alib6::u32>(lhs) & static_cast<alib6::u32>(rhs)
        );
    }

    inline constexpr bool has_style(WindowStyle styles, WindowStyle style) {
        return static_cast<alib6::u32>(styles & style) != 0;
    }

    inline constexpr WindowStyle window_style_normal =
        WindowStyle::Resizable |
        WindowStyle::Visible |
        WindowStyle::Decorated |
        WindowStyle::Focused |
        WindowStyle::AutoIconify |
        WindowStyle::CenterCursor;

    struct AVE_API CreateWindowInfo{
        Context & ctx;

        /// 一般都很小，而且只搞一次
        std::string title = "Hello from AVE";
        alib6::u32 width = 1920;
        alib6::u32 height = 1080;
        WindowStyle style = WindowStyle::FollowGLFW;
        GLFWmonitor* monitor = nullptr;
        std::optional<std::pair<int, int>> position;

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
        Window(Window && win) noexcept :window(win.window){
            win.window = nullptr;
        }
        Window& operator=(Window && win) noexcept {
            if(&win == this) return *this;
            panic_debug(window != nullptr, "Cannot move window to an object that has a window already.");
            if(window != nullptr) [[unlikely]] {
                destroy();
            }
            
            window = win.window;
            win.window = nullptr;
            return *this;
        }
        Window& operator=(const Window &) = delete;
        Window(const Window &) = delete;

    
        bool create(const CreateWindowInfo & ci){
            if(window){
                ci.ew.report(
                    ave_already_created,
                    "Error: window has been created already."
                );
                return false;
            }

            if(!GLFWManager::init(ci.ew)) return false;

            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

            if(!has_style(ci.style, WindowStyle::FollowGLFW)){
                auto set_hint = [&](int hint, WindowStyle style){
                    glfwWindowHint(
                        hint,
                        has_style(ci.style, style) ? GLFW_TRUE : GLFW_FALSE
                    );
                };

                set_hint(GLFW_RESIZABLE, WindowStyle::Resizable);
                set_hint(GLFW_VISIBLE, WindowStyle::Visible);
                set_hint(GLFW_DECORATED, WindowStyle::Decorated);
                set_hint(GLFW_FOCUSED, WindowStyle::Focused);
                set_hint(GLFW_AUTO_ICONIFY, WindowStyle::AutoIconify);
                set_hint(GLFW_FLOATING, WindowStyle::Floating);
                set_hint(GLFW_MAXIMIZED, WindowStyle::Maximized);
                set_hint(GLFW_CENTER_CURSOR, WindowStyle::CenterCursor);
                set_hint(GLFW_TRANSPARENT_FRAMEBUFFER, WindowStyle::TransparentFramebuffer);
                set_hint(GLFW_FOCUS_ON_SHOW, WindowStyle::FocusOnShow);
                set_hint(GLFW_SCALE_TO_MONITOR, WindowStyle::ScaleToMonitor);
                set_hint(GLFW_SCALE_FRAMEBUFFER, WindowStyle::ScaleFramebuffer);
            }

            window = glfwCreateWindow(
                static_cast<int>(ci.width),
                static_cast<int>(ci.height),
                ci.title.c_str(),
                ci.monitor,
                nullptr // GLFW_NO_API 不支持共享 OpenGL context
            );

            if(!window){
                const char* description = nullptr;
                glfwGetError(&description);
                ci.ew.report(
                    ave_bad_glfw,
                    "Fatal Error: cannot create GLFW window: {}.",
                    description ? description : "unknown GLFW error"
                );
                return false;
            }

            if(ci.position && !ci.monitor){
                glfwSetWindowPos(window, ci.position->first, ci.position->second);
            }

            return true;
        }

        inline void destroy() noexcept {
            if(window){
                glfwDestroyWindow(window);
                window = nullptr;
            }
        }

        /// 这里是一些简单的glfw封装
        inline explicit operator bool() const noexcept { return window != nullptr; }
        inline bool should_close() noexcept { return !window || glfwWindowShouldClose(window); }
        inline GLFWwindow * get_system_handle() noexcept { return window; }
        inline static void poll_events() noexcept { glfwPollEvents(); }

        inline void set_size(int width, int height) {
            glfwSetWindowSize(window, width, height);
        }

        inline std::pair<int, int> get_size() const {
            int width = 0;
            int height = 0;
            glfwGetWindowSize(window, &width, &height);
            return { width, height };
        }

        inline std::pair<int, int> get_framebuffer_size() const {
            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            return { width, height };
        }

        inline std::pair<float, float> get_content_scale() const {
            float xscale = 1.0f;
            float yscale = 1.0f;
            glfwGetWindowContentScale(window, &xscale, &yscale);
            return { xscale, yscale };
        }

        inline void set_position(int x, int y) {
            glfwSetWindowPos(window, x, y);
        }

        inline void set_title(std::string_view title) {
            const std::string null_terminated(title);
            glfwSetWindowTitle(window, null_terminated.c_str());
        }
    };

}
