/**
 * @file table.h
 * @brief AVE 诊断用表格与彩色输出：薄转发到 alib5 表格引擎（alib6.table 回移版）。
 *
 * 迁移去 modules 时，alib6 的表格子系统一度没有回移，这里曾自造一套极简 `ave::Table`
 * （圆角框 + ASCII 横线 + 头字节计数宽度），导致日志表格从 alib6 的圆角 Unicode 边框退化。
 * 现在 alib6::table 已按 alib6 语义回移为 `alib5::table`（include/alib5/atable.h），
 * 本文件只做命名转发，不再维护第二套排版实现：
 *  - `ave::Table` / `ave::TableConfig` / `ave::ColAlign` / `ave::RowAlign`
 *    直接别名到 `alib5::table::*`；
 *  - `ave::log::color` / `ave::log::Color` / `ave::log::Style` 转发到 `alib5::lot::*`；
 *  - `std::formatter<ave::Table>` 不再特化：`ave::Table` 与 `alib5::table::Table` 是同一类型，
 *    alib5 已提供 `std::formatter<alib5::table::Table>`（重复特化同一类型是非法的）。
 *
 * 因此 reflect/* 与 misc/fps_detective.h 的既有调用点（tbl[r][c] << ... << color(...)）
 * 无需改写逻辑，即恢复迁移前的圆角 Unicode 边框外观。
 */
#pragma once

#include <alib5/atable.h>

#include <string>

namespace ave {

    // ==================== 表格类型（alib5 表格引擎，alib6.table 回移版） ====================

    /// @brief 表格排版器（等价 alib6::Table）
    using Table = alib5::table::Table;
    /// @brief 表格边框与对齐配置（七个预设：unicode_rounded/unicode_box/double_line/modern_dot/markdown/minimal/ascii_classic）
    using TableConfig = alib5::table::TableConfig;
    /// @brief 列水平对齐模式
    using ColAlign = alib5::table::ColAlign;
    /// @brief 行垂直对齐模式
    using RowAlign = alib5::table::RowAlign;

    // ==================== 彩色 manipulator（转发 alib5::lot） ====================

    namespace log {

        /// @brief ANSI 颜色枚举（与 alib6::log::Color 同名同义）
        using Color = alib5::lot::Color;
        /// @brief 样式位掩码（与 alib6::log::Style 同名同义）
        using Style = alib5::lot::Style;
        /// @brief 兼容原 alib6::log::CanForward 概念（alib5 同名概念）
        template<class T>
        concept CanForward = alib5::CanForward<T>;

        /// @brief 彩色 manipulator：流式写入表格单元格时注入颜色标签
        [[nodiscard]] inline constexpr alib5::log_tag color(
            Color fg,
            Color bg = Color::None,
            Style st = Style::None,
            uint16_t cate_id = 0
        ) {
            return alib5::lot::color(fg, bg, st, cate_id);
        }

    } // namespace log

    /// @brief 便捷字符串化（保留迁移前的 ave::to_string 入口）
    [[nodiscard]] inline std::string to_string(const Table& t) {
        auto s = t.str();
        return std::string(s.data(), s.size());
    }

} // namespace ave
