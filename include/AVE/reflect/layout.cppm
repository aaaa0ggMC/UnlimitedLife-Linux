/**
 * @file layout.cppm
 * @brief GLSL std140 (UBO) and std430 (SSBO) compile-time reflection and layout verification
 * @version 6.0
 * @date 2026-09-13
 */
module;
#include <meta>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <string>
#include <array>
#include <format>
#include <type_traits>
#include <utility>

export module ave.reflect:layout;

import std;
import alib6;

export namespace ave {

    /**
     * @brief 着色器缓冲区内存布局标准
     */
    enum class BufferLayoutStandard : alib6::u8 {
        Std140, ///< Uniform Buffer Object (UBO) 基础对齐规范
        Std430  ///< Shader Storage Buffer Object (SSBO) 扩展对齐规范
    };

    // ========================================================================
    // 基础着色器向量与矩阵类型 (供 UBO/SSBO 结构体选用)
    // ========================================================================

    struct alignas(8) vec2 { float x{0.0f}, y{0.0f}; };
    struct vec3 { float x{0.0f}, y{0.0f}, z{0.0f}; };
    struct alignas(16) vec4 { float x{0.0f}, y{0.0f}, z{0.0f}, w{0.0f}; };

    struct alignas(8) ivec2 { alib6::i32 x{0}, y{0}; };
    struct ivec3 { alib6::i32 x{0}, y{0}, z{0}; };
    struct alignas(16) ivec4 { alib6::i32 x{0}, y{0}, z{0}, w{0}; };

    struct alignas(8) uvec2 { alib6::u32 x{0}, y{0}; };
    struct uvec3 { alib6::u32 x{0}, y{0}, z{0}; };
    struct alignas(16) uvec4 { alib6::u32 x{0}, y{0}, z{0}, w{0}; };

    struct alignas(16) dvec2 { alib6::f64 x{0.0}, y{0.0}; };
    struct dvec3 { alib6::f64 x{0.0}, y{0.0}, z{0.0}; };
    struct alignas(32) dvec4 { alib6::f64 x{0.0}, y{0.0}, z{0.0}, w{0.0}; };

    /// @brief 2x2 矩阵 (列主序)
    struct alignas(8) mat2 { vec2 cols[2]; };
    /// @brief 3x3 矩阵 (列主序，在 std140 下每列按 vec4 步长 16 字节对齐)
    struct alignas(16) mat3 { vec4 cols[3]; };
    /// @brief 4x4 矩阵 (列主序)
    struct alignas(16) mat4 { vec4 cols[4]; };

    template<typename T>
    struct is_vector_trait : std::false_type {};

    template<> struct is_vector_trait<vec2> : std::true_type {};
    template<> struct is_vector_trait<vec3> : std::true_type {};
    template<> struct is_vector_trait<vec4> : std::true_type {};
    template<> struct is_vector_trait<ivec2> : std::true_type {};
    template<> struct is_vector_trait<ivec3> : std::true_type {};
    template<> struct is_vector_trait<ivec4> : std::true_type {};
    template<> struct is_vector_trait<uvec2> : std::true_type {};
    template<> struct is_vector_trait<uvec3> : std::true_type {};
    template<> struct is_vector_trait<uvec4> : std::true_type {};
    template<> struct is_vector_trait<dvec2> : std::true_type {};
    template<> struct is_vector_trait<dvec3> : std::true_type {};
    template<> struct is_vector_trait<dvec4> : std::true_type {};

    // ========================================================================
    // 诊断报告数据结构
    // ========================================================================

    struct MemberLayoutReport {
        std::string_view name;
        std::size_t actual_offset { 0 };
        std::size_t expected_offset { 0 };
        std::size_t actual_size { 0 };
        std::size_t expected_size { 0 };
        std::size_t expected_alignment { 0 };
        bool is_valid { true };
        std::string_view mismatch_reason {};
    };

