module;
#include <AVE/config.h>
export module ave.window:input;

import std;
import alib6;
import :keycode;
import :event;

export namespace ave {

    class AVE_API Input {
    private:
        std::array<KeyInfo, max_key_code> m_keys {};
        std::array<KeyInfo, max_mouse_button> m_mouse_buttons {};

        double m_mouse_x { 0.0 };
        double m_mouse_y { 0.0 };
        double m_prev_mouse_x { 0.0 };
        double m_prev_mouse_y { 0.0 };
        double m_mouse_delta_x { 0.0 };
        double m_mouse_delta_y { 0.0 };
        double m_scroll_delta_x { 0.0 };
        double m_scroll_delta_y { 0.0 };
        bool m_first_mouse_move { true };
        bool m_mouse_entered { true };

        KeyModifier m_modifiers { KeyModifier::None };
        float m_default_double_tap_ms { 250.0f };

        // 虚拟按键动作与轴映射
        struct ActionBinding {
            std::vector<KeyCode> keys;
            std::vector<MouseButton> mouse_buttons;
        };

        struct AxisBinding {
            KeyCode pos_key { KeyCode::Unknown };
            KeyCode neg_key { KeyCode::Unknown };
        };

        std::unordered_map<std::string, ActionBinding> m_actions;
        std::unordered_map<std::string, AxisBinding> m_axes;

        inline static float get_now_ms() noexcept {
            auto now = std::chrono::steady_clock::now();
            return std::chrono::duration<float, std::milli>(now.time_since_epoch()).count();
        }

    public:
        Input() {
            // 默认所有按键双击阈值为 250ms
            for (auto& k : m_keys) {
                k.double_time_ms = m_default_double_tap_ms;
            }
            for (auto& m : m_mouse_buttons) {
                m.double_time_ms = m_default_double_tap_ms;
            }
        }

        /// 每帧开始时调用，推进按键生命周期并重置帧间增量
        void new_frame() noexcept {
            // 推进按键状态
            for (auto& k : m_keys) {
                if (k.status == KeyState::PressedThisTick) {
                    k.status = KeyState::Held;
                } else if (k.status == KeyState::ReleasedThisTick) {
                    k.status = KeyState::Released;
                }
            }

            // 推进鼠标按键状态
            for (auto& m : m_mouse_buttons) {
                if (m.status == KeyState::PressedThisTick) {
                    m.status = KeyState::Held;
                } else if (m.status == KeyState::ReleasedThisTick) {
                    m.status = KeyState::Released;
                }
            }

            // 更新鼠标增量
            m_mouse_delta_x = m_mouse_x - m_prev_mouse_x;
            m_mouse_delta_y = m_mouse_y - m_prev_mouse_y;
            m_prev_mouse_x = m_mouse_x;
            m_prev_mouse_y = m_mouse_y;

            // 重置滚轮增量
            m_scroll_delta_x = 0.0;
            m_scroll_delta_y = 0.0;
        }

        /// 兼容 AGE 的 update 命名
        inline void update() noexcept {
            new_frame();
        }

        /// 接收事件驱动系统的事件，实时同步状态机（零查询开销，绝不漏键）
        void on_event(Event& e) noexcept {
            EventDispatcher dispatcher(e);

            dispatcher.dispatch<KeyPressedEvent>([this](KeyPressedEvent& ev) {
                int key_idx = static_cast<int>(ev.key);
                if (key_idx >= 0 && key_idx < static_cast<int>(max_key_code)) {
                    auto& k = m_keys[key_idx];
                    k.status = KeyState::PressedThisTick;
                    k.this_pressed = get_now_ms();
                }
                m_modifiers = ev.mods;
                return false;
            });

            dispatcher.dispatch<KeyReleasedEvent>([this](KeyReleasedEvent& ev) {
                int key_idx = static_cast<int>(ev.key);
                if (key_idx >= 0 && key_idx < static_cast<int>(max_key_code)) {
                    auto& k = m_keys[key_idx];
                    k.status = KeyState::ReleasedThisTick;
                }
                m_modifiers = ev.mods;
                return false;
            });

            dispatcher.dispatch<MouseButtonPressedEvent>([this](MouseButtonPressedEvent& ev) {
                int btn_idx = static_cast<int>(ev.button);
                if (btn_idx >= 0 && btn_idx < static_cast<int>(max_mouse_button)) {
                    auto& m = m_mouse_buttons[btn_idx];
                    m.status = KeyState::PressedThisTick;
                    m.this_pressed = get_now_ms();
                }
                m_modifiers = ev.mods;
                return false;
            });

            dispatcher.dispatch<MouseButtonReleasedEvent>([this](MouseButtonReleasedEvent& ev) {
                int btn_idx = static_cast<int>(ev.button);
                if (btn_idx >= 0 && btn_idx < static_cast<int>(max_mouse_button)) {
                    auto& m = m_mouse_buttons[btn_idx];
                    m.status = KeyState::ReleasedThisTick;
                }
                m_modifiers = ev.mods;
                return false;
            });

            dispatcher.dispatch<MouseMoveEvent>([this](MouseMoveEvent& ev) {
                if (m_first_mouse_move) {
                    m_prev_mouse_x = ev.x;
                    m_prev_mouse_y = ev.y;
                    m_first_mouse_move = false;
                }
                m_mouse_x = ev.x;
                m_mouse_y = ev.y;
                return false;
            });

            dispatcher.dispatch<MouseScrollEvent>([this](MouseScrollEvent& ev) {
                m_scroll_delta_x += ev.x_offset;
                m_scroll_delta_y += ev.y_offset;
                return false;
            });

            dispatcher.dispatch<MouseEnterEvent>([this](MouseEnterEvent& ev) {
                m_mouse_entered = ev.entered;
                return false;
            });

            dispatcher.dispatch<WindowFocusEvent>([this](WindowFocusEvent& ev) {
                if (!ev.focused) {
                    reset_state();
                }
                return false;
            });
        }

