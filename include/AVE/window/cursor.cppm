/**
 * @file cursor.cppm
 * @brief 光标模式：类型安全的 Normal / Hidden / Disabled 三态
 * @version 5.0
 * @date 2026-09-29
 *
 * 设计说明（相对 AGE 的改进）：
 * - AGE 用「空 1x1 光标 + glfwSetInputMode(GLFW_CURSOR_DISABLED)」散落在示例中，
 *   本模块把「隐藏」与「锁定」两种意图收敛为一个 CursorMode 枚举，Window 统一托管；
 * - 初始模式经 CreateWindowInfo.cursor_mode 声明，运行期经 Window::set_cursor_mode 切换；
 * - 失焦自动释放（避免 alt-tab 后光标被锁死在其它程序之外）；
 * - 切换时通知绑定的 Input 重置鼠标基准，消除 GLFW 模式切换带来的一次性坐标跳变。
 */
module;
#include <AVE/config.h>

export module ave.window:cursor;

import std;
import alib6;

export namespace ave {

    enum class CursorMode : alib6::u8 {
        /// 正常显示；窗口内为绝对像素坐标（默认）
        Normal,
        /// 光标不可见但未锁定；坐标越界后不再更新
        Hidden,
        /// 隐藏并锁定：虚拟无限坐标 + 原始增量，FPS 相机首选
        Disabled,
    };

    [[nodiscard]] inline constexpr std::string_view to_string(CursorMode mode) noexcept {
        switch(mode) {
            case CursorMode::Normal:   return "Normal";
            case CursorMode::Hidden:   return "Hidden";
            case CursorMode::Disabled: return "Disabled";
        }
        return "Unknown";
    }

    /// 是否锁定（虚拟坐标、不越界、持续接收增量）
    [[nodiscard]] inline constexpr bool is_cursor_locked(CursorMode mode) noexcept {
        return mode == CursorMode::Disabled;
    }

    /// 是否可见
    [[nodiscard]] inline constexpr bool is_cursor_visible(CursorMode mode) noexcept {
        return mode == CursorMode::Normal;
    }

}

export namespace std {
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
