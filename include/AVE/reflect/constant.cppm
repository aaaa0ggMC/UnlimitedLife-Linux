/**
 * @file constant.cppm
 * @brief Vulkan push constant reflection, layout deduction, and builder
 * @version 6.0
 * @date 2026-09-13
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>
#include <meta>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <string>
#include <vector>
#include <array>
#include <span>
#include <unordered_map>
#include <type_traits>
#include <format>
#include <algorithm>

export module ave.reflect:constant;

import std;
import alib6;
import ave.ecode;
import ave.render.base;
import :layout;

export namespace ave {

    /// @brief 将着色器阶段标志转换为易读的字符串
    inline std::string vk_shader_stages_to_string(VkShaderStageFlags flags) {
        if (flags == VK_SHADER_STAGE_ALL_GRAPHICS) return "ALL_GRAPHICS";
        if (flags == VK_SHADER_STAGE_ALL) return "ALL";
        std::string result;
        auto append_stage = [&](VkShaderStageFlagBits bit, std::string_view name) {
            if (flags & bit) {
                if (!result.empty()) result += " | ";
                result += name;
            }
        };
        append_stage(VK_SHADER_STAGE_VERTEX_BIT, "VERTEX");
        append_stage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "TESSELLATION_CONTROL");
        append_stage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "TESSELLATION_EVALUATION");
        append_stage(VK_SHADER_STAGE_GEOMETRY_BIT, "GEOMETRY");
        append_stage(VK_SHADER_STAGE_FRAGMENT_BIT, "FRAGMENT");
        append_stage(VK_SHADER_STAGE_COMPUTE_BIT, "COMPUTE");
        return result.empty() ? "NONE" : result;
    }

    /// @brief Push Constant 最终构建产物
    struct ConstantLayout {
        std::vector<ConstantAttribute> attributes;
        alib6::u32 total_size { 0 };
        VkShaderStageFlags stage_flags { VK_SHADER_STAGE_ALL_GRAPHICS };

        [[nodiscard]] VkPushConstantRange to_range() const noexcept {
            return VkPushConstantRange{
                .stageFlags = stage_flags,
                .offset = 0,
                .size = total_size
            };
        }

        [[nodiscard]] std::vector<VkPushConstantRange> to_ranges() const {
            if (attributes.empty()) {
                if (total_size > 0) {
                    return { to_range() };
                }
                return {};
            }

            // 检查所有 attributes 的 stage_flags 是否一致
            bool all_same_stage = true;
            for (const auto& a : attributes) {
                if (a.stage_flags != stage_flags) {
                    all_same_stage = false;
                    break;
                }
            }

            if (all_same_stage) {
                return { to_range() };
            }

            // 若包含不同 stage，按 stage 分组聚合为互斥 range
            std::vector<VkPushConstantRange> ranges;
            std::unordered_map<VkShaderStageFlags, std::pair<alib6::u32, alib6::u32>> stage_map;
            for (const auto& a : attributes) {
                auto it = stage_map.find(a.stage_flags);
                if (it == stage_map.end()) {
                    stage_map[a.stage_flags] = { a.offset, a.offset + a.size };
                } else {
                    it->second.first = std::min(it->second.first, a.offset);
                    it->second.second = std::max(it->second.second, a.offset + a.size);
                }
            }
            ranges.reserve(stage_map.size());
            for (const auto& [stg, r] : stage_map) {
                ranges.push_back(VkPushConstantRange{
                    .stageFlags = stg,
                    .offset = r.first,
                    .size = r.second - r.first
                });
            }
            return ranges;
        }

        [[nodiscard]] alib6::Table to_table(alib6::TableConfig cfg = alib6::TableConfig::unicode_rounded()) const {
            alib6::Table tbl(cfg);
            tbl.config.col_align = alib6::ColAlign::Left;

            tbl[0][0] << alib6::log::color(alib6::log::Color::Cyan, alib6::log::Color::None, alib6::log::Style::Bold) << "Offset";
            tbl[0][1] << alib6::log::color(alib6::log::Color::Cyan, alib6::log::Color::None, alib6::log::Style::Bold) << "Size";
            tbl[0][2] << alib6::log::color(alib6::log::Color::Cyan, alib6::log::Color::None, alib6::log::Style::Bold) << "Name / Field";
            tbl[0][3] << alib6::log::color(alib6::log::Color::Cyan, alib6::log::Color::None, alib6::log::Style::Bold) << "Stages";

            std::size_t row = 1;
            for (const auto& a : attributes) {
                tbl[row][0] << std::format("off: {}B", a.offset);
                tbl[row][1] << std::format("{}B", a.size);
                tbl[row][2] << a.name;
                tbl[row][3] << vk_shader_stages_to_string(a.stage_flags);
                ++row;
            }

            return tbl;
        }

        void write_to_log(std::pmr::string& target) const {
            std::format_to(
                std::back_inserter(target),
                "[ConstantLayout] total_size={}B, {} attributes:\n",
                total_size, attributes.size()
            );
            for (const auto& a : attributes) {
                std::format_to(
                    std::back_inserter(target),
                    "  - Attr '{}': offset={}B, size={}B, stages={}\n",
                    a.name, a.offset, a.size, vk_shader_stages_to_string(a.stage_flags)
                );
            }
        }
    };

    // ========================================================================
    // 成员反射工具函数 (Member Pointer Reflection Utilities)
    // ========================================================================

    /// @brief 成员指针类型特征提取
    template<typename T>
    struct member_pointer_traits;

    template<typename ClassT, typename MemberT>
    struct member_pointer_traits<MemberT ClassT::*> {
        using class_type = ClassT;
        using member_type = MemberT;
    };

    /// @brief 常量成员或区间偏移与大小信息
    struct MemberRangeInfo {
        std::size_t offset{0};
        std::size_t size{0};
    };

    /// @brief 编译期静态计算成员指针区间的偏移与大小 (闭区间 [BeginPtr, EndPtr])
    template<auto BeginPtr, auto EndPtr = BeginPtr>
    consteval MemberRangeInfo get_member_range_info() {
        using BeginTraits = member_pointer_traits<decltype(BeginPtr)>;
        using EndTraits = member_pointer_traits<decltype(EndPtr)>;
        static_assert(std::is_same_v<typename BeginTraits::class_type, typename EndTraits::class_type>,
                      "Begin and End member pointers must belong to the same struct type.");
        using T = typename BeginTraits::class_type;

        constexpr auto info = []() consteval -> MemberRangeInfo {
            constexpr auto ctx = std::meta::access_context::unchecked();
            static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx));
            constexpr T dummy{};

            const void* begin_target = static_cast<const void*>(&(dummy.*BeginPtr));
            const void* end_target = static_cast<const void*>(&(dummy.*EndPtr));

            std::size_t begin_offset = 0;
            std::size_t end_offset = 0;
            std::size_t end_size = 0;
            bool found_begin = false;
            bool found_end = false;

            template for (constexpr auto m : members) {
                const void* curr = static_cast<const void*>(&(dummy.[: m :]));
                if (curr == begin_target) {
                    begin_offset = std::meta::offset_of(m).bytes;
                    found_begin = true;
                }
                if (curr == end_target) {
                    end_offset = std::meta::offset_of(m).bytes;
                    using M = [: std::meta::type_of(m) :];
                    end_size = sizeof(M);
                    found_end = true;
                }
            }

            if (!found_begin || !found_end) {
                throw "Member pointer not found in struct";
            }
            if (end_offset < begin_offset) {
                throw "End member must be located at or after Begin member";
            }

            return MemberRangeInfo{
                .offset = begin_offset,
                .size = (end_offset + end_size) - begin_offset
            };
        }();

        static_assert(info.offset % 4 == 0, "Push constant offset must be a multiple of 4 bytes in Vulkan.");
        static_assert(info.size % 4 == 0, "Push constant size must be a multiple of 4 bytes in Vulkan.");

        return info;
    }

    /// @brief 编译期静态计算单个成员指针的偏移与大小
    template<auto MemberPtr>
    consteval MemberRangeInfo get_member_info() {
        return get_member_range_info<MemberPtr, MemberPtr>();
    }

    namespace detail::reflect_constant {

        template<typename T, auto MemberPtr>
        consteval int find_member_index_by_ptr() {
            constexpr auto ctx = std::meta::access_context::unchecked();
            static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx));
            static constexpr T dummy{};
            const void* target = static_cast<const void*>(&(dummy.*MemberPtr));
            int idx = 0;
            template for (constexpr auto m : members) {
                const void* curr = static_cast<const void*>(&(dummy.[: m :]));
                if (curr == target) {
                    return idx;
                }
                idx++;
            }
            return -1;
        }

        template<typename T, std::meta::info Member>
        consteval int find_member_index_by_info() {
            constexpr auto ctx = std::meta::access_context::unchecked();
            static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx));
            int idx = 0;
            template for (constexpr auto m : members) {
                if (m == Member) {
                    return idx;
                }
                idx++;
            }
            return -1;
        }

    } // namespace detail::reflect_constant

    // ========================================================================
    // 常量布局构建器 (ConstantLayoutBuilder<T>)
    // ========================================================================

    template<typename T>
    class ConstantLayoutBuilder {
        static_assert(sizeof(T) % 4 == 0, "Push constant struct size must be a multiple of 4 bytes (Vulkan requirement).");
        static_assert(alignof(T) >= 4, "Push constant struct alignment must be at least 4 bytes (Vulkan requirement).");

    public:
        static constexpr auto get_members() {
            constexpr auto ctx = std::meta::access_context::unchecked();
            return std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx));
        }

        static constexpr std::size_t MemberCount = get_members().size();

        struct AttributeInfo {
            std::string_view name;
            alib6::u32 offset{0};
            alib6::u32 size{0};
            VkShaderStageFlags stage_flags{VK_SHADER_STAGE_ALL_GRAPHICS};
            std::size_t member_index{0};
        };

    private:
        std::array<AttributeInfo, MemberCount> m_attributes{};
        VkShaderStageFlags m_default_stages{VK_SHADER_STAGE_ALL_GRAPHICS};
        alib6::u32 m_base_offset{0};

        consteval void init_attributes() {
            constexpr auto ctx = std::meta::access_context::unchecked();
            static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx));
            std::size_t idx = 0;
            template for (constexpr auto m : members) {
                using M = [: std::meta::type_of(m) :];
                constexpr std::string_view m_name = std::meta::identifier_of(m);
                constexpr std::size_t m_off = std::meta::offset_of(m).bytes;
                constexpr std::size_t m_sz = sizeof(M);

                m_attributes[idx] = AttributeInfo{
                    .name = m_name,
                    .offset = static_cast<alib6::u32>(m_off),
                    .size = static_cast<alib6::u32>(m_sz),
                    .stage_flags = m_default_stages,
                    .member_index = idx
                };
                ++idx;
            }
        }

    public:
        constexpr ConstantLayoutBuilder() noexcept {
            init_attributes();
        }

        constexpr ConstantLayoutBuilder& with_stage(VkShaderStageFlags stages) noexcept {
            m_default_stages = stages;
            for (auto& a : m_attributes) {
                a.stage_flags = stages;
            }
            return *this;
        }

        constexpr ConstantLayoutBuilder& with_offset(alib6::u32 offset) noexcept {
            m_base_offset = offset;
            return *this;
        }

        /// @brief 通过成员指针覆盖 stage_flags (例如 .with_stage<&Block::model>(VK_SHADER_STAGE_VERTEX_BIT))
        template<auto MemberPtr>
        constexpr ConstantLayoutBuilder& with_stage(VkShaderStageFlags stages) {
            constexpr int idx = detail::reflect_constant::find_member_index_by_ptr<T, MemberPtr>();
            static_assert(idx >= 0, "MemberPtr is not a non-static data member of struct T");
            m_attributes[idx].stage_flags = stages;
            return *this;
        }

        /// @brief 通过反射表达式覆盖 stage_flags (例如 .with_stage<^^Block::model>(VK_SHADER_STAGE_VERTEX_BIT))
        template<std::meta::info Member>
        constexpr ConstantLayoutBuilder& with_stage(VkShaderStageFlags stages) {
            constexpr int idx = detail::reflect_constant::find_member_index_by_info<T, Member>();
            static_assert(idx >= 0, "Member is not a non-static data member of struct T");
            m_attributes[idx].stage_flags = stages;
            return *this;
        }

        /// @brief 通过字段名称覆盖 stage_flags
        constexpr ConstantLayoutBuilder& with_stage(std::string_view member_name, VkShaderStageFlags stages) {
            for (auto& a : m_attributes) {
                if (a.name == member_name) {
                    a.stage_flags = stages;
                }
            }
            return *this;
        }

        [[nodiscard]] ConstantLayout build(alib6::ErrorWrapper ew = {}) const {
            ConstantLayout layout;
            layout.total_size = static_cast<alib6::u32>(sizeof(T));
            layout.stage_flags = m_default_stages;
            layout.attributes.reserve(MemberCount);

            for (const auto& a : m_attributes) {
                layout.attributes.push_back(ConstantAttribute{
                    .name = std::string(a.name),
                    .offset = a.offset + m_base_offset,
                    .size = a.size,
                    .stage_flags = a.stage_flags
                });
            }
            return layout;
        }

        [[nodiscard]] std::vector<ConstantAttribute> build_attributes(alib6::ErrorWrapper ew = {}) const {
            return build(ew).attributes;
        }

        [[nodiscard]] VkPushConstantRange to_range() const noexcept {
            return VkPushConstantRange{
                .stageFlags = m_default_stages,
                .offset = m_base_offset,
                .size = static_cast<uint32_t>(sizeof(T))
            };
        }
    };

    // ========================================================================
    // 积木拼接运算符 (operator+)
    // ========================================================================

    template<typename T1, typename T2>
    inline std::vector<ConstantAttribute> operator+(
        const ConstantLayoutBuilder<T1>& a,
        const ConstantLayoutBuilder<T2>& b
    ) {
        auto la = a.build();
        auto lb = b.build();
        std::vector<ConstantAttribute> result = std::move(la.attributes);
        result.insert(result.end(), lb.attributes.begin(), lb.attributes.end());
        return result;
    }

    // ========================================================================
    // 工厂入口函数 (ave::constant_layout<T>())
    // ========================================================================

    /// @brief 创建 Push Constant 常量布局建造者
    template<typename T>
    [[nodiscard]] constexpr auto constant_layout() noexcept {
        return ConstantLayoutBuilder<T>{};
    }

} // namespace ave
