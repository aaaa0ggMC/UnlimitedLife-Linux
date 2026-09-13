/**
 * @file pipeline.cppm
 * @brief Base and specialized graphics pipeline classes
 * @version 5.0
 * @date 2026-09-12
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
        std::vector<ConstantAttribute> constant_attributes;
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

    class AVE_API Pipeline {
    protected:
        std::shared_ptr<Device> device;
        VkPipelineLayout layout { VK_NULL_HANDLE };
        VkPipeline pipeline { VK_NULL_HANDLE };
        VkPipelineBindPoint bind_point { VK_PIPELINE_BIND_POINT_GRAPHICS };
        PipelineType type { PipelineType::DynamicGraphics };
        alib6::u32 push_constant_size { 0 };
        VkShaderStageFlags push_constant_stage_flags { 0 };
        std::vector<VkPushConstantRange> push_constant_ranges;
        std::vector<ConstantAttribute> constant_attributes;

        Pipeline() = default;

    public:
        void set_created_state(
            std::shared_ptr<Device> dev,
            VkPipelineLayout lay,
            VkPipeline pipe,
            alib6::u32 pc_size,
            VkShaderStageFlags pc_stages,
            std::vector<VkPushConstantRange> pc_ranges,
            std::vector<ConstantAttribute> const_attrs
        ) noexcept {
            device = std::move(dev);
            layout = lay;
            pipeline = pipe;
            push_constant_size = pc_size;
            push_constant_stage_flags = pc_stages;
            push_constant_ranges = std::move(pc_ranges);
            constant_attributes = std::move(const_attrs);
        }

        virtual ~Pipeline();
        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;
        Pipeline(Pipeline&&) = delete;
        Pipeline& operator=(Pipeline&&) = delete;

        void destroy() noexcept;

        [[nodiscard]] inline const std::shared_ptr<Device>& get_device() const noexcept { return device; }
        [[nodiscard]] inline VkPipelineLayout get_layout() const noexcept { return layout; }
        [[nodiscard]] inline VkPipeline get_system_handle() const noexcept { return pipeline; }
        [[nodiscard]] inline VkPipelineBindPoint get_bind_point() const noexcept { return bind_point; }
        [[nodiscard]] inline PipelineType get_type() const noexcept { return type; }
        [[nodiscard]] inline bool is_dynamic() const noexcept { return type == PipelineType::DynamicGraphics; }
        [[nodiscard]] inline bool is_legacy() const noexcept { return type == PipelineType::LegacyGraphics; }
        [[nodiscard]] inline bool is_graphics() const noexcept {
            return type == PipelineType::DynamicGraphics || type == PipelineType::LegacyGraphics;
        }
        [[nodiscard]] inline bool is_compute() const noexcept { return type == PipelineType::Compute; }
        [[nodiscard]] inline explicit operator bool() const noexcept { return pipeline != VK_NULL_HANDLE; }
        [[nodiscard]] inline alib6::u32 get_push_constant_size() const noexcept { return push_constant_size; }
        [[nodiscard]] inline VkShaderStageFlags get_push_constant_stage_flags() const noexcept { return push_constant_stage_flags; }
        [[nodiscard]] inline std::span<const VkPushConstantRange> get_push_constant_ranges() const noexcept { return push_constant_ranges; }
        [[nodiscard]] inline const std::vector<ConstantAttribute>& get_constant_attributes() const noexcept { return constant_attributes; }

        inline void bind(VkCommandBuffer command_buffer) const noexcept {
            vkCmdBindPipeline(command_buffer, bind_point, pipeline);
        }
    };

    class AVE_API DynamicPipeline final : public Pipeline {
    private:
        friend class Pipeline;
        DynamicPipeline();
        [[nodiscard]] bool initialize(CreatePipelineInfo<pipeline_type::Dynamic> ci);

    public:
        ~DynamicPipeline() override = default;

        [[nodiscard]] static std::shared_ptr<DynamicPipeline> create(
            CreatePipelineInfo<pipeline_type::Dynamic> ci
        );
    };

    class AVE_API LegacyPipeline final : public Pipeline {
    private:
        friend class Pipeline;
        LegacyPipeline();
        [[nodiscard]] bool initialize(CreatePipelineInfo<pipeline_type::Legacy> ci);

    public:
        ~LegacyPipeline() override = default;

        [[nodiscard]] static std::shared_ptr<LegacyPipeline> create(
            CreatePipelineInfo<pipeline_type::Legacy> ci
        );
    };
}