    template<std::size_t MaxMembers = 32>
    struct LayoutVerificationReport {
        BufferLayoutStandard standard { BufferLayoutStandard::Std140 };
        std::string_view type_name {};
        std::size_t actual_size { 0 };
        std::size_t expected_size { 0 };
        std::size_t actual_alignment { 0 };
        std::size_t expected_alignment { 0 };
        bool size_matches { true };
        bool alignment_matches { true };
        bool members_match { true };

        std::size_t member_count { 0 };
        std::array<MemberLayoutReport, MaxMembers> members {};

        [[nodiscard]] constexpr bool is_valid() const noexcept {
            return size_matches && alignment_matches && members_match;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return is_valid();
        }

        /// @brief 转换为美化表格 (alib6::Table)
        [[nodiscard]] alib6::Table to_table(alib6::TableConfig cfg = alib6::TableConfig::unicode_rounded()) const {
            alib6::Table tbl(cfg);
            tbl.config.col_align = alib6::ColAlign::Left;

            const char* std_name = (standard == BufferLayoutStandard::Std140) ? "std140 (UBO)" : "std430 (SSBO)";
            tbl[0][0] << alib6::log::color(alib6::log::Color::Cyan, alib6::log::Color::None, alib6::log::Style::Bold)
                      << std::format("Buffer Layout [{}]", std_name);
            tbl[0][1] << alib6::log::color(alib6::log::Color::Cyan, alib6::log::Color::None, alib6::log::Style::Bold)
                      << "Type";
            tbl[0][2] << alib6::log::color(alib6::log::Color::Cyan, alib6::log::Color::None, alib6::log::Style::Bold)
                      << "Actual / Expected";
            tbl[0][3] << alib6::log::color(alib6::log::Color::Cyan, alib6::log::Color::None, alib6::log::Style::Bold)
                      << "Status";

            tbl[1][0] << "Total Size";
            tbl[1][1] << type_name;
            tbl[1][2] << std::format("{} B / {} B", actual_size, expected_size);
            if (size_matches) {
                tbl[1][3] << alib6::log::color(alib6::log::Color::Green) << "MATCH";
            } else {
                tbl[1][3] << alib6::log::color(alib6::log::Color::Red, alib6::log::Color::None, alib6::log::Style::Bold) << "MISMATCH";
            }

            tbl[2][0] << "Base Alignment";
            tbl[2][1] << type_name;
            tbl[2][2] << std::format("{} B / {} B", actual_alignment, expected_alignment);
            if (alignment_matches) {
                tbl[2][3] << alib6::log::color(alib6::log::Color::Green) << "MATCH";
            } else {
                tbl[2][3] << alib6::log::color(alib6::log::Color::Red, alib6::log::Color::None, alib6::log::Style::Bold) << "MISMATCH";
            }

            std::size_t row = 3;
            for (std::size_t i = 0; i < member_count && i < MaxMembers; ++i) {
                const auto& m = members[i];
                tbl[row][0] << std::format("  .{}", m.name);
                tbl[row][1] << std::format("align: {}", m.expected_alignment);
                tbl[row][2] << std::format("off: {}/{} | sz: {}/{}", m.actual_offset, m.expected_offset, m.actual_size, m.expected_size);
                if (m.is_valid) {
                    tbl[row][3] << alib6::log::color(alib6::log::Color::Green) << "OK";
                } else {
                    tbl[row][3] << alib6::log::color(alib6::log::Color::Red) << m.mismatch_reason;
                }
                ++row;
            }

            return tbl;
        }

        /// @brief 直接流式写入 alib6::log 管道 (兼容 alib6::log::CanForward 概念)
        void write_to_log(std::pmr::string& target) const {
            const char* std_name = (standard == BufferLayoutStandard::Std140) ? "std140" : "std430";
            if (is_valid()) {
                std::format_to(
                    std::back_inserter(target),
                    "[{}] Type '{}' matches {} layout: size={}B, align={}B, {} members verified.",
                    std_name, type_name, std_name, actual_size, actual_alignment, member_count
                );
            } else {
                std::format_to(
                    std::back_inserter(target),
                    "[{}] LAYOUT MISMATCH for type '{}':\n"
                    "  - Size: actual={}B, expected={}B (match={})\n"
                    "  - Alignment: actual={}B, expected={}B (match={})\n",
                    std_name, type_name, actual_size, expected_size, size_matches,
                    actual_alignment, expected_alignment, alignment_matches
                );
                for (std::size_t i = 0; i < member_count && i < MaxMembers; ++i) {
                    const auto& m = members[i];
                    if (!m.is_valid) {
                        std::format_to(
                            std::back_inserter(target),
                            "  - Member '{}': offset actual={} vs expected={}, size actual={} vs expected={} ({})\n",
                            m.name, m.actual_offset, m.expected_offset, m.actual_size, m.expected_size, m.mismatch_reason
                        );
                    }
                }
            }
        }
    };

} // namespace ave

namespace ave::detail::reflect {

    constexpr std::size_t round_up(std::size_t value, std::size_t alignment) noexcept {
        if (alignment == 0) return value;
        return (value + alignment - 1) & ~(alignment - 1);
    }

    struct TypeLayout {
        std::size_t size{0};
        std::size_t alignment{0};
        std::size_t array_stride{0};
    };

    template<std::meta::info Type>
    consteval bool is_vector_type() noexcept {
        using T = [: std::meta::dealias(Type) :];
        if constexpr (is_vector_trait<T>::value) {
            return true;
        }
        constexpr std::string_view name = std::meta::identifier_of(Type);
        if (name.ends_with("vec2") || name.ends_with("Vec2") ||
            name.ends_with("vec3") || name.ends_with("Vec3") ||
            name.ends_with("vec4") || name.ends_with("Vec4") ||
            name.ends_with("dvec2") || name.ends_with("dvec3") || name.ends_with("dvec4") ||
            name.ends_with("ivec2") || name.ends_with("ivec3") || name.ends_with("ivec4") ||
            name.ends_with("uvec2") || name.ends_with("uvec3") || name.ends_with("uvec4")) {
            return true;
        }
        return false;
    }

    template<BufferLayoutStandard Standard, std::meta::info Type>
    consteval TypeLayout compute_type_layout() noexcept;

    template<BufferLayoutStandard Standard, std::meta::info Type>
    consteval TypeLayout compute_struct_layout() noexcept {
        constexpr auto ctx = std::meta::access_context::unchecked();
        static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(Type, ctx));
        
        std::size_t current_offset = 0;
        std::size_t max_align = 1;

        template for (constexpr auto m : members) {
            constexpr auto m_layout = compute_type_layout<Standard, std::meta::type_of(m)>();
            if (m_layout.alignment > max_align) {
                max_align = m_layout.alignment;
            }
            current_offset = round_up(current_offset, m_layout.alignment);
            current_offset += m_layout.size;
        }

        std::size_t base_alignment = max_align;
        if constexpr (Standard == BufferLayoutStandard::Std140) {
            base_alignment = round_up(base_alignment, 16);
        }
        std::size_t total_size = round_up(current_offset, (Standard == BufferLayoutStandard::Std140) ? 16 : base_alignment);
        
        std::size_t stride = total_size;
        if constexpr (Standard == BufferLayoutStandard::Std140) {
            stride = round_up(stride, 16);
        }

