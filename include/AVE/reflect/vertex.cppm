/**
 * @file vertex.cppm
 * @brief Vulkan vertex input reflection, format deduction, and layout builder with alib6 MonoBitSet slot allocation
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
#include <type_traits>
#include <format>
#include <algorithm>

export module ave.reflect:vertex;

import std;
import alib6;
import ave.ecode;
import ave.render.base;
import :layout;

export namespace ave {

    // ========================================================================
    // 归一化紧凑顶点类型 (Normalized Attribute Types)
    // ========================================================================

    /// @brief 8位无符号归一化向量 (0 ~ 255 -> 0.0 ~ 1.0)
    struct unorm8x4 { alib6::u8 r{0}, g{0}, b{0}, a{0}; };
    /// @brief 8位有符号归一化向量 (-128 ~ 127 -> -1.0 ~ 1.0)
    struct snorm8x4 { alib6::i8 r{0}, g{0}, b{0}, a{0}; };

    /// @brief 16位无符号归一化双分量向量
    struct unorm16x2 { alib6::u16 x{0}, y{0}; };
    /// @brief 16位无符号归一化四分量向量
    struct unorm16x4 { alib6::u16 x{0}, y{0}, z{0}, w{0}; };

    /// @brief 16位有符号归一化双分量向量
    struct snorm16x2 { alib6::i16 x{0}, y{0}; };
    /// @brief 16位有符号归一化四分量向量
    struct snorm16x4 { alib6::i16 x{0}, y{0}, z{0}, w{0}; };

    // ========================================================================
    // 类型到 VkFormat 的映射特征 (Vertex Format Traits)
    // ========================================================================

    template<typename T>
    struct vertex_format_trait;

    // 浮点与浮点向量
    template<> struct vertex_format_trait<float> {
        static constexpr VkFormat format = VK_FORMAT_R32_SFLOAT;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(float);
    };
    template<> struct vertex_format_trait<vec2> {
        static constexpr VkFormat format = VK_FORMAT_R32G32_SFLOAT;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(vec2);
    };
    template<> struct vertex_format_trait<vec3> {
        static constexpr VkFormat format = VK_FORMAT_R32G32B32_SFLOAT;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(vec3);
    };
    template<> struct vertex_format_trait<vec4> {
        static constexpr VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(vec4);
    };

    // 双精度浮点
    template<> struct vertex_format_trait<alib6::f64> {
        static constexpr VkFormat format = VK_FORMAT_R64_SFLOAT;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(alib6::f64);
    };
    template<> struct vertex_format_trait<dvec2> {
        static constexpr VkFormat format = VK_FORMAT_R64G64_SFLOAT;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(dvec2);
    };
    template<> struct vertex_format_trait<dvec3> {
        static constexpr VkFormat format = VK_FORMAT_R64G64B64_SFLOAT;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(dvec3);
    };
    template<> struct vertex_format_trait<dvec4> {
        static constexpr VkFormat format = VK_FORMAT_R64G64B64A64_SFLOAT;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(dvec4);
    };

    // 有符号整型
    template<> struct vertex_format_trait<alib6::i32> {
        static constexpr VkFormat format = VK_FORMAT_R32_SINT;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(alib6::i32);
    };
    template<> struct vertex_format_trait<ivec2> {
        static constexpr VkFormat format = VK_FORMAT_R32G32_SINT;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(ivec2);
    };
    template<> struct vertex_format_trait<ivec3> {
        static constexpr VkFormat format = VK_FORMAT_R32G32B32_SINT;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(ivec3);
    };
    template<> struct vertex_format_trait<ivec4> {
        static constexpr VkFormat format = VK_FORMAT_R32G32B32A32_SINT;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(ivec4);
    };

    // 无符号整型
    template<> struct vertex_format_trait<alib6::u32> {
        static constexpr VkFormat format = VK_FORMAT_R32_UINT;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(alib6::u32);
    };
    template<> struct vertex_format_trait<uvec2> {
        static constexpr VkFormat format = VK_FORMAT_R32G32_UINT;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(uvec2);
    };
    template<> struct vertex_format_trait<uvec3> {
        static constexpr VkFormat format = VK_FORMAT_R32G32B32_UINT;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(uvec3);
    };
    template<> struct vertex_format_trait<uvec4> {
        static constexpr VkFormat format = VK_FORMAT_R32G32B32A32_UINT;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(uvec4);
    };

    // 归一化类型
    template<> struct vertex_format_trait<unorm8x4> {
        static constexpr VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(unorm8x4);
    };
    template<> struct vertex_format_trait<snorm8x4> {
        static constexpr VkFormat format = VK_FORMAT_R8G8B8A8_SNORM;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(snorm8x4);
    };
    template<> struct vertex_format_trait<unorm16x2> {
        static constexpr VkFormat format = VK_FORMAT_R16G16_UNORM;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(unorm16x2);
    };
    template<> struct vertex_format_trait<unorm16x4> {
        static constexpr VkFormat format = VK_FORMAT_R16G16B16A16_UNORM;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(unorm16x4);
    };
    template<> struct vertex_format_trait<snorm16x2> {
        static constexpr VkFormat format = VK_FORMAT_R16G16_SNORM;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(snorm16x2);
    };
    template<> struct vertex_format_trait<snorm16x4> {
        static constexpr VkFormat format = VK_FORMAT_R16G16B16A16_SNORM;
        static constexpr uint32_t slots = 1;
        static constexpr uint32_t slot_stride = sizeof(snorm16x4);
    };

    // 矩阵类型 (每个列占用 1 个 location 插槽)
    template<> struct vertex_format_trait<mat2> {
        static constexpr VkFormat format = VK_FORMAT_R32G32_SFLOAT;
        static constexpr uint32_t slots = 2;
        static constexpr uint32_t slot_stride = sizeof(vec2);
    };
    template<> struct vertex_format_trait<mat3> {
        static constexpr VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
        static constexpr uint32_t slots = 3;
        static constexpr uint32_t slot_stride = sizeof(vec4);
    };
    template<> struct vertex_format_trait<mat4> {
        static constexpr VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
        static constexpr uint32_t slots = 4;
        static constexpr uint32_t slot_stride = sizeof(vec4);
    };

    // C原生数组与std::array支持
    template<> struct vertex_format_trait<float[2]> : vertex_format_trait<vec2> {};
    template<> struct vertex_format_trait<float[3]> : vertex_format_trait<vec3> {};
    template<> struct vertex_format_trait<float[4]> : vertex_format_trait<vec4> {};
    template<> struct vertex_format_trait<std::array<float, 2>> : vertex_format_trait<vec2> {};
    template<> struct vertex_format_trait<std::array<float, 3>> : vertex_format_trait<vec3> {};
    template<> struct vertex_format_trait<std::array<float, 4>> : vertex_format_trait<vec4> {};

    /// @brief 将 VkFormat 转换为易读的字符串名称
    inline constexpr std::string_view vk_format_to_string(VkFormat format) noexcept {
        switch (format) {
            case VK_FORMAT_R32_SFLOAT: return "R32_SFLOAT (float)";
            case VK_FORMAT_R32G32_SFLOAT: return "R32G32_SFLOAT (vec2)";
            case VK_FORMAT_R32G32B32_SFLOAT: return "R32G32B32_SFLOAT (vec3)";
            case VK_FORMAT_R32G32B32A32_SFLOAT: return "R32G32B32A32_SFLOAT (vec4)";
            case VK_FORMAT_R64_SFLOAT: return "R64_SFLOAT (double)";
            case VK_FORMAT_R64G64_SFLOAT: return "R64G64_SFLOAT (dvec2)";
            case VK_FORMAT_R64G64B64_SFLOAT: return "R64G64B64_SFLOAT (dvec3)";
            case VK_FORMAT_R64G64B64A64_SFLOAT: return "R64G64B64A64_SFLOAT (dvec4)";
            case VK_FORMAT_R32_SINT: return "R32_SINT (int32)";
            case VK_FORMAT_R32G32_SINT: return "R32G32_SINT (ivec2)";
            case VK_FORMAT_R32G32B32_SINT: return "R32G32B32_SINT (ivec3)";
            case VK_FORMAT_R32G32B32A32_SINT: return "R32G32B32A32_SINT (ivec4)";
            case VK_FORMAT_R32_UINT: return "R32_UINT (uint32)";
            case VK_FORMAT_R32G32_UINT: return "R32G32_UINT (uvec2)";
            case VK_FORMAT_R32G32B32_UINT: return "R32G32B32_UINT (uvec3)";
            case VK_FORMAT_R32G32B32A32_UINT: return "R32G32B32A32_UINT (uvec4)";
            case VK_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM (unorm8x4)";
            case VK_FORMAT_R8G8B8A8_SNORM: return "R8G8B8A8_SNORM (snorm8x4)";
            case VK_FORMAT_R16G16_UNORM: return "R16G16_UNORM (unorm16x2)";
            case VK_FORMAT_R16G16B16A16_UNORM: return "R16G16B16A16_UNORM (unorm16x4)";
            case VK_FORMAT_R16G16_SNORM: return "R16G16_SNORM (snorm16x2)";
            case VK_FORMAT_R16G16B16A16_SNORM: return "R16G16B16A16_SNORM (snorm16x4)";
            default: return "UNKNOWN";
        }
    }

    // ========================================================================
    // 最终构建产物 (VertexInputState)
    // ========================================================================

    struct VertexInputState {
        std::vector<VertexBinding> bindings;
        std::vector<VertexAttribute> attributes;
        std::vector<std::string> attribute_names;

        [[nodiscard]] VkPipelineVertexInputStateCreateInfo to_create_info() const noexcept {
            static thread_local std::vector<VkVertexInputBindingDescription> vk_b;
            static thread_local std::vector<VkVertexInputAttributeDescription> vk_a;
            vk_b.clear();
            vk_a.clear();
            vk_b.reserve(bindings.size());
            vk_a.reserve(attributes.size());
            for (const auto& b : bindings) vk_b.push_back(b);
            for (const auto& a : attributes) vk_a.push_back(a);

            return VkPipelineVertexInputStateCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .vertexBindingDescriptionCount = static_cast<uint32_t>(vk_b.size()),
                .pVertexBindingDescriptions = vk_b.empty() ? nullptr : vk_b.data(),
                .vertexAttributeDescriptionCount = static_cast<uint32_t>(vk_a.size()),
                .pVertexAttributeDescriptions = vk_a.empty() ? nullptr : vk_a.data(),
            };
        }

        [[nodiscard]] alib6::Table to_table(alib6::TableConfig cfg = alib6::TableConfig::unicode_rounded()) const {
            alib6::Table tbl(cfg);
            tbl.config.col_align = alib6::ColAlign::Left;

            tbl[0][0] << alib6::log::color(alib6::log::Color::Cyan, alib6::log::Color::None, alib6::log::Style::Bold) << "Location";
            tbl[0][1] << alib6::log::color(alib6::log::Color::Cyan, alib6::log::Color::None, alib6::log::Style::Bold) << "Binding";
            tbl[0][2] << alib6::log::color(alib6::log::Color::Cyan, alib6::log::Color::None, alib6::log::Style::Bold) << "Name / Field";
            tbl[0][3] << alib6::log::color(alib6::log::Color::Cyan, alib6::log::Color::None, alib6::log::Style::Bold) << "Format";
            tbl[0][4] << alib6::log::color(alib6::log::Color::Cyan, alib6::log::Color::None, alib6::log::Style::Bold) << "Offset";

            std::size_t row = 1;
            for (std::size_t i = 0; i < attributes.size(); ++i) {
                const auto& a = attributes[i];
                std::string_view name = (i < attribute_names.size()) ? attribute_names[i] : "-";

                tbl[row][0] << std::format("loc {}", a.location);
                tbl[row][1] << std::format("binding {}", a.binding);
                tbl[row][2] << name;
                tbl[row][3] << vk_format_to_string(a.format);
                tbl[row][4] << std::format("off: {}B", a.offset);
                ++row;
            }

            return tbl;
        }

        void write_to_log(std::pmr::string& target) const {
            std::format_to(
                std::back_inserter(target),
                "[VertexInputState] {} bindings, {} attributes:\n",
                bindings.size(), attributes.size()
            );
            for (const auto& b : bindings) {
                std::format_to(
                    std::back_inserter(target),
                    "  - Binding {}: stride={}B, rate={}\n",
                    b.binding, b.stride,
                    (b.input_rate == VK_VERTEX_INPUT_RATE_VERTEX ? "VERTEX" : "INSTANCE")
                );
            }
            for (std::size_t i = 0; i < attributes.size(); ++i) {
                const auto& a = attributes[i];
                std::string_view name = (i < attribute_names.size()) ? attribute_names[i] : "-";
                std::format_to(
                    std::back_inserter(target),
                    "  - Attr [loc {}, bind {}] '{}': format={}, offset={}B\n",
                    a.location, a.binding, name, vk_format_to_string(a.format), a.offset
                );
            }
        }
    };

    // ========================================================================
    // 反射辅助函数 (Member Matching & Attribute Counting)
    // ========================================================================

    namespace detail::reflect {

        template<typename T>
        consteval std::size_t count_vertex_attributes() {
            constexpr auto ctx = std::meta::access_context::unchecked();
            static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx));
            std::size_t total = 0;
            template for (constexpr auto m : members) {
                using M = [: std::meta::type_of(m) :];
                total += vertex_format_trait<M>::slots;
            }
            return total;
        }

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

    } // namespace detail::reflect

    template<typename T>
    class VertexLayoutBuilder;

    // ========================================================================
    // 布局构建器复合容器 (VertexLayoutComposite)
    // ========================================================================

    class VertexLayoutComposite {
    public:
        struct CompositeAttr {
            std::string name;
            uint32_t location{0};
            uint32_t binding{0};
            VkFormat format{VK_FORMAT_UNDEFINED};
            uint32_t offset{0};
            bool has_explicit_location{false};
        };

    private:
        std::vector<VertexBinding> m_bindings;
        std::vector<CompositeAttr> m_attributes;

    public:
        VertexLayoutComposite() = default;

        template<typename T>
        void add(const VertexLayoutBuilder<T>& builder);

        [[nodiscard]] uint32_t next_available_binding() const noexcept {
            uint32_t max_b = 0;
            for (const auto& b : m_bindings) {
                if (b.binding >= max_b) {
                    max_b = b.binding + 1;
                }
            }
            return max_b;
        }

        [[nodiscard]] VertexInputState build(alib6::ErrorWrapper ew = {}) const {
            // 使用 alib6::storage::MonoBitSet 进行插槽冲突检测与空洞智能填充
            alib6::storage::MonoBitSet occupied(64);
            auto attrs = m_attributes;

            // 第一阶段：注册并校验显式指定的 location
            for (const auto& a : attrs) {
                if (a.has_explicit_location) {
                    if (a.location >= 64) {
                        ew.report(ave_invalid_vertex_layout,
                            "Vertex attribute '{}' location {} exceeds maximum supported (64)",
                            a.name, a.location);
                        continue;
                    }
                    if (occupied.test(a.location)) {
                        ew.report(ave_invalid_vertex_layout,
                            "Vertex attribute location collision: location {} is already occupied (conflict on '{}')",
                            a.location, a.name);
                    } else {
                        occupied.set(a.location);
                    }
                }
            }

            // 第二阶段：未显式指定的字段，通过 MonoBitSet::find_next_0 寻找最近空闲槽位
            alib6::usize search_pos = 0;
            for (auto& a : attrs) {
                if (!a.has_explicit_location) {
                    auto free_slot = occupied.find_next_0(search_pos, 64);
                    if (!free_slot) {
                        ew.report(ave_invalid_vertex_layout,
                            "Exhausted available vertex input locations for attribute '{}' (limit 64)", a.name);
                        continue;
                    }
                    a.location = static_cast<uint32_t>(*free_slot);
                    occupied.set(*free_slot);
                    search_pos = *free_slot + 1;
                }
            }

            VertexInputState state;
            state.bindings = m_bindings;
            state.attributes.reserve(attrs.size());
            state.attribute_names.reserve(attrs.size());

            for (const auto& a : attrs) {
                state.attributes.push_back(VertexAttribute{
                    .location = a.location,
                    .binding = a.binding,
                    .format = a.format,
                    .offset = a.offset
                });
                state.attribute_names.push_back(a.name);
            }

            return state;
        }

        [[nodiscard]] std::vector<VertexAttribute> build_attributes(alib6::ErrorWrapper ew = {}) const {
            return build(ew).attributes;
        }

        [[nodiscard]] std::vector<VertexBinding> build_bindings(alib6::ErrorWrapper ew = {}) const {
            return build(ew).bindings;
        }

        [[nodiscard]] VkPipelineVertexInputStateCreateInfo to_create_info(alib6::ErrorWrapper ew = {}) const noexcept {
            // Note: Use state.to_create_info() from an allocated VertexInputState object for safe lifetime
            static thread_local VertexInputState tls_state;
            tls_state = build(ew);
            return tls_state.to_create_info();
        }
    };

    // ========================================================================
    // 顶点布局构建器 (VertexLayoutBuilder<T>)
    // ========================================================================

    template<typename T>
    class VertexLayoutBuilder {
    public:
        static constexpr std::size_t AttrCount = detail::reflect::count_vertex_attributes<T>();

        struct AttributeInfo {
            std::string_view name;
            uint32_t location{0};
            uint32_t binding{0};
            VkFormat format{VK_FORMAT_UNDEFINED};
            uint32_t offset{0};
            std::size_t member_index{0};
            bool has_explicit_location{false};
        };

    private:
        VertexBinding m_binding{
            .binding = 0,
            .stride = static_cast<uint32_t>(sizeof(T)),
            .input_rate = VK_VERTEX_INPUT_RATE_VERTEX
        };
        std::array<AttributeInfo, AttrCount> m_attributes{};
        bool m_has_custom_binding{false};
        bool m_has_custom_locations{false};

        consteval void init_attributes() {
            constexpr auto ctx = std::meta::access_context::unchecked();
            static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx));
            std::size_t attr_idx = 0;
            std::size_t member_idx = 0;
            uint32_t curr_loc = 0;

            template for (constexpr auto m : members) {
                using M = [: std::meta::type_of(m) :];
                using Trait = vertex_format_trait<M>;
                constexpr std::string_view m_name = std::meta::identifier_of(m);
                constexpr std::size_t base_offset = std::meta::offset_of(m).bytes;

                for (uint32_t s = 0; s < Trait::slots; ++s) {
                    m_attributes[attr_idx] = AttributeInfo{
                        .name = m_name,
                        .location = curr_loc++,
                        .binding = 0,
                        .format = Trait::format,
                        .offset = static_cast<uint32_t>(base_offset + s * Trait::slot_stride),
                        .member_index = member_idx,
                        .has_explicit_location = false
                    };
                    ++attr_idx;
                }
                ++member_idx;
            }
        }

    public:
        constexpr VertexLayoutBuilder() noexcept {
            init_attributes();
        }

        [[nodiscard]] constexpr bool has_custom_binding() const noexcept { return m_has_custom_binding; }
        [[nodiscard]] constexpr bool has_custom_locations() const noexcept { return m_has_custom_locations; }
        [[nodiscard]] constexpr const VertexBinding& get_binding_description() const noexcept { return m_binding; }
        [[nodiscard]] constexpr std::span<const AttributeInfo> get_attributes() const noexcept { return m_attributes; }

        constexpr VertexLayoutBuilder& with_binding(uint32_t binding) noexcept {
            m_binding.binding = binding;
            for (auto& a : m_attributes) {
                a.binding = binding;
            }
            m_has_custom_binding = true;
            return *this;
        }

        constexpr VertexLayoutBuilder& as_instance() noexcept {
            m_binding.input_rate = VK_VERTEX_INPUT_RATE_INSTANCE;
            return *this;
        }

        constexpr VertexLayoutBuilder& with_input_rate(VkVertexInputRate rate) noexcept {
            m_binding.input_rate = rate;
            return *this;
        }

        constexpr VertexLayoutBuilder& with_start_location(uint32_t start_loc) noexcept {
            if (!m_attributes.empty()) {
                uint32_t base = m_attributes[0].location;
                for (auto& a : m_attributes) {
                    a.location = (a.location - base) + start_loc;
                    a.has_explicit_location = true;
                }
            }
            m_has_custom_locations = true;
            return *this;
        }

        /// @brief 通过成员指针覆盖 location (例如 .with_location<&Vertex::uv>(5))
        template<auto MemberPtr>
        constexpr VertexLayoutBuilder& with_location(uint32_t new_loc) {
            constexpr int idx = detail::reflect::find_member_index_by_ptr<T, MemberPtr>();
            static_assert(idx >= 0, "MemberPtr is not a non-static data member of struct T");

            uint32_t loc = new_loc;
            for (auto& a : m_attributes) {
                if (a.member_index == static_cast<std::size_t>(idx)) {
                    a.location = loc++;
                    a.has_explicit_location = true;
                }
            }
            m_has_custom_locations = true;
            return *this;
        }

        /// @brief 通过反射表达式覆盖 location (例如 .with_location<^^Vertex::uv>(5))
        template<std::meta::info Member>
        constexpr VertexLayoutBuilder& with_location(uint32_t new_loc) {
            constexpr int idx = detail::reflect::find_member_index_by_info<T, Member>();
            static_assert(idx >= 0, "Member is not a non-static data member of struct T");

            uint32_t loc = new_loc;
            for (auto& a : m_attributes) {
                if (a.member_index == static_cast<std::size_t>(idx)) {
                    a.location = loc++;
                    a.has_explicit_location = true;
                }
            }
            m_has_custom_locations = true;
            return *this;
        }

        /// @brief 通过字段名称覆盖 location
        constexpr VertexLayoutBuilder& with_location(std::string_view member_name, uint32_t new_loc) {
            uint32_t loc = new_loc;
            for (auto& a : m_attributes) {
                if (a.name == member_name) {
                    a.location = loc++;
                    a.has_explicit_location = true;
                }
            }
            m_has_custom_locations = true;
            return *this;
        }

        [[nodiscard]] VertexInputState build(alib6::ErrorWrapper ew = {}) const {
            // 使用 alib6::storage::MonoBitSet 进行插槽冲突检测与空洞智能填充
            alib6::storage::MonoBitSet occupied(64);
            auto attrs = m_attributes;

            // 第一阶段：注册并校验显式指定的 location
            for (const auto& a : attrs) {
                if (a.has_explicit_location) {
                    if (a.location >= 64) {
                        ew.report(ave_invalid_vertex_layout,
                            "Vertex attribute '{}' location {} exceeds maximum supported (64)",
                            a.name, a.location);
                        continue;
                    }
                    if (occupied.test(a.location)) {
                        ew.report(ave_invalid_vertex_layout,
                            "Vertex attribute location collision: location {} is already occupied (conflict on '{}')",
                            a.location, a.name);
                    } else {
                        occupied.set(a.location);
                    }
                }
            }

            // 第二阶段：未显式指定的字段，通过 MonoBitSet::find_next_0 寻找最近空闲槽位
            alib6::usize search_pos = 0;
            for (auto& a : attrs) {
                if (!a.has_explicit_location) {
                    auto free_slot = occupied.find_next_0(search_pos, 64);
                    if (!free_slot) {
                        ew.report(ave_invalid_vertex_layout,
                            "Exhausted available vertex input locations for attribute '{}' (limit 64)", a.name);
                        continue;
                    }
                    a.location = static_cast<uint32_t>(*free_slot);
                    occupied.set(*free_slot);
                    search_pos = *free_slot + 1;
                }
            }

            VertexInputState state;
            state.bindings.push_back(m_binding);
            state.attributes.reserve(AttrCount);
            state.attribute_names.reserve(AttrCount);
            for (const auto& a : attrs) {
                state.attributes.push_back(VertexAttribute{
                    .location = a.location,
                    .binding = a.binding,
                    .format = a.format,
                    .offset = a.offset
                });
                state.attribute_names.emplace_back(a.name);
            }
            return state;
        }

        [[nodiscard]] std::vector<VertexAttribute> build_attributes(alib6::ErrorWrapper ew = {}) const {
            return build(ew).attributes;
        }

        [[nodiscard]] std::vector<VertexBinding> build_bindings(alib6::ErrorWrapper ew = {}) const {
            return build(ew).bindings;
        }
    };

    template<typename T>
    inline void VertexLayoutComposite::add(const VertexLayoutBuilder<T>& builder) {
        uint32_t next_b = next_available_binding();

        auto b_desc = builder.get_binding_description();
        if (!builder.has_custom_binding() && !m_bindings.empty()) {
            b_desc.binding = next_b;
        }
        m_bindings.push_back(b_desc);

        for (const auto& a : builder.get_attributes()) {
            m_attributes.push_back(CompositeAttr{
                .name = std::string(a.name),
                .location = a.location,
                .binding = b_desc.binding,
                .format = a.format,
                .offset = a.offset,
                .has_explicit_location = a.has_explicit_location
            });
        }
    }

    // ========================================================================
    // 积木拼接运算符 (operator+)
    // ========================================================================

    template<typename T1, typename T2>
    inline VertexLayoutComposite operator+(const VertexLayoutBuilder<T1>& a, const VertexLayoutBuilder<T2>& b) {
        VertexLayoutComposite comp;
        comp.add(a);
        comp.add(b);
        return comp;
    }

    template<typename T>
    inline VertexLayoutComposite operator+(VertexLayoutComposite comp, const VertexLayoutBuilder<T>& b) {
        comp.add(b);
        return comp;
    }

    // ========================================================================
    // 工厂入口函数 (ave::vertex_layout<T>())
    // ========================================================================

    /// @brief 创建顶点布局建造者
    template<typename T>
    [[nodiscard]] constexpr auto vertex_layout() noexcept {
        return VertexLayoutBuilder<T>{};
    }

} // namespace ave
