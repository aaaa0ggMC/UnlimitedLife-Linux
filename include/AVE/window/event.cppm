module;
#include <AVE/config.h>
export module ave.window:event;

import std;
import alib6;
import :keycode;

export namespace ave {

    enum class EventType : alib6::u32 {
        None = 0,
        // Window
        WindowClose,
        WindowResize,
        WindowFramebufferResize,
        WindowMove,
        WindowFocus,
        WindowIconify,
        WindowMaximize,
        WindowContentScale,
        // Key
        KeyPressed,
        KeyReleased,
        KeyRepeat,
        // Mouse
        MouseButtonPressed,
        MouseButtonReleased,
        MouseMove,
        MouseScrolled,
        MouseEnter,
        // Char
        CharInput,
        // Drop
        FileDrop
    };

    enum class EventCategory : alib6::u32 {
        None               = 0,
        Window             = 1 << 0,
        Input              = 1 << 1,
        Keyboard           = 1 << 2,
        Mouse              = 1 << 3,
        MouseButton        = 1 << 4
    };

    inline constexpr EventCategory operator|(EventCategory lhs, EventCategory rhs) noexcept {
        return static_cast<EventCategory>(static_cast<alib6::u32>(lhs) | static_cast<alib6::u32>(rhs));
    }

    inline constexpr EventCategory operator&(EventCategory lhs, EventCategory rhs) noexcept {
        return static_cast<EventCategory>(static_cast<alib6::u32>(lhs) & static_cast<alib6::u32>(rhs));
    }

    inline constexpr bool in_category(EventCategory categories, EventCategory target) noexcept {
        return (static_cast<alib6::u32>(categories) & static_cast<alib6::u32>(target)) != 0;
    }

    class AVE_API Event {
    public:
        bool handled { false };

        virtual ~Event() = default;
        virtual EventType get_type() const noexcept = 0;
        virtual std::string_view get_name() const noexcept = 0;
        virtual alib6::u32 get_category_flags() const noexcept = 0;

        inline bool is_in_category(EventCategory category) const noexcept {
            return (get_category_flags() & static_cast<alib6::u32>(category)) != 0;
        }
    };

    // ==========================================
    // 窗口事件 (Window Events)
    // ==========================================

    class AVE_API WindowResizeEvent : public Event {
    public:
        int width { 0 };
        int height { 0 };

        constexpr WindowResizeEvent(int w, int h) noexcept : width(w), height(h) {}

        static constexpr EventType get_static_type() noexcept { return EventType::WindowResize; }
        EventType get_type() const noexcept override { return get_static_type(); }
        std::string_view get_name() const noexcept override { return "WindowResize"; }
        alib6::u32 get_category_flags() const noexcept override {
            return static_cast<alib6::u32>(EventCategory::Window);
        }
    };

    class AVE_API WindowFramebufferResizeEvent : public Event {
    public:
        int width { 0 };
        int height { 0 };

        constexpr WindowFramebufferResizeEvent(int w, int h) noexcept : width(w), height(h) {}

        static constexpr EventType get_static_type() noexcept { return EventType::WindowFramebufferResize; }
        EventType get_type() const noexcept override { return get_static_type(); }
        std::string_view get_name() const noexcept override { return "WindowFramebufferResize"; }
        alib6::u32 get_category_flags() const noexcept override {
            return static_cast<alib6::u32>(EventCategory::Window);
        }
    };

    class AVE_API WindowCloseEvent : public Event {
    public:
        constexpr WindowCloseEvent() noexcept = default;

        static constexpr EventType get_static_type() noexcept { return EventType::WindowClose; }
        EventType get_type() const noexcept override { return get_static_type(); }
        std::string_view get_name() const noexcept override { return "WindowClose"; }
        alib6::u32 get_category_flags() const noexcept override {
            return static_cast<alib6::u32>(EventCategory::Window);
        }
    };

    class AVE_API WindowMoveEvent : public Event {
    public:
        int x { 0 };
        int y { 0 };

        constexpr WindowMoveEvent(int xpos, int ypos) noexcept : x(xpos), y(ypos) {}

        static constexpr EventType get_static_type() noexcept { return EventType::WindowMove; }
        EventType get_type() const noexcept override { return get_static_type(); }
        std::string_view get_name() const noexcept override { return "WindowMove"; }
        alib6::u32 get_category_flags() const noexcept override {
            return static_cast<alib6::u32>(EventCategory::Window);
        }
    };

    class AVE_API WindowFocusEvent : public Event {
    public:
        bool focused { false };

        constexpr explicit WindowFocusEvent(bool focus) noexcept : focused(focus) {}

        static constexpr EventType get_static_type() noexcept { return EventType::WindowFocus; }
        EventType get_type() const noexcept override { return get_static_type(); }
        std::string_view get_name() const noexcept override { return "WindowFocus"; }
        alib6::u32 get_category_flags() const noexcept override {
            return static_cast<alib6::u32>(EventCategory::Window);
        }
    };

    class AVE_API WindowIconifyEvent : public Event {
    public:
        bool iconified { false };

