/**
 * @file pipeline.cppm
 * @brief Legacy and dynamic graphics pipeline template
 * @version 5.0
 * @date 2026-09-11
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.render:pipeline;

import std;
import alib6;
import :device;
import :legacy_render;
import :pipeline_fwd;

export namespace ave {
    struct AVE_API PipelineShaderStageInfo {
        VkPipelineShaderStageCreateFlags flags { 0 };
        VkShaderStageFlagBits stage { VK_SHADER_STAGE_VERTEX_BIT };
        VkShaderModule module { VK_NULL_HANDLE };
        std::string entry_point { "main" };
        const VkSpecializationInfo* specialization_info { nullptr };
    };

    struct AVE_API CreatePipelineCommonInfo {
        std::shared_ptr<Device> device;
        std::vector<PipelineShaderStageInfo> shader_stages;

        VkPipelineLayoutCreateFlags layout_flags { 0 };
        std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
        std::vector<VkPushConstantRange> push_constant_ranges;

        VkPipelineCreateFlags flags { 0 };
        VkPipelineCache cache { VK_NULL_HANDLE };
        VkPipeline base_pipeline { VK_NULL_HANDLE };
        alib6::i32 base_pipeline_index { -1 };

        std::vector<VkVertexInputBindingDescription> vertex_bindings;
        std::vector<VkVertexInputAttributeDescription> vertex_attributes;
        VkPipelineInputAssemblyStateCreateInfo input_assembly {};
        VkPipelineTessellationStateCreateInfo tessellation {};
        VkPipelineViewportStateCreateInfo viewport {};
        VkPipelineRasterizationStateCreateInfo rasterization {};
        VkPipelineMultisampleStateCreateInfo multisample {};
        VkPipelineDepthStencilStateCreateInfo depth_stencil {};
        bool use_depth_stencil_state { false };
        std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments;
        VkPipelineColorBlendStateCreateInfo color_blend {};
        std::vector<VkDynamicState> dynamic_states;

        mutable alib6::ErrorWrapper ew {};
    };

    template<>
    struct AVE_API CreatePipelineInfo<pipeline_type::Legacy>
        : CreatePipelineCommonInfo
    {
        std::shared_ptr<LegacyRender> render;
        alib6::u32 subpass { 0 };
    };

    template<>
    struct AVE_API CreatePipelineInfo<pipeline_type::Dynamic>
        : CreatePipelineCommonInfo
    {
        alib6::u32 view_mask { 0 };
        std::vector<VkFormat> color_attachment_formats;
        VkFormat depth_attachment_format { VK_FORMAT_UNDEFINED };
        VkFormat stencil_attachment_format { VK_FORMAT_UNDEFINED };
    };

    AVE_API void default_configure_graphics_pipeline(
        CreatePipelineCommonInfo& ci,
        alib6::u32 color_attachment_count = 1
    );

    template<PipelineType Type>
    class AVE_API Pipeline final {
    private:
        std::shared_ptr<Device> device;
        VkPipelineLayout layout { VK_NULL_HANDLE };
        VkPipeline pipeline { VK_NULL_HANDLE };

        Pipeline() = default;
        [[nodiscard]] bool initialize(CreatePipelineInfo<Type> ci);

    public:
        ~Pipeline();
        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;
        Pipeline(Pipeline&&) = delete;
        Pipeline& operator=(Pipeline&&) = delete;

        [[nodiscard]] static std::shared_ptr<Pipeline> create(
            CreatePipelineInfo<Type> ci
        );
        void destroy() noexcept;

        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept;
        [[nodiscard]] VkPipelineLayout get_layout() const noexcept;
        [[nodiscard]] VkPipeline get_system_handle() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;
    };

    extern template class AVE_API Pipeline<pipeline_type::Dynamic>;
    extern template class AVE_API Pipeline<pipeline_type::Legacy>;
}