        return TypeLayout{
            .size = total_size,
            .alignment = base_alignment,
            .array_stride = stride
        };
    }

    template<typename T>
    struct is_std_array : std::false_type {};

    template<typename T, std::size_t N>
    struct is_std_array<std::array<T, N>> : std::true_type {
        using element_type = T;
        static constexpr std::size_t extent = N;
    };

    template<BufferLayoutStandard Standard, std::meta::info Type>
    consteval TypeLayout compute_type_layout() noexcept {
        constexpr auto dealiased = std::meta::dealias(Type);
        using T = [: dealiased :];

        // 1. 标量类型 (整型、浮点型、布尔型)
        if constexpr (std::is_arithmetic_v<T>) {
            constexpr std::size_t sz = sizeof(T);
            constexpr std::size_t al = alignof(T);
            constexpr std::size_t stride = (Standard == BufferLayoutStandard::Std140) ? round_up(sz, 16) : sz;
            return TypeLayout{ .size = sz, .alignment = al, .array_stride = stride };
        }

        // 2. 原生数组 T[N]
        else if constexpr (std::is_array_v<T>) {
            using ElemT = std::remove_extent_t<T>;
            constexpr std::size_t elem_count = std::extent_v<T>;
            constexpr auto elem_layout = compute_type_layout<Standard, ^^ElemT>();
            
            std::size_t array_align = elem_layout.alignment;
            std::size_t elem_stride = elem_layout.array_stride;
            if constexpr (Standard == BufferLayoutStandard::Std140) {
                array_align = round_up(array_align, 16);
                elem_stride = round_up(elem_layout.size, 16);
            }

            std::size_t total_size = elem_count * elem_stride;
            return TypeLayout{
                .size = total_size,
                .alignment = array_align,
                .array_stride = total_size
            };
        }

        // 3. std::array<T, N> 容器
        else if constexpr (is_std_array<T>::value) {
            using ElemT = typename is_std_array<T>::element_type;
            constexpr std::size_t elem_count = is_std_array<T>::extent;

            constexpr auto elem_layout = compute_type_layout<Standard, ^^ElemT>();
            std::size_t array_align = elem_layout.alignment;
            std::size_t elem_stride = elem_layout.array_stride;
            if constexpr (Standard == BufferLayoutStandard::Std140) {
                array_align = round_up(array_align, 16);
                elem_stride = round_up(elem_layout.size, 16);
            }
            std::size_t total_size = elem_count * elem_stride;
            return TypeLayout{
                .size = total_size,
                .alignment = array_align,
                .array_stride = total_size
            };
        }

        // 4. 类与结构体
        else if constexpr (std::is_class_v<T>) {
            constexpr auto ctx = std::meta::access_context::unchecked();
            static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(dealiased, ctx));
            
            // 检查是否为向量类型
            if constexpr (is_vector_type<dealiased>()) {
                constexpr auto first_t = std::meta::dealias(std::meta::type_of(members[0]));
                using FirstT = [: first_t :];
                constexpr std::size_t scalar_sz = sizeof(FirstT);
                constexpr std::size_t num_components = members.size();
                std::size_t vec_align = (num_components == 2) ? (2 * scalar_sz) : (4 * scalar_sz);
                std::size_t vec_size = num_components * scalar_sz;
                std::size_t stride = (Standard == BufferLayoutStandard::Std140) ? round_up(vec_align, 16) : vec_align;
                return TypeLayout{
                    .size = vec_size,
                    .alignment = vec_align,
                    .array_stride = stride
                };
            }

            // 普通结构体
            return compute_struct_layout<Standard, dealiased>();
        }

        // 5. 默认兜底
        else {
            constexpr std::size_t sz = sizeof(T);
            constexpr std::size_t al = alignof(T);
            return TypeLayout{ .size = sz, .alignment = al, .array_stride = sz };
        }
    }

} // namespace ave::detail::reflect

export namespace ave {

