#pragma once

#include <AVE/alib.h>

/**
 * @file cursor.cppm
 * @brief 光标模式：类型安全的 Normal / Hidden / Disabled 三态
 * @version 5.0
 * @date 2026-09-29
 * @start-date 2026/09/29
 *
 * 设计说明（从 AGE 移植时的改进）：
 * - AGE 把光标拆成两件事散落在示例里：「空 1x1 光标隐藏」（setCursorVisibility）
 *   与「glfwSetInputMode(GLFW_CURSOR, GLFW_CURSOR_DISABLED) 锁定」（setInputMode），
 *   调用方还需要自己处理切换时的一次性坐标跳变（示例里用 -114514 之类的
 *   sentinel 跳过第一帧）；
 * - AVE 把「隐藏」与「锁定」两种意图收敛为一个 CursorMode 枚举，由 Window 统一托管：
 *   - 初始模式经 ave::CreateWindowInfo::cursor_mode 声明；
 *   - 运行期经 ave::Window::set_cursor_mode / ave::Window::get_cursor_mode 切换；
 *   - 窗口失焦时自动恢复 Normal，避免 alt-tab 后光标被锁死在其它程序外；
 *   - 模式切换时通知绑定的 ave::Input 重置鼠标基准，天然消除跳变，
 *     使用方无需任何 sentinel 技巧；
 *   - Hidden / Disabled 除设置 GLFW 输入模式外，还会挂一张 1x1 全透明光标兜底
 *     （GLFW 在 Wayland 上只把光标面设为 NULL，部分合成器仍会把光标画回来），
 *     并可经 Window::reapply_cursor_mode 在锁定期间持续重设。
 *
 * @code
 * // 典型用法：按 M 锁定/释放鼠标（FPS 相机）
 * if (input.is_key_just_pressed(ave::KeyCode::M)) {
 *     const bool lock = window.get_cursor_mode() != ave::CursorMode::Disabled;
 *     window.set_cursor_mode(lock ? ave::CursorMode::Disabled
 *                                 : ave::CursorMode::Normal);
 * }
 * // 锁定后 get_mouse_delta() 即不越界的原始增量
 * @endcode
 */
#include <AVE/config.h>



namespace ave {

    /**
     * @brief 光标模式
     * @start-date 2026/09/29
     *
     * 与 GLFW 的 GLFW_CURSOR_NORMAL / HIDDEN / DISABLED 一一对应，
     * 但以意图命名：是否需要「隐藏」、是否需要「锁定/原始增量」。
     */
    enum class CursorMode : ave::u8 {
        /// @brief 正常显示；窗口内为绝对像素坐标（默认）
        Normal,
        /// @brief 光标不可见但未锁定；坐标越界后不再更新
        Hidden,
        /// @brief 隐藏并锁定：虚拟无限坐标 + 原始增量，FPS 相机首选
        Disabled,
    };

    /// @brief 获取模式的可读名称（"Normal" / "Hidden" / "Disabled" / "Unknown"）
    [[nodiscard]] inline constexpr std::string_view to_string(CursorMode mode) noexcept;

    /// @brief 是否处于锁定态（虚拟坐标、不越界、持续接收增量）
    [[nodiscard]] inline constexpr bool is_cursor_locked(CursorMode mode) noexcept;

    /// @brief 光标是否可见（仅 Normal 可见）
    [[nodiscard]] inline constexpr bool is_cursor_visible(CursorMode mode) noexcept;

    inline constexpr std::string_view to_string(CursorMode mode) noexcept {
        switch(mode) {
            case CursorMode::Normal:   return "Normal";
            case CursorMode::Hidden:   return "Hidden";
            case CursorMode::Disabled: return "Disabled";
        }
        return "Unknown";
    }

    inline constexpr bool is_cursor_locked(CursorMode mode) noexcept {
        return mode == CursorMode::Disabled;
    }

    inline constexpr bool is_cursor_visible(CursorMode mode) noexcept {
        return mode == CursorMode::Normal;
    }

}

namespace std {
    /// @brief ave::CursorMode 的 std::formatter 特化（转发到 ave::to_string）
    template<>
    struct formatter<ave::CursorMode, char> {
        constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
        auto format(ave::CursorMode mode, format_context& ctx) const {
            return std::formatter<std::string_view, char>{}.format(
                ave::to_string(mode), ctx
            );
        }
    };
}