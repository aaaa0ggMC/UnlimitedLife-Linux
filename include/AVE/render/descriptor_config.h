#pragma once

#include <AVE/alib.h>
#include <AVE/render/base.h>

/**
 * @file descriptor_config.h
 * @brief JSON-driven descriptor layout configuration (compact array / reflected object forms)
 * @version 5.5
 * @date 2026-10-27
 *
 * 目标：初始化管线时不再逐条手写 `DescriptorBinding::ssbo(3, VK_SHADER_STAGE_COMPUTE_BIT)`，
 * 而是用一段紧凑 JSON 描述绑定，由 alib5 的 JSON/反射设施转成 `DescriptorBinding`。
 *
 * 支持的形式（大小写不敏感）：
 * @code
 * // 1. 顶层数组，每项 [type, binding, stage?, count?]；stage 支持 "vertex|fragment"
 * ave::parse_descriptor_bindings(R"([["ubo",0],["ssbo",3],["sampler",4,"compute"]])",
 *                                ave::ShaderStage::compute);
 *
 * // 2. 顶层对象，stage 为整组默认值；条目既可以是数组，也可以是反射对象
 * //    {"stage":"compute","bindings":[["ssbo",3],{"type":"sampler","binding":4,"count":2}]}
 *
 * // 3. 两元素且第二项是字符串时，binding 取数组下标（适合连续 binding）
 * //    [["ubo","compute"],["ssbo","compute"]]
 * @endcode
 */
#include <AVE/config.h>
#include <alib5/adata.h>
#include <optional>
#include <string_view>
#include <vector>



namespace ave {

    /// @brief 描述符类型；枚举名即 JSON 中可写的名字（忽略大小写）。
    enum class DescriptorKind {
        ubo,
        dynamic_ubo,
        ssbo,
        dynamic_ssbo,
        sampler,
        image
    };

    /// @brief 着色阶段；枚举名即 JSON 中可写的名字（忽略大小写）。
    enum class ShaderStage {
        vertex,
        tessellation_control,
        tessellation_evaluation,
        geometry,
        fragment,
        compute,
        all_graphics,
        all
    };

    /// @brief 反射友好的单条绑定描述；stage 为空时使用布局默认 stage。
    /// @note stage_flags 用于组合阶段（如 vertex|fragment）；非空时优先于 stage。
    struct AVE_API DescriptorBindingSpec {
        DescriptorKind type { DescriptorKind::ubo };
        ave::u32 binding { 0 };
        ave::u32 count { 1 };
        std::optional<ShaderStage> stage {};
        std::optional<VkShaderStageFlags> stage_flags {};
    };

    [[nodiscard]] AVE_API VkDescriptorType to_vk_descriptor_type(DescriptorKind kind) noexcept;
    [[nodiscard]] AVE_API VkShaderStageFlags to_vk_shader_stages(ShaderStage stage) noexcept;

    /// @brief 把描述（可带默认 stage）转成 AVE 管线使用的 DescriptorBinding。
    [[nodiscard]] AVE_API DescriptorBinding to_descriptor_binding(
        const DescriptorBindingSpec& spec,
        ShaderStage default_stage = ShaderStage::all
    );

    /// @brief 解析紧凑 JSON 并生成绑定列表；解析失败经 @p ew 上报并返回空列表。
    [[nodiscard]] AVE_API std::vector<DescriptorBinding> parse_descriptor_bindings(
        std::string_view json,
        ShaderStage default_stage = ShaderStage::all,
        ave::ErrorWrapper ew = {}
    );

    /// @brief 从文件读取并解析（相对路径按当前工作目录）。
    [[nodiscard]] AVE_API std::vector<DescriptorBinding> load_descriptor_bindings(
        std::string_view path,
        ShaderStage default_stage = ShaderStage::all,
        ave::ErrorWrapper ew = {}
    );
}
