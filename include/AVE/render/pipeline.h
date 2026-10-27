#pragma once

#include <AVE/alib.h>
#include <AVE/render/base.h>
#include <AVE/render/device.h>
#include <AVE/render/legacy_render.h>
#include <AVE/render/pipeline_fwd.h>

/**
 * @file pipeline.cppm
 * @brief Base and specialized graphics/compute pipeline classes
 * @version 5.0
 * @date 2026-09-12
 */
#include <AVE/config.h>
#include <vulkan/vulkan.h>



namespace ave {
    struct AVE_API PipelineShaderStageInfo {
        VkPipelineShaderStageCreateFlags flags { 0 };
        VkShaderStageFlagBits stage { VK_SHADER_STAGE_VERTEX_BIT };
        VkShaderModule module { VK_NULL_HANDLE };
        std::string entry_point { "main" };
        const VkSpecializationInfo* specialization_info { nullptr };
    };

    /// 图形与计算管线共用的着色器、布局及创建资源。
    struct AVE_API CreatePipelineResourceInfo {
        std::shared_ptr<Device> device;
        std::vector<PipelineShaderStageInfo> shader_stages;

        VkPipelineLayoutCreateFlags layout_flags { 0 };
        std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
        std::vector<DescriptorBinding> descriptor_bindings;
        std::vector<DescriptorSetLayoutInfo> descriptor_sets;
        std::vector<ConstantAttribute> constant_attributes;
        std::vector<VkPushConstantRange> push_constant_ranges;

        VkPipelineCreateFlags flags { 0 };
        VkPipelineCache cache { VK_NULL_HANDLE };
        VkPipeline base_pipeline { VK_NULL_HANDLE };
        ave::i32 base_pipeline_index { -1 };

        mutable ave::ErrorWrapper ew {};
    };

    struct AVE_API CreatePipelineCommonInfo
        : CreatePipelineResourceInfo
    {
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
    };

    /// shader_stages 必须且只能包含一个 compute stage；外部 shader module 不转移所有权。
    template<>
    struct AVE_API CreatePipelineInfo<pipeline_type::Compute>
        : CreatePipelineResourceInfo
    {};

    template<>
    struct AVE_API CreatePipelineInfo<pipeline_type::Legacy>
        : CreatePipelineCommonInfo
    {
        std::shared_ptr<LegacyRender> render;
        ave::u32 subpass { 0 };
    };

    template<>
    struct AVE_API CreatePipelineInfo<pipeline_type::Dynamic>
        : CreatePipelineCommonInfo
    {
        ave::u32 view_mask { 0 };
        std::vector<VkFormat> color_attachment_formats;
        VkFormat depth_attachment_format { VK_FORMAT_UNDEFINED };
        VkFormat stencil_attachment_format { VK_FORMAT_UNDEFINED };
    };

    AVE_API void default_configure_graphics_pipeline(
        CreatePipelineCommonInfo& ci,
        ave::u32 color_attachment_count = 1
    );

    class AVE_API Pipeline {
    protected:
        std::shared_ptr<Device> device;
        VkPipelineLayout layout { VK_NULL_HANDLE };
        VkPipeline pipeline { VK_NULL_HANDLE };
        VkPipelineBindPoint bind_point { VK_PIPELINE_BIND_POINT_GRAPHICS };
        PipelineType type { PipelineType::DynamicGraphics };
        ave::u32 push_constant_size { 0 };
        VkShaderStageFlags push_constant_stage_flags { 0 };
        std::vector<VkPushConstantRange> push_constant_ranges;
        std::vector<ConstantAttribute> constant_attributes;
        std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
        std::vector<VkDescriptorSetLayout> owned_descriptor_set_layouts;

        Pipeline() = default;

