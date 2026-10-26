/**
 * @file table.h
 * @brief AVE 诊断用表格与彩色输出（去 modules 迁移，最小自包含实现）。
 *
 * 原 reflect/* 与 misc/fps_detective 的 `to_table()` 调试可视化依赖 alib6::Table
 * 与 alib6::log::color。alib5 无等价 API；为「保留表格可视化」，在 ave 命名空间内
 * 提供最小的 tbl[r][c] << ... 渲染器与 ANSI 彩色 manipulator（基于 alib5 ACP_* 思路）。
 * 仅用于诊断打印，不参与核心反射/布局校验结果；文字排版为近似还原。
 */
#pragma once

#include <AVE/error_code.h>

#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <optional>
#include <cstddef>

namespace ave {
namespace log {

    enum class Color { None, Red, Green, Cyan, Yellow, Magenta, Gray };
    enum class Style { None, Bold };

    namespace detail {
        inline const char* fg_code(Color c) {
            switch(c) {
                case Color::Red:     return "\e[31m";
                case Color::Green:   return "\e[32m";
                case Color::Cyan:    return "\e[36m";
                case Color::Yellow:  return "\e[33m";
                case Color::Magenta: return "\e[35m";
                case Color::Gray:    return "\e[90m";
                case Color::None:    break;
            }
            return "";
        }
        inline const char* bg_code(Color c) {
            switch(c) {
                case Color::Red:     return "\e[41m";
                case Color::Green:   return "\e[42m";
                case Color::Cyan:    return "\e[46m";
                case Color::Yellow:  return "\e[43m";
                case Color::Magenta: return "\e[45m";
                case Color::Gray:    return "\e[100m";
                case Color::None:    break;
            }
            return "";
        }
        inline const char* style_code(Style s) {
            return s == Style::Bold ? "\e[1m" : "";
        }
        // 计算去除 ANSI 转义后的可见宽度。
        inline usize visible_width(std::string_view s) {
            usize w = 0;
            for(usize i = 0; i < s.size();) {
                if(s[i] == '\e') {
                    usize end = s.find('m', i);
                    i = (end == std::string_view::npos) ? s.size() : end + 1;
                } else {
                    // 以 UTF-8 首字节计数（近似列宽，等宽终端下正确）。
                    if((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) ++w;
                    ++i;
                }
            }
            return w;
        }
        inline std::string pad_to(std::string_view s, usize width) {
            usize vis = visible_width(s);
            std::string out(s);
            if(width > vis) out.append(width - vis, ' ');
            return out;
        }
    }

    /// 彩色 manipulator：流式写入时输出 ANSI 起始序列。
    struct ColorManip {
        std::string codes;
    };

    inline ColorManip color(Color fg, Color bg = Color::None, Style st = Style::None) {
        std::string c;
        c += detail::fg_code(fg);
        c += detail::bg_code(bg);
        c += detail::style_code(st);
        return ColorManip{ std::move(c) };
    }

    /// 兼容原 alib6::log::CanForward 概念（供 write_to_log 友元声明约束）。
    template<class T>
    concept CanForward = requires(T& t, std::pmr::string& s) { t.write_to_log(s); };

} // namespace log

    enum class ColAlign { Left, Right, Center };

    struct TableConfig {
        ColAlign col_align { ColAlign::Left };
        static TableConfig unicode_rounded() { return TableConfig{}; }
    };

    /// 最小表格：tbl[row][col] << value / << log::color(...)，渲染为圆角框。
    class Table {
    public:
        std::vector<std::vector<std::string>> cells;
        std::vector<std::vector<bool>> colored;
        TableConfig config;

        explicit Table(TableConfig cfg = TableConfig::unicode_rounded())
            : config(cfg) {}

        struct Cell {
            Table* t;
            usize r, c;
            template<class T>
            Cell& operator<<(const T& v) {
                std::format_to(std::back_inserter(t->at(r, c)), "{}", v);
                return *this;
            }
            Cell& operator<<(const log::ColorManip& m) {
                t->at(r, c) += m.codes;
                t->mark_colored(r, c);
                return *this;
            }
        };
        struct Row {
            Table* t;
            usize r;
            Cell operator[](usize c) { return Cell{ t, r, c }; }
        };
        Row operator[](usize r) { return Row{ this, r }; }

        std::string& at(usize r, usize c) {
            if(cells.size() <= r) { cells.resize(r + 1); colored.resize(r + 1); }
            if(cells[r].size() <= c) { cells[r].resize(c + 1); colored[r].resize(c + 1, false); }
            return cells[r][c];
        }
        void mark_colored(usize r, usize c) {
            if(colored.size() <= r) colored.resize(r + 1);
            if(colored[r].size() <= c) colored[r].resize(c + 1, false);
            colored[r][c] = true;
        }

        [[nodiscard]] std::string str() const {
            usize rows = cells.size();
            usize cols = 0;
            for(const auto& row : cells) cols = std::max<std::size_t>(cols, row.size());
            if(cols == 0) cols = 1;

            std::vector<usize> width(cols, 0);
            for(const auto& row : cells) {
                for(usize c = 0; c < row.size(); ++c)
                    width[c] = std::max<std::size_t>(width[c], log::detail::visible_width(row[c]));
            }

            std::string out;
            auto hline = [&](const char* l, const char* mid, const char* r) {
                out += l;
                for(usize c = 0; c < cols; ++c) {
                    out.append(width[c] + 2, '-');
                    out += (c + 1 == cols) ? r : mid;
                }
                out += '\n';
            };
            hline("\u256d", "\u252c", "\u256e"); // ╭ ┬ ╮
            for(usize r = 0; r < rows; ++r) {
                out += "\u2502"; // │
                for(usize c = 0; c < cols; ++c) {
                    std::string_view content = (c < cells[r].size()) ? std::string_view(cells[r][c]) : std::string_view{};
                    usize vis = log::detail::visible_width(content);
                    usize pad = (width[c] > vis) ? width[c] - vis : 0;
                    std::string left, right;
                    if(config.col_align == ColAlign::Right) left.assign(pad, ' ');
                    else if(config.col_align == ColAlign::Center) { left.assign(pad / 2, ' '); right.assign(pad - pad / 2, ' '); }
                    else right.assign(pad, ' ');
                    out += ' ';
                    out += left;
                    out += content;
                    if((c < colored[r].size()) && colored[r][c]) out += "\e[0m";
                    out += right;
                    out += ' ';
                    out += "\u2502";
                }
                out += '\n';
                if(r + 1 == rows) hline("\u2570", "\u2534", "\u256f"); // ╰ ┴ ╯
                else hline("\u251c", "\u253c", "\u2524");              // ├ ┼ ┤
            }
            return out;
        }
    };

    inline std::string to_string(const Table& t) { return t.str(); }

} // namespace ave

// 让 ave::Table 可被 alib5 日志流（基于 std::formatter）与 std::format 输出。
template<>
struct std::formatter<ave::Table> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    template<class FormatContext>
    auto format(const ave::Table& t, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}", t.str());
    }
};
