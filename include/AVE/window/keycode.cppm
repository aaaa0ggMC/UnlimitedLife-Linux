module;
#include <AVE/config.h>
#include <GLFW/glfw3.h>
export module ave.window:keycode;

import std;
import alib6;

export namespace ave {

    enum class KeyCode : int {
        Unknown      = GLFW_KEY_UNKNOWN,

        // 字母键
        A            = GLFW_KEY_A,
        B            = GLFW_KEY_B,
        C            = GLFW_KEY_C,
        D            = GLFW_KEY_D,
        E            = GLFW_KEY_E,
        F            = GLFW_KEY_F,
        G            = GLFW_KEY_G,
        H            = GLFW_KEY_H,
        I            = GLFW_KEY_I,
        J            = GLFW_KEY_J,
        K            = GLFW_KEY_K,
        L            = GLFW_KEY_L,
        M            = GLFW_KEY_M,
        N            = GLFW_KEY_N,
        O            = GLFW_KEY_O,
        P            = GLFW_KEY_P,
        Q            = GLFW_KEY_Q,
        R            = GLFW_KEY_R,
        S            = GLFW_KEY_S,
        T            = GLFW_KEY_T,
        U            = GLFW_KEY_U,
        V            = GLFW_KEY_V,
        W            = GLFW_KEY_W,
        X            = GLFW_KEY_X,
        Y            = GLFW_KEY_Y,
        Z            = GLFW_KEY_Z,

        // 数字键
        Num0         = GLFW_KEY_0,
        Num1         = GLFW_KEY_1,
        Num2         = GLFW_KEY_2,
        Num3         = GLFW_KEY_3,
        Num4         = GLFW_KEY_4,
        Num5         = GLFW_KEY_5,
        Num6         = GLFW_KEY_6,
        Num7         = GLFW_KEY_7,
        Num8         = GLFW_KEY_8,
        Num9         = GLFW_KEY_9,

        // 功能键 / 符号
        Space        = GLFW_KEY_SPACE,
        Apostrophe   = GLFW_KEY_APOSTROPHE,
        Comma        = GLFW_KEY_COMMA,
        Minus        = GLFW_KEY_MINUS,
        Period       = GLFW_KEY_PERIOD,
        Slash        = GLFW_KEY_SLASH,
        Semicolon    = GLFW_KEY_SEMICOLON,
        Equal        = GLFW_KEY_EQUAL,
        LeftBracket  = GLFW_KEY_LEFT_BRACKET,
        RightBracket = GLFW_KEY_RIGHT_BRACKET,
        Backslash    = GLFW_KEY_BACKSLASH,
        GraveAccent  = GLFW_KEY_GRAVE_ACCENT,
        World1       = GLFW_KEY_WORLD_1,
        World2       = GLFW_KEY_WORLD_2,

        // 控制键
        Escape       = GLFW_KEY_ESCAPE,
        Enter        = GLFW_KEY_ENTER,
        Tab          = GLFW_KEY_TAB,
        Backspace    = GLFW_KEY_BACKSPACE,
        Insert       = GLFW_KEY_INSERT,
        Delete       = GLFW_KEY_DELETE,
        Right        = GLFW_KEY_RIGHT,
        Left         = GLFW_KEY_LEFT,
        Down         = GLFW_KEY_DOWN,
        Up           = GLFW_KEY_UP,
        PageUp       = GLFW_KEY_PAGE_UP,
        PageDown     = GLFW_KEY_PAGE_DOWN,
        Home         = GLFW_KEY_HOME,
        End          = GLFW_KEY_END,
        CapsLock     = GLFW_KEY_CAPS_LOCK,
        ScrollLock   = GLFW_KEY_SCROLL_LOCK,
        NumLock      = GLFW_KEY_NUM_LOCK,
        PrintScreen  = GLFW_KEY_PRINT_SCREEN,
        Pause        = GLFW_KEY_PAUSE,

