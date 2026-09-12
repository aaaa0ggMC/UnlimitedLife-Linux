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
import :keycode;
import :event;
import :input;

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

        std::function<void(Event&)> m_event_callback;

        struct ListenerEntry {
            alib6::u64 id;
            std::function<void(Event&)> callback;
        };
        std::vector<ListenerEntry> m_listeners;
        alib6::u64 m_next_listener_id { 1 };

        Input* m_bound_input { nullptr };

        void setup_callbacks() {
            if (!window) return;

            glfwSetWindowCloseCallback(window, [](GLFWwindow* w) {
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                if (!self) return;
                WindowCloseEvent ev;
                self->dispatch_event(ev);
            });

            glfwSetWindowSizeCallback(window, [](GLFWwindow* w, int width, int height) {
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                if (!self) return;
                WindowResizeEvent ev(width, height);
                self->dispatch_event(ev);
            });

            glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int width, int height) {
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                if (!self) return;
                WindowFramebufferResizeEvent ev(width, height);
                self->dispatch_event(ev);
            });

            glfwSetWindowPosCallback(window, [](GLFWwindow* w, int xpos, int ypos) {
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                if (!self) return;
                WindowMoveEvent ev(xpos, ypos);
                self->dispatch_event(ev);
            });

            glfwSetWindowFocusCallback(window, [](GLFWwindow* w, int focused) {
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                if (!self) return;
                WindowFocusEvent ev(focused == GLFW_TRUE);
                self->dispatch_event(ev);
            });

            glfwSetWindowIconifyCallback(window, [](GLFWwindow* w, int iconified) {
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                if (!self) return;
                WindowIconifyEvent ev(iconified == GLFW_TRUE);
                self->dispatch_event(ev);
            });

            glfwSetWindowMaximizeCallback(window, [](GLFWwindow* w, int maximized) {
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                if (!self) return;
                WindowMaximizeEvent ev(maximized == GLFW_TRUE);
                self->dispatch_event(ev);
            });

            glfwSetWindowContentScaleCallback(window, [](GLFWwindow* w, float xscale, float yscale) {
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                if (!self) return;
                WindowContentScaleEvent ev(xscale, yscale);
                self->dispatch_event(ev);
            });

            glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                if (!self) return;
                KeyCode keycode = static_cast<KeyCode>(key);
                KeyModifier modifiers = static_cast<KeyModifier>(mods);
                switch (action) {
                    case GLFW_PRESS: {
                        KeyPressedEvent ev(keycode, scancode, modifiers, 0);
                        self->dispatch_event(ev);
                        break;
                    }
                    case GLFW_RELEASE: {
                        KeyReleasedEvent ev(keycode, scancode, modifiers);
                        self->dispatch_event(ev);
                        break;
                    }
                    case GLFW_REPEAT: {
                        KeyRepeatEvent ev(keycode, scancode, modifiers, 1);
                        self->dispatch_event(ev);
                        break;
                    }
                    default:
                        break;
                }
            });

            glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int mods) {
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                if (!self) return;
                MouseButton btn = static_cast<MouseButton>(button);
                KeyModifier modifiers = static_cast<KeyModifier>(mods);
                if (action == GLFW_PRESS) {
                    MouseButtonPressedEvent ev(btn, modifiers);
                    self->dispatch_event(ev);
                } else if (action == GLFW_RELEASE) {
                    MouseButtonReleasedEvent ev(btn, modifiers);
                    self->dispatch_event(ev);
                }
            });

            glfwSetCursorPosCallback(window, [](GLFWwindow* w, double xpos, double ypos) {
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                if (!self) return;
                MouseMoveEvent ev(xpos, ypos);
                self->dispatch_event(ev);
            });

            glfwSetScrollCallback(window, [](GLFWwindow* w, double xoffset, double yoffset) {
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                if (!self) return;
                MouseScrollEvent ev(xoffset, yoffset);
                self->dispatch_event(ev);
            });

            glfwSetCursorEnterCallback(window, [](GLFWwindow* w, int entered) {
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                if (!self) return;
                MouseEnterEvent ev(entered == GLFW_TRUE);
                self->dispatch_event(ev);
            });

            glfwSetCharCallback(window, [](GLFWwindow* w, unsigned int codepoint) {
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                if (!self) return;
                CharEvent ev(static_cast<char32_t>(codepoint));
                self->dispatch_event(ev);
            });

            glfwSetDropCallback(window, [](GLFWwindow* w, int count, const char** paths) {
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                if (!self) return;
                std::vector<std::string> path_list;
                path_list.reserve(count);
                for (int i = 0; i < count; ++i) {
                    path_list.emplace_back(paths[i]);
                }
                DropEvent ev(std::move(path_list));
                self->dispatch_event(ev);
            });
        }

    public:
        Window() = default;
        /// 支持懒人构建
        inline Window(const CreateWindowInfo & ci){
            create(ci);
        }
        /// 如果忘了，也给你擦屁股
        inline ~Window(){ destroy(); }

        /// 提供移动
        Window(Window && win) noexcept
            : window(win.window)
            , m_event_callback(std::move(win.m_event_callback))
            , m_listeners(std::move(win.m_listeners))
            , m_next_listener_id(win.m_next_listener_id)
            , m_bound_input(win.m_bound_input) {
            win.window = nullptr;
            win.m_bound_input = nullptr;
            if (window) {
                glfwSetWindowUserPointer(window, this);
            }
        }

        Window& operator=(Window && win) noexcept {
            if(&win == this) return *this;
            panic_debug(window != nullptr, "Cannot move window to an object that has a window already.");
            if(window != nullptr) [[unlikely]] {
                destroy();
            }
            
            window = win.window;
            m_event_callback = std::move(win.m_event_callback);
            m_listeners = std::move(win.m_listeners);
            m_next_listener_id = win.m_next_listener_id;
            m_bound_input = win.m_bound_input;

            win.window = nullptr;
            win.m_bound_input = nullptr;

            if (window) {
                glfwSetWindowUserPointer(window, this);
            }
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

            glfwSetWindowUserPointer(window, this);
            setup_callbacks();

            return true;
        }

        inline void destroy() noexcept {
            if(window){
                glfwSetWindowUserPointer(window, nullptr);
                glfwDestroyWindow(window);
                window = nullptr;
            }
        }

        // ==========================================
        // 事件系统接口 (Event System API)
        // ==========================================

        inline void set_event_callback(std::function<void(Event&)> cb) {
            m_event_callback = std::move(cb);
        }

        inline alib6::u64 add_event_listener(std::function<void(Event&)> cb) {
            alib6::u64 id = m_next_listener_id++;
            m_listeners.push_back({ id, std::move(cb) });
            return id;
        }

        inline bool remove_event_listener(alib6::u64 id) {
            auto it = std::remove_if(m_listeners.begin(), m_listeners.end(), [id](const auto& entry) {
                return entry.id == id;
            });
            if (it != m_listeners.end()) {
                m_listeners.erase(it, m_listeners.end());
                return true;
            }
            return false;
        }

        inline void bind_input(Input& input) noexcept {
            m_bound_input = &input;
        }

        inline void unbind_input() noexcept {
            m_bound_input = nullptr;
        }

        inline Input* get_bound_input() const noexcept {
            return m_bound_input;
        }

        void dispatch_event(Event& e) {
            // 优先更新绑定的输入状态机
            if (m_bound_input) {
                m_bound_input->on_event(e);
            }

            // 主事件回调
            if (m_event_callback) {
                m_event_callback(e);
            }

            // 多监听器（支持消费拦截）
            for (auto& listener : m_listeners) {
                if (e.handled) break;
                listener.callback(e);
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
