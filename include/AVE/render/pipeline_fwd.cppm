module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.render:pipeline_fwd;

import std;
import alib6;

export namespace ave {
    namespace pipeline_type {
        struct Dynamic {};
        struct Legacy {};
    }

    template<class Type>
    concept PipelineType =
        std::same_as<Type, pipeline_type::Dynamic> ||
        std::same_as<Type, pipeline_type::Legacy>;

    template<PipelineType Type = pipeline_type::Dynamic>
    class Pipeline;

    template<PipelineType Type>
    struct CreatePipelineInfo;

    using DynamicPipeline = Pipeline<pipeline_type::Dynamic>;
    using LegacyPipeline = Pipeline<pipeline_type::Legacy>;

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

    using ConfigureLegacyPipeline = std::function<void(
        CreatePipelineInfo<pipeline_type::Legacy>&
    )>;
}