    public:
        void set_created_state(
            std::shared_ptr<Device> dev,
            VkPipelineLayout lay,
            VkPipeline pipe,
            ave::u32 pc_size,
            VkShaderStageFlags pc_stages,
            std::vector<VkPushConstantRange> pc_ranges,
            std::vector<ConstantAttribute> const_attrs,
            std::vector<VkDescriptorSetLayout> set_layouts = {},
            std::vector<VkDescriptorSetLayout> owned_layouts = {}
        ) noexcept {
            device = std::move(dev);
            layout = lay;
            pipeline = pipe;
            push_constant_size = pc_size;
            push_constant_stage_flags = pc_stages;
            push_constant_ranges = std::move(pc_ranges);
            constant_attributes = std::move(const_attrs);
            descriptor_set_layouts = std::move(set_layouts);
            owned_descriptor_set_layouts = std::move(owned_layouts);
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
        [[nodiscard]] inline ave::u32 get_push_constant_size() const noexcept { return push_constant_size; }
        [[nodiscard]] inline VkShaderStageFlags get_push_constant_stage_flags() const noexcept { return push_constant_stage_flags; }
        [[nodiscard]] inline std::span<const VkPushConstantRange> get_push_constant_ranges() const noexcept { return push_constant_ranges; }
        [[nodiscard]] inline const std::vector<ConstantAttribute>& get_constant_attributes() const noexcept { return constant_attributes; }
        [[nodiscard]] inline const std::vector<VkDescriptorSetLayout>& get_descriptor_set_layouts() const noexcept { return descriptor_set_layouts; }
        [[nodiscard]] inline VkDescriptorSetLayout get_descriptor_set_layout(ave::u32 set_index = 0) const noexcept {
            return set_index < descriptor_set_layouts.size() ? descriptor_set_layouts[set_index] : VK_NULL_HANDLE;
        }
        [[nodiscard]] inline const std::vector<VkDescriptorSetLayout>& get_owned_descriptor_set_layouts() const noexcept { return owned_descriptor_set_layouts; }

        inline void bind(VkCommandBuffer command_buffer) const noexcept {
            vkCmdBindPipeline(command_buffer, bind_point, pipeline);
        }

        /// @brief 绑定一个 descriptor set（默认 set 0）；与 bind() 的先后顺序不影响结果。
        inline void bind_descriptor_set(
            VkCommandBuffer command_buffer,
            VkDescriptorSet set,
            ave::u32 set_index = 0
        ) const noexcept {
            if(command_buffer == VK_NULL_HANDLE || set == VK_NULL_HANDLE) return;
            vkCmdBindDescriptorSets(command_buffer, bind_point,
                get_layout(), set_index, 1, &set, 0, nullptr);
        }

        /// @brief 上传 push constant；stages 为 0 时使用管线布局记录的全部 stage。
        /// @note 布局含多个 stage 不同的 range 时，请显式传入目标 range 的 stage，避免 stage 组合越界。
        template<typename T>
        inline void push_constant(
            VkCommandBuffer command_buffer,
            const T& data,
            VkShaderStageFlags stages = 0,
            ave::u32 offset = 0
        ) const noexcept {
            const VkShaderStageFlags flags = stages == 0 ? push_constant_stage_flags : stages;
            if(command_buffer == VK_NULL_HANDLE || flags == 0) return;
            vkCmdPushConstants(command_buffer, get_layout(), flags, offset,
                static_cast<ave::u32>(sizeof(T)), std::addressof(data));
        }
    };

    class AVE_API ComputePipeline final : public Pipeline {
    private:
        ComputePipeline();
        [[nodiscard]] bool initialize(CreateComputePipelineInfo ci);

    public:
        ~ComputePipeline() override = default;

        [[nodiscard]] static std::shared_ptr<ComputePipeline> create(CreateComputePipelineInfo ci);
        /// 字节码/文件重载临时创建 shader module，管线创建后自动释放。
        [[nodiscard]] static std::shared_ptr<ComputePipeline> create(
            std::shared_ptr<Device> device,
            ShaderBytecode shader,
            ConfigureComputePipeline configure = nullptr,
            ave::ErrorWrapper ew = {}
        );
        [[nodiscard]] static std::shared_ptr<ComputePipeline> create(
            std::shared_ptr<Device> device,
            std::string_view shader_path,
            ConfigureComputePipeline configure = nullptr,
            ave::ErrorWrapper ew = {}
        );

        /// 在 render pass 之外绑定并 dispatch；descriptor/push constants、同步和队列由调用方管理。
        inline void dispatch(
            VkCommandBuffer command_buffer,
            ave::u32 group_count_x,
            ave::u32 group_count_y = 1,
            ave::u32 group_count_z = 1
        ) const noexcept {
            bind(command_buffer);
            vkCmdDispatch(command_buffer, group_count_x, group_count_y, group_count_z);
        }

        /// @brief 绑定管线、descriptor set 与 push constant 后 dispatch；绑定顺序由 AVE 保证。
        template<typename T>
        inline void dispatch(
            VkCommandBuffer command_buffer,
            VkDescriptorSet set,
            const T& push,
            ave::u32 group_count_x,
            ave::u32 group_count_y = 1,
            ave::u32 group_count_z = 1
        ) const noexcept {
            bind(command_buffer);
            bind_descriptor_set(command_buffer, set);
            push_constant(command_buffer, push);
            vkCmdDispatch(command_buffer, group_count_x, group_count_y, group_count_z);
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