    /**
     * @brief 对指定类型进行编译期缓冲区内存排布验证 (std140 或 std430)
     */
    template<BufferLayoutStandard Standard, typename T, std::size_t MaxMembers = 32>
    consteval auto verify_buffer_layout() noexcept {
        LayoutVerificationReport<MaxMembers> report;
        report.standard = Standard;
        report.type_name = std::meta::identifier_of(std::meta::dealias(^^T));
        report.actual_size = sizeof(T);
        report.actual_alignment = alignof(T);

        constexpr auto type_info = std::meta::dealias(^^T);
        constexpr auto struct_layout = detail::reflect::compute_type_layout<Standard, type_info>();
        report.expected_size = struct_layout.size;
        report.expected_alignment = struct_layout.alignment;
        report.size_matches = (report.actual_size == report.expected_size);
        report.alignment_matches = (report.actual_alignment >= report.expected_alignment);

        if constexpr (std::is_class_v<T>) {
            constexpr auto ctx = std::meta::access_context::unchecked();
            static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(type_info, ctx));
            report.member_count = members.size();

            std::size_t current_expected_offset = 0;
            std::size_t idx = 0;

            template for (constexpr auto m : members) {
                if (idx < MaxMembers) {
                    constexpr auto m_type = std::meta::type_of(m);
                    constexpr auto m_layout = detail::reflect::compute_type_layout<Standard, m_type>();
                    current_expected_offset = detail::reflect::round_up(current_expected_offset, m_layout.alignment);

                    MemberLayoutReport m_rep;
                    m_rep.name = std::meta::identifier_of(m);
                    m_rep.actual_offset = std::meta::offset_of(m).bytes;
                    m_rep.expected_offset = current_expected_offset;
                    m_rep.actual_size = std::meta::size_of(m_type);
                    m_rep.expected_size = m_layout.size;
                    m_rep.expected_alignment = m_layout.alignment;

                    if (m_rep.actual_offset != m_rep.expected_offset) {
                        m_rep.is_valid = false;
                        m_rep.mismatch_reason = "Offset mismatch with standard alignment";
                        report.members_match = false;
                    } else if (m_rep.actual_size != m_rep.expected_size) {
                        m_rep.is_valid = false;
                        m_rep.mismatch_reason = "Size mismatch with standard stride/padding";
                        report.members_match = false;
                    } else {
                        m_rep.is_valid = true;
                    }

                    report.members[idx++] = m_rep;
                    current_expected_offset += m_layout.size;
                }
            }
        }

        return report;
    }

    /**
     * @brief 验证类型是否满足 GLSL std140 (UBO) 规范
     */
    template<typename T, std::size_t MaxMembers = 32>
    consteval auto verify_std140() noexcept {
        return verify_buffer_layout<BufferLayoutStandard::Std140, T, MaxMembers>();
    }

    /**
     * @brief 验证类型是否满足 GLSL std430 (SSBO) 规范
     */
    template<typename T, std::size_t MaxMembers = 32>
    consteval auto verify_std430() noexcept {
        return verify_buffer_layout<BufferLayoutStandard::Std430, T, MaxMembers>();
    }

    /**
     * @brief 编译期布尔常量：类型是否满足 std140 规范
     */
    template<typename T>
    inline constexpr bool is_std140_compatible_v = verify_std140<T>().is_valid();

    /**
     * @brief 编译期布尔常量：类型是否满足 std430 规范
     */
    template<typename T>
    inline constexpr bool is_std430_compatible_v = verify_std430<T>().is_valid();

    /**
     * @brief C++20/26 Concept: 要求类型与 std140 (UBO) 规范完全兼容
     */
    template<typename T>
    concept Std140Compatible = is_std140_compatible_v<T>;

    /**
     * @brief C++20/26 Concept: 要求类型与 std430 (SSBO) 规范完全兼容
     */
    template<typename T>
    concept Std430Compatible = is_std430_compatible_v<T>;

    /**
     * @brief 编译期断言辅助：若类型不符合 std140，则产生静态编译期错误
     */
    template<typename T>
    consteval void assert_std140() {
        static_assert(is_std140_compatible_v<T>, "Type does not conform to GLSL std140 layout requirements");
    }

    /**
     * @brief 编译期断言辅助：若类型不符合 std430，则产生静态编译期错误
     */
    template<typename T>
    consteval void assert_std430() {
        static_assert(is_std430_compatible_v<T>, "Type does not conform to GLSL std430 layout requirements");
    }

} // namespace ave

export template<std::size_t N>
struct std::formatter<ave::LayoutVerificationReport<N>, char> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto format(const ave::LayoutVerificationReport<N>& report, std::format_context& ctx) const {
        std::pmr::string buf;
        report.write_to_log(buf);
        return std::format_to(ctx.out(), "{}", std::string_view(buf.data(), buf.size()));
    }
};