        // F1 - F25
        F1           = GLFW_KEY_F1,
        F2           = GLFW_KEY_F2,
        F3           = GLFW_KEY_F3,
        F4           = GLFW_KEY_F4,
        F5           = GLFW_KEY_F5,
        F6           = GLFW_KEY_F6,
        F7           = GLFW_KEY_F7,
        F8           = GLFW_KEY_F8,
        F9           = GLFW_KEY_F9,
        F10          = GLFW_KEY_F10,
        F11          = GLFW_KEY_F11,
        F12          = GLFW_KEY_F12,
        F13          = GLFW_KEY_F13,
        F14          = GLFW_KEY_F14,
        F15          = GLFW_KEY_F15,
        F16          = GLFW_KEY_F16,
        F17          = GLFW_KEY_F17,
        F18          = GLFW_KEY_F18,
        F19          = GLFW_KEY_F19,
        F20          = GLFW_KEY_F20,
        F21          = GLFW_KEY_F21,
        F22          = GLFW_KEY_F22,
        F23          = GLFW_KEY_F23,
        F24          = GLFW_KEY_F24,
        F25          = GLFW_KEY_F25,

        // 小键盘
        KP0          = GLFW_KEY_KP_0,
        KP1          = GLFW_KEY_KP_1,
        KP2          = GLFW_KEY_KP_2,
        KP3          = GLFW_KEY_KP_3,
        KP4          = GLFW_KEY_KP_4,
        KP5          = GLFW_KEY_KP_5,
        KP6          = GLFW_KEY_KP_6,
        KP7          = GLFW_KEY_KP_7,
        KP8          = GLFW_KEY_KP_8,
        KP9          = GLFW_KEY_KP_9,
        KPDecimal    = GLFW_KEY_KP_DECIMAL,
        KPDivide     = GLFW_KEY_KP_DIVIDE,
        KPMultiply   = GLFW_KEY_KP_MULTIPLY,
        KPSubtract   = GLFW_KEY_KP_SUBTRACT,
        KPAdd        = GLFW_KEY_KP_ADD,
        KPEnter      = GLFW_KEY_KP_ENTER,
        KPEqual      = GLFW_KEY_KP_EQUAL,

        // 修饰键
        LeftShift    = GLFW_KEY_LEFT_SHIFT,
        LeftControl  = GLFW_KEY_LEFT_CONTROL,
        LeftAlt      = GLFW_KEY_LEFT_ALT,
        LeftSuper    = GLFW_KEY_LEFT_SUPER,
        RightShift   = GLFW_KEY_RIGHT_SHIFT,
        RightControl = GLFW_KEY_RIGHT_CONTROL,
        RightAlt     = GLFW_KEY_RIGHT_ALT,
        RightSuper   = GLFW_KEY_RIGHT_SUPER,
        Menu         = GLFW_KEY_MENU
    };

    inline constexpr size_t max_key_code = 512;

    enum class KeyAction : int {
        Release = GLFW_RELEASE,
        Press   = GLFW_PRESS,
        Repeat  = GLFW_REPEAT
    };

    enum class KeyModifier : alib6::u32 {
        None     = 0,
        Shift    = GLFW_MOD_SHIFT,
        Control  = GLFW_MOD_CONTROL,
        Alt      = GLFW_MOD_ALT,
        Super    = GLFW_MOD_SUPER,
        CapsLock = GLFW_MOD_CAPS_LOCK,
        NumLock  = GLFW_MOD_NUM_LOCK
    };

    inline constexpr KeyModifier operator|(KeyModifier lhs, KeyModifier rhs) noexcept {
        return static_cast<KeyModifier>(static_cast<alib6::u32>(lhs) | static_cast<alib6::u32>(rhs));
    }

    inline constexpr KeyModifier operator&(KeyModifier lhs, KeyModifier rhs) noexcept {
        return static_cast<KeyModifier>(static_cast<alib6::u32>(lhs) & static_cast<alib6::u32>(rhs));
    }

    inline constexpr KeyModifier operator~(KeyModifier mod) noexcept {
        return static_cast<KeyModifier>(~static_cast<alib6::u32>(mod));
    }

    inline constexpr bool has_modifier(KeyModifier mods, KeyModifier mod) noexcept {
        return (static_cast<alib6::u32>(mods) & static_cast<alib6::u32>(mod)) != 0;
    }