        // ==========================================
        // 键盘状态查询接口
        // ==========================================

        inline bool is_key_down(KeyCode key) const noexcept {
            int idx = static_cast<int>(key);
            if (idx < 0 || idx >= static_cast<int>(max_key_code)) return false;
            return m_keys[idx].is_down();
        }

        inline bool is_key_up(KeyCode key) const noexcept {
            int idx = static_cast<int>(key);
            if (idx < 0 || idx >= static_cast<int>(max_key_code)) return true;
            return m_keys[idx].is_up();
        }

        inline bool is_key_just_pressed(KeyCode key) const noexcept {
            int idx = static_cast<int>(key);
            if (idx < 0 || idx >= static_cast<int>(max_key_code)) return false;
            return m_keys[idx].is_just_pressed();
        }

        inline bool is_key_just_released(KeyCode key) const noexcept {
            int idx = static_cast<int>(key);
            if (idx < 0 || idx >= static_cast<int>(max_key_code)) return false;
            return m_keys[idx].is_just_released();
        }

        inline bool has_key_double_tapped(KeyCode key) noexcept {
            int idx = static_cast<int>(key);
            if (idx < 0 || idx >= static_cast<int>(max_key_code)) return false;
            return m_keys[idx].has_double_tapped();
        }

        inline const KeyInfo& get_key_info(KeyCode key) const noexcept {
            int idx = static_cast<int>(key);
            if (idx < 0 || idx >= static_cast<int>(max_key_code)) {
                static const KeyInfo invalid_key_info {};
                return invalid_key_info;
            }
            return m_keys[idx];
        }

        inline KeyInfo& get_key_info(KeyCode key) noexcept {
            int idx = static_cast<int>(key);
            return m_keys[std::clamp(idx, 0, static_cast<int>(max_key_code - 1))];
        }

        inline void set_key_double_tap(KeyCode key, float time_ms) noexcept {
            int idx = static_cast<int>(key);
            if (idx >= 0 && idx < static_cast<int>(max_key_code)) {
                m_keys[idx].double_time_ms = time_ms;
            }
        }

        // ==========================================
        // 鼠标状态查询接口
        // ==========================================

        inline bool is_mouse_button_down(MouseButton button) const noexcept {
            int idx = static_cast<int>(button);
            if (idx < 0 || idx >= static_cast<int>(max_mouse_button)) return false;
            return m_mouse_buttons[idx].is_down();
        }

        inline bool is_mouse_button_up(MouseButton button) const noexcept {
            int idx = static_cast<int>(button);
            if (idx < 0 || idx >= static_cast<int>(max_mouse_button)) return true;
            return m_mouse_buttons[idx].is_up();
        }

        inline bool is_mouse_button_just_pressed(MouseButton button) const noexcept {
            int idx = static_cast<int>(button);
            if (idx < 0 || idx >= static_cast<int>(max_mouse_button)) return false;
            return m_mouse_buttons[idx].is_just_pressed();
        }

        inline bool is_mouse_button_just_released(MouseButton button) const noexcept {
            int idx = static_cast<int>(button);
            if (idx < 0 || idx >= static_cast<int>(max_mouse_button)) return false;
            return m_mouse_buttons[idx].is_just_released();
        }

        inline bool has_mouse_double_clicked(MouseButton button) noexcept {
            int idx = static_cast<int>(button);
            if (idx < 0 || idx >= static_cast<int>(max_mouse_button)) return false;
            return m_mouse_buttons[idx].has_double_tapped();
        }

