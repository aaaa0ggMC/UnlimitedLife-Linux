/**
 * @file pipeline_fwd.cppm
 * @brief Forward declarations and common types for Pipeline hierarchy
 * @version 5.0
 * @date 2026-09-12
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.render:pipeline_fwd;

import std;
import alib6;
import ave.render.base;

export namespace ave {
    enum class PipelineType : alib6::u8 {
        DynamicGraphics,
        LegacyGraphics,
        Compute,
        RayTracing
    };

    namespace pipeline_type {
        struct Dynamic {};
        struct Legacy {};
    }

    class Pipeline;
    class DynamicPipeline;
    class LegacyPipeline;

    struct CreatePipelineCommonInfo;

    template<class Type>
    struct CreatePipelineInfo;

    using CreateDynamicPipelineInfo = CreatePipelineInfo<pipeline_type::Dynamic>;
    using CreateLegacyPipelineInfo = CreatePipelineInfo<pipeline_type::Legacy>;

    using ShaderBytecode = std::span<const alib6::u8>;

    struct AVE_API TessellationShaderBytecode {
        ShaderBytecode control {};
        ShaderBytecode evaluation {};
        alib6::u32 patch_control_points { 3 };

        [[nodiscard]] bool complete() const noexcept {
            return !control.empty() && !evaluation.empty();
        }
    };

    struct AVE_API GraphicsShaderBytecode {
        ShaderBytecode vertex {};
        ShaderBytecode fragment {};
        ShaderBytecode geometry {};
        TessellationShaderBytecode tessellation {};
        std::string entry_point { "main" };
    };

    struct AVE_API TessellationShaderPaths {
        std::string_view control {};
        std::string_view evaluation {};
        alib6::u32 patch_control_points { 3 };

        [[nodiscard]] bool complete() const noexcept {
            return !control.empty() && !evaluation.empty();
        }
    };

    struct AVE_API GraphicsShaderPaths {
        std::string_view vertex;
        std::string_view fragment;
        std::string_view geometry {};
        TessellationShaderPaths tessellation {};
        std::string entry_point { "main" };
    };

    using ConfigureGraphicsPipeline = std::function<void(
        CreatePipelineCommonInfo&
    )>;

    using ConfigureLegacyPipeline = std::function<void(
        CreatePipelineInfo<pipeline_type::Legacy>&
    )>;

    using ConfigureDynamicPipeline = std::function<void(
        CreatePipelineInfo<pipeline_type::Dynamic>&
    )>;

    /// 简化版聚合图形管线配置（仅 vert 与 frag 无默认值）
    struct AVE_API GraphicsPipelineConfig {
        std::string_view vert;
        std::string_view frag;
        std::string_view geom {};
        std::string entry_point { "main" };

        std::vector<VertexBinding> bindings {};
        std::vector<VertexAttribute> attributes {};

        VkPrimitiveTopology topology { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
        VkPolygonMode polygon_mode { VK_POLYGON_MODE_FILL };
        VkCullModeFlags cull_mode { VK_CULL_MODE_BACK_BIT };
        VkFrontFace front_face { VK_FRONT_FACE_CLOCKWISE };

        bool depth_test { false };
        bool depth_write { false };
        VkCompareOp depth_compare_op { VK_COMPARE_OP_LESS };

        bool blend_enable { true };
        VkBlendFactor src_color_blend { VK_BLEND_FACTOR_SRC_ALPHA };
        VkBlendFactor dst_color_blend { VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA };
        VkBlendOp color_blend_op { VK_BLEND_OP_ADD };
        VkBlendFactor src_alpha_blend { VK_BLEND_FACTOR_ONE };
        VkBlendFactor dst_alpha_blend { VK_BLEND_FACTOR_ZERO };
        VkBlendOp alpha_blend_op { VK_BLEND_OP_ADD };

        std::vector<VkDescriptorSetLayout> descriptor_set_layouts {};
        std::vector<ConstantAttribute> constant_attributes {};
        std::vector<VkPushConstantRange> push_constant_ranges {};

        ConfigureGraphicsPipeline configure { nullptr };
        mutable alib6::ErrorWrapper ew {};
    };
}