    enum class MouseButton : int {
        Button1 = GLFW_MOUSE_BUTTON_1,
        Button2 = GLFW_MOUSE_BUTTON_2,
        Button3 = GLFW_MOUSE_BUTTON_3,
        Button4 = GLFW_MOUSE_BUTTON_4,
        Button5 = GLFW_MOUSE_BUTTON_5,
        Button6 = GLFW_MOUSE_BUTTON_6,
        Button7 = GLFW_MOUSE_BUTTON_7,
        Button8 = GLFW_MOUSE_BUTTON_8,

        Left    = GLFW_MOUSE_BUTTON_LEFT,
        Right   = GLFW_MOUSE_BUTTON_RIGHT,
        Middle  = GLFW_MOUSE_BUTTON_MIDDLE,
        Last    = GLFW_MOUSE_BUTTON_LAST
    };

    inline constexpr size_t max_mouse_button = 8;

    enum class KeyState : alib6::u8 {
        Released = 0,
        PressedThisTick = 1,
        Held = 2,
        ReleasedThisTick = 3
    };

    struct KeyInfo {
        KeyState status { KeyState::Released };
        float double_time_ms { -1.0f }; // 小于等于0表示未开启双击检测
        float last_pressed { -1.0f };
        float this_pressed { -1.0f };

        inline constexpr KeyInfo() noexcept = default;

        inline bool is_down(bool invalid_during_double_time = false) const noexcept {
            if (!invalid_during_double_time) {
                return (status == KeyState::PressedThisTick) || (status == KeyState::Held);
            }
            return ((status == KeyState::PressedThisTick) || (status == KeyState::Held)) && (this_pressed != last_pressed);
        }

        inline bool is_up() const noexcept {
            return (status == KeyState::Released) || (status == KeyState::ReleasedThisTick);
        }

        inline bool is_just_pressed() const noexcept {
            return status == KeyState::PressedThisTick;
        }

        inline bool is_just_released() const noexcept {
            return status == KeyState::ReleasedThisTick;
        }

        inline bool has_double_tapped() noexcept {
            if (double_time_ms <= 0.0f) return false;
            if (status != KeyState::PressedThisTick) return false;
            if (last_pressed >= 0.0f && this_pressed != last_pressed && (this_pressed - last_pressed) <= double_time_ms) {
                last_pressed = -1.0f; // 消耗掉双击
                return true;
            }
            last_pressed = this_pressed;
            return false;
        }
    };