        inline const KeyInfo& get_mouse_button_info(MouseButton button) const noexcept {
            int idx = static_cast<int>(button);
            if (idx < 0 || idx >= static_cast<int>(max_mouse_button)) {
                static const KeyInfo invalid_mouse_info {};
                return invalid_mouse_info;
            }
            return m_mouse_buttons[idx];
        }

        inline KeyInfo& get_mouse_button_info(MouseButton button) noexcept {
            int idx = static_cast<int>(button);
            return m_mouse_buttons[std::clamp(idx, 0, static_cast<int>(max_mouse_button - 1))];
        }

        inline void set_mouse_double_click(MouseButton button, float time_ms) noexcept {
            int idx = static_cast<int>(button);
            if (idx >= 0 && idx < static_cast<int>(max_mouse_button)) {
                m_mouse_buttons[idx].double_time_ms = time_ms;
            }
        }

        inline std::pair<double, double> get_mouse_position() const noexcept {
            return { m_mouse_x, m_mouse_y };
        }

        inline std::pair<double, double> get_mouse_delta() const noexcept {
            return { m_mouse_delta_x, m_mouse_delta_y };
        }

        inline std::pair<double, double> get_mouse_scroll() const noexcept {
            return { m_scroll_delta_x, m_scroll_delta_y };
        }

        inline bool is_mouse_inside_window() const noexcept {
            return m_mouse_entered;
        }

        // ==========================================
        // 修饰键查询
        // ==========================================

        inline KeyModifier get_modifiers() const noexcept { return m_modifiers; }
        inline bool has_shift() const noexcept { return has_modifier(m_modifiers, KeyModifier::Shift); }
        inline bool has_control() const noexcept { return has_modifier(m_modifiers, KeyModifier::Control); }
        inline bool has_alt() const noexcept { return has_modifier(m_modifiers, KeyModifier::Alt); }
        inline bool has_super() const noexcept { return has_modifier(m_modifiers, KeyModifier::Super); }
        inline bool has_caps_lock() const noexcept { return has_modifier(m_modifiers, KeyModifier::CapsLock); }
        inline bool has_num_lock() const noexcept { return has_modifier(m_modifiers, KeyModifier::NumLock); }

        // ==========================================
        // 虚拟 Action / Axis 映射
        // ==========================================

        void map_action(std::string_view name, KeyCode key) {
            m_actions[std::string(name)].keys.push_back(key);
        }

        void map_action(std::string_view name, MouseButton button) {
            m_actions[std::string(name)].mouse_buttons.push_back(button);
        }

        void map_axis(std::string_view name, KeyCode pos_key, KeyCode neg_key) {
            m_axes[std::string(name)] = AxisBinding {
                .pos_key = pos_key,
                .neg_key = neg_key
            };
        }

        bool is_action_down(std::string_view name) const noexcept {
            auto it = m_actions.find(std::string(name));
            if (it == m_actions.end()) return false;
            for (auto k : it->second.keys) {
                if (is_key_down(k)) return true;
            }
            for (auto b : it->second.mouse_buttons) {
                if (is_mouse_button_down(b)) return true;
            }
            return false;
        }

        bool is_action_just_pressed(std::string_view name) const noexcept {
            auto it = m_actions.find(std::string(name));
            if (it == m_actions.end()) return false;
            for (auto k : it->second.keys) {
                if (is_key_just_pressed(k)) return true;
            }
            for (auto b : it->second.mouse_buttons) {
                if (is_mouse_button_just_pressed(b)) return true;
            }
            return false;
        }

        bool is_action_just_released(std::string_view name) const noexcept {
            auto it = m_actions.find(std::string(name));
            if (it == m_actions.end()) return false;
            for (auto k : it->second.keys) {
                if (is_key_just_released(k)) return true;
            }
            for (auto b : it->second.mouse_buttons) {
                if (is_mouse_button_just_released(b)) return true;
            }
            return false;
        }

        float get_axis(std::string_view name) const noexcept {
            auto it = m_axes.find(std::string(name));
            if (it == m_axes.end()) return 0.0f;
            float val = 0.0f;
            if (is_key_down(it->second.pos_key)) val += 1.0f;
            if (is_key_down(it->second.neg_key)) val -= 1.0f;
            return val;
        }

        /// 清除所有当前按下的状态（例如窗口失焦时）
        void reset_state() noexcept {
            for (auto& k : m_keys) {
                if (k.is_down()) {
                    k.status = KeyState::Released;
                }
            }
            for (auto& m : m_mouse_buttons) {
                if (m.is_down()) {
                    m.status = KeyState::Released;
                }
            }
            m_modifiers = KeyModifier::None;
            m_mouse_delta_x = 0.0;
            m_mouse_delta_y = 0.0;
            m_scroll_delta_x = 0.0;
            m_scroll_delta_y = 0.0;
        }
    };

}