        constexpr explicit WindowIconifyEvent(bool iconify) noexcept : iconified(iconify) {}

        static constexpr EventType get_static_type() noexcept { return EventType::WindowIconify; }
        EventType get_type() const noexcept override { return get_static_type(); }
        std::string_view get_name() const noexcept override { return "WindowIconify"; }
        alib6::u32 get_category_flags() const noexcept override {
            return static_cast<alib6::u32>(EventCategory::Window);
        }
    };

    class AVE_API WindowMaximizeEvent : public Event {
    public:
        bool maximized { false };

        constexpr explicit WindowMaximizeEvent(bool max) noexcept : maximized(max) {}

        static constexpr EventType get_static_type() noexcept { return EventType::WindowMaximize; }
        EventType get_type() const noexcept override { return get_static_type(); }
        std::string_view get_name() const noexcept override { return "WindowMaximize"; }
        alib6::u32 get_category_flags() const noexcept override {
            return static_cast<alib6::u32>(EventCategory::Window);
        }
    };

    class AVE_API WindowContentScaleEvent : public Event {
    public:
        float xscale { 1.0f };
        float yscale { 1.0f };

        constexpr WindowContentScaleEvent(float xs, float ys) noexcept : xscale(xs), yscale(ys) {}

        static constexpr EventType get_static_type() noexcept { return EventType::WindowContentScale; }
        EventType get_type() const noexcept override { return get_static_type(); }
        std::string_view get_name() const noexcept override { return "WindowContentScale"; }
        alib6::u32 get_category_flags() const noexcept override {
            return static_cast<alib6::u32>(EventCategory::Window);
        }
    };

    // ==========================================
    // 键盘事件 (Key Events)
    // ==========================================

    class AVE_API KeyEvent : public Event {
    public:
        KeyCode key { KeyCode::Unknown };
        int scancode { 0 };
        KeyModifier mods { KeyModifier::None };

        constexpr KeyEvent(KeyCode k, int scan, KeyModifier m) noexcept
            : key(k), scancode(scan), mods(m) {}

        alib6::u32 get_category_flags() const noexcept override {
            return static_cast<alib6::u32>(EventCategory::Keyboard | EventCategory::Input);
        }

        inline bool has_shift() const noexcept { return has_modifier(mods, KeyModifier::Shift); }
        inline bool has_control() const noexcept { return has_modifier(mods, KeyModifier::Control); }
        inline bool has_alt() const noexcept { return has_modifier(mods, KeyModifier::Alt); }
        inline bool has_super() const noexcept { return has_modifier(mods, KeyModifier::Super); }
        inline bool has_caps_lock() const noexcept { return has_modifier(mods, KeyModifier::CapsLock); }
        inline bool has_num_lock() const noexcept { return has_modifier(mods, KeyModifier::NumLock); }

        inline std::string_view get_key_name() const noexcept {
            return ave::get_key_name(key);
        }
    };

    class AVE_API KeyPressedEvent : public KeyEvent {
    public:
        int repeat_count { 0 };

        constexpr KeyPressedEvent(KeyCode k, int scan, KeyModifier m, int repeat = 0) noexcept
            : KeyEvent(k, scan, m), repeat_count(repeat) {}

        static constexpr EventType get_static_type() noexcept { return EventType::KeyPressed; }
        EventType get_type() const noexcept override { return get_static_type(); }
        std::string_view get_name() const noexcept override { return "KeyPressed"; }
    };

    class AVE_API KeyReleasedEvent : public KeyEvent {
    public:
        constexpr KeyReleasedEvent(KeyCode k, int scan, KeyModifier m) noexcept
            : KeyEvent(k, scan, m) {}

        static constexpr EventType get_static_type() noexcept { return EventType::KeyReleased; }
        EventType get_type() const noexcept override { return get_static_type(); }
        std::string_view get_name() const noexcept override { return "KeyReleased"; }
    };

    class AVE_API KeyRepeatEvent : public KeyEvent {
    public:
        int repeat_count { 1 };

        constexpr KeyRepeatEvent(KeyCode k, int scan, KeyModifier m, int repeat = 1) noexcept
            : KeyEvent(k, scan, m), repeat_count(repeat) {}

        static constexpr EventType get_static_type() noexcept { return EventType::KeyRepeat; }
        EventType get_type() const noexcept override { return get_static_type(); }
        std::string_view get_name() const noexcept override { return "KeyRepeat"; }
    };

    // ==========================================
    // 鼠标事件 (Mouse Events)
    // ==========================================

    class AVE_API MouseButtonEvent : public Event {
    public:
        MouseButton button { MouseButton::Left };
        KeyModifier mods { KeyModifier::None };

        constexpr MouseButtonEvent(MouseButton b, KeyModifier m) noexcept
            : button(b), mods(m) {}

        alib6::u32 get_category_flags() const noexcept override {
            return static_cast<alib6::u32>(EventCategory::Mouse | EventCategory::MouseButton | EventCategory::Input);
        }

