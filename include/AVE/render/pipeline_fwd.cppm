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
}