    constexpr std::string_view get_key_name(KeyCode code) noexcept {
        switch (code) {
            case KeyCode::A: return "A";
            case KeyCode::B: return "B";
            case KeyCode::C: return "C";
            case KeyCode::D: return "D";
            case KeyCode::E: return "E";
            case KeyCode::F: return "F";
            case KeyCode::G: return "G";
            case KeyCode::H: return "H";
            case KeyCode::I: return "I";
            case KeyCode::J: return "J";
            case KeyCode::K: return "K";
            case KeyCode::L: return "L";
            case KeyCode::M: return "M";
            case KeyCode::N: return "N";
            case KeyCode::O: return "O";
            case KeyCode::P: return "P";
            case KeyCode::Q: return "Q";
            case KeyCode::R: return "R";
            case KeyCode::S: return "S";
            case KeyCode::T: return "T";
            case KeyCode::U: return "U";
            case KeyCode::V: return "V";
            case KeyCode::W: return "W";
            case KeyCode::X: return "X";
            case KeyCode::Y: return "Y";
            case KeyCode::Z: return "Z";

            case KeyCode::Num0: return "0";
            case KeyCode::Num1: return "1";
            case KeyCode::Num2: return "2";
            case KeyCode::Num3: return "3";
            case KeyCode::Num4: return "4";
            case KeyCode::Num5: return "5";
            case KeyCode::Num6: return "6";
            case KeyCode::Num7: return "7";
            case KeyCode::Num8: return "8";
            case KeyCode::Num9: return "9";

            case KeyCode::Space: return "Space";
            case KeyCode::Apostrophe: return "'";
            case KeyCode::Comma: return ",";
            case KeyCode::Minus: return "-";
            case KeyCode::Period: return ".";
            case KeyCode::Slash: return "/";
            case KeyCode::Semicolon: return ";";
            case KeyCode::Equal: return "=";
            case KeyCode::LeftBracket: return "[";
            case KeyCode::RightBracket: return "]";
            case KeyCode::Backslash: return "\\";
            case KeyCode::GraveAccent: return "`";

            case KeyCode::Escape: return "Escape";
            case KeyCode::Enter: return "Enter";
            case KeyCode::Tab: return "Tab";
            case KeyCode::Backspace: return "Backspace";
            case KeyCode::Insert: return "Insert";
            case KeyCode::Delete: return "Delete";
            case KeyCode::Right: return "Right";
            case KeyCode::Left: return "Left";
            case KeyCode::Down: return "Down";
            case KeyCode::Up: return "Up";
            case KeyCode::PageUp: return "PageUp";
            case KeyCode::PageDown: return "PageDown";
            case KeyCode::Home: return "Home";
            case KeyCode::End: return "End";
            case KeyCode::CapsLock: return "CapsLock";
            case KeyCode::ScrollLock: return "ScrollLock";
            case KeyCode::NumLock: return "NumLock";
            case KeyCode::PrintScreen: return "PrintScreen";
            case KeyCode::Pause: return "Pause";

            case KeyCode::F1: return "F1";
            case KeyCode::F2: return "F2";
            case KeyCode::F3: return "F3";
            case KeyCode::F4: return "F4";
            case KeyCode::F5: return "F5";
            case KeyCode::F6: return "F6";
            case KeyCode::F7: return "F7";
            case KeyCode::F8: return "F8";
            case KeyCode::F9: return "F9";
            case KeyCode::F10: return "F10";
            case KeyCode::F11: return "F11";
            case KeyCode::F12: return "F12";
            case KeyCode::F13: return "F13";
            case KeyCode::F14: return "F14";
            case KeyCode::F15: return "F15";
            case KeyCode::F16: return "F16";
            case KeyCode::F17: return "F17";
            case KeyCode::F18: return "F18";
            case KeyCode::F19: return "F19";
            case KeyCode::F20: return "F20";
            case KeyCode::F21: return "F21";
            case KeyCode::F22: return "F22";
            case KeyCode::F23: return "F23";
            case KeyCode::F24: return "F24";
            case KeyCode::F25: return "F25";

            case KeyCode::KP0: return "Num0";
            case KeyCode::KP1: return "Num1";
            case KeyCode::KP2: return "Num2";
            case KeyCode::KP3: return "Num3";
            case KeyCode::KP4: return "Num4";
            case KeyCode::KP5: return "Num5";
            case KeyCode::KP6: return "Num6";
            case KeyCode::KP7: return "Num7";
            case KeyCode::KP8: return "Num8";
            case KeyCode::KP9: return "Num9";
            case KeyCode::KPDecimal: return "NumDecimal";
            case KeyCode::KPDivide: return "NumDivide";
            case KeyCode::KPMultiply: return "NumMultiply";
            case KeyCode::KPSubtract: return "NumSubtract";
            case KeyCode::KPAdd: return "NumAdd";
            case KeyCode::KPEnter: return "NumEnter";
            case KeyCode::KPEqual: return "NumEqual";

            case KeyCode::LeftShift: return "LeftShift";
            case KeyCode::LeftControl: return "LeftControl";
            case KeyCode::LeftAlt: return "LeftAlt";
            case KeyCode::LeftSuper: return "LeftSuper";
            case KeyCode::RightShift: return "RightShift";
            case KeyCode::RightControl: return "RightControl";
            case KeyCode::RightAlt: return "RightAlt";
            case KeyCode::RightSuper: return "RightSuper";
            case KeyCode::Menu: return "Menu";

            default: return "Unknown";
        }
    }

    constexpr std::string_view get_mouse_button_name(MouseButton button) noexcept {
        switch (button) {
            case MouseButton::Left: return "MouseLeft";
            case MouseButton::Right: return "MouseRight";
            case MouseButton::Middle: return "MouseMiddle";
            case MouseButton::Button4: return "MouseButton4";
            case MouseButton::Button5: return "MouseButton5";
            case MouseButton::Button6: return "MouseButton6";
            case MouseButton::Button7: return "MouseButton7";
            case MouseButton::Button8: return "MouseButton8";
            default: return "UnknownMouse";
        }
    }

}