        inline bool has_shift() const noexcept { return has_modifier(mods, KeyModifier::Shift); }
        inline bool has_control() const noexcept { return has_modifier(mods, KeyModifier::Control); }
        inline bool has_alt() const noexcept { return has_modifier(mods, KeyModifier::Alt); }
        inline bool has_super() const noexcept { return has_modifier(mods, KeyModifier::Super); }

        inline std::string_view get_button_name() const noexcept {
            return ave::get_mouse_button_name(button);
        }
    };

    class AVE_API MouseButtonPressedEvent : public MouseButtonEvent {
    public:
        constexpr MouseButtonPressedEvent(MouseButton b, KeyModifier m) noexcept
            : MouseButtonEvent(b, m) {}

        static constexpr EventType get_static_type() noexcept { return EventType::MouseButtonPressed; }
        EventType get_type() const noexcept override { return get_static_type(); }
        std::string_view get_name() const noexcept override { return "MouseButtonPressed"; }
    };

    class AVE_API MouseButtonReleasedEvent : public MouseButtonEvent {
    public:
        constexpr MouseButtonReleasedEvent(MouseButton b, KeyModifier m) noexcept
            : MouseButtonEvent(b, m) {}

        static constexpr EventType get_static_type() noexcept { return EventType::MouseButtonReleased; }
        EventType get_type() const noexcept override { return get_static_type(); }
        std::string_view get_name() const noexcept override { return "MouseButtonReleased"; }
    };

    class AVE_API MouseMoveEvent : public Event {
    public:
        double x { 0.0 };
        double y { 0.0 };

        constexpr MouseMoveEvent(double xpos, double ypos) noexcept
            : x(xpos), y(ypos) {}

        static constexpr EventType get_static_type() noexcept { return EventType::MouseMove; }
        EventType get_type() const noexcept override { return get_static_type(); }
        std::string_view get_name() const noexcept override { return "MouseMove"; }
        alib6::u32 get_category_flags() const noexcept override {
            return static_cast<alib6::u32>(EventCategory::Mouse | EventCategory::Input);
        }
    };

    class AVE_API MouseScrollEvent : public Event {
    public:
        double x_offset { 0.0 };
        double y_offset { 0.0 };

        constexpr MouseScrollEvent(double xoff, double yoff) noexcept
            : x_offset(xoff), y_offset(yoff) {}

        static constexpr EventType get_static_type() noexcept { return EventType::MouseScrolled; }
        EventType get_type() const noexcept override { return get_static_type(); }
        std::string_view get_name() const noexcept override { return "MouseScrolled"; }
        alib6::u32 get_category_flags() const noexcept override {
            return static_cast<alib6::u32>(EventCategory::Mouse | EventCategory::Input);
        }
    };

    class AVE_API MouseEnterEvent : public Event {
    public:
        bool entered { false };

        constexpr explicit MouseEnterEvent(bool enter) noexcept : entered(enter) {}

        static constexpr EventType get_static_type() noexcept { return EventType::MouseEnter; }
        EventType get_type() const noexcept override { return get_static_type(); }
        std::string_view get_name() const noexcept override { return "MouseEnter"; }
        alib6::u32 get_category_flags() const noexcept override {
            return static_cast<alib6::u32>(EventCategory::Mouse | EventCategory::Input);
        }
    };

    // ==========================================
    // 字符 / 输入法事件 (Char Event)
    // ==========================================

    class AVE_API CharEvent : public Event {
    public:
        char32_t codepoint { 0 };

        constexpr explicit CharEvent(char32_t cp) noexcept : codepoint(cp) {}

        static constexpr EventType get_static_type() noexcept { return EventType::CharInput; }
        EventType get_type() const noexcept override { return get_static_type(); }
        std::string_view get_name() const noexcept override { return "CharInput"; }
        alib6::u32 get_category_flags() const noexcept override {
            return static_cast<alib6::u32>(EventCategory::Keyboard | EventCategory::Input);
        }
    };

    // ==========================================
    // 文件拖放事件 (Drop Event)
    // ==========================================

    class AVE_API DropEvent : public Event {
    public:
        std::vector<std::string> paths;

        DropEvent(std::vector<std::string> p) noexcept : paths(std::move(p)) {}

        static constexpr EventType get_static_type() noexcept { return EventType::FileDrop; }
        EventType get_type() const noexcept override { return get_static_type(); }
        std::string_view get_name() const noexcept override { return "FileDrop"; }
        alib6::u32 get_category_flags() const noexcept override {
            return static_cast<alib6::u32>(EventCategory::Window);
        }
    };

    // ==========================================
    // 事件分发器 (Event Dispatcher)
    // ==========================================

    class AVE_API EventDispatcher {
    private:
        Event& m_event;
    public:
        explicit EventDispatcher(Event& event) noexcept : m_event(event) {}

        template<typename T, typename F>
        bool dispatch(F&& func) {
            if (m_event.get_type() == T::get_static_type()) {
                m_event.handled |= static_cast<bool>(func(static_cast<T&>(m_event)));
                return true;
            }
            return false;
        }
    };

}
