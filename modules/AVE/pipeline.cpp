module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>
#include <alib6/debug.h>
#include <cstring>

module ave.render;

import std;
import alib6;
import ave.ecode;
import :pipeline;
import :legacy_render;
import :dynamic_render;
import :swapchain;
import :render;

namespace ave {

void default_configure_graphics_pipeline(
    CreatePipelineCommonInfo& ci,
    alib6::u32 color_attachment_count
) {
    ci.vertex_bindings.clear();
    ci.vertex_attributes.clear();

    ci.input_assembly = {};
    ci.input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ci.input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ci.input_assembly.primitiveRestartEnable = VK_FALSE;

    ci.tessellation = {};
    ci.tessellation.sType =
        VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    ci.tessellation.patchControlPoints = 3;

    ci.viewport = {};
    ci.viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    ci.viewport.viewportCount = 1;
    ci.viewport.scissorCount = 1;

    ci.rasterization = {};
    ci.rasterization.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    ci.rasterization.depthClampEnable = VK_FALSE;
    ci.rasterization.rasterizerDiscardEnable = VK_FALSE;
    ci.rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    ci.rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    ci.rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
    ci.rasterization.depthBiasEnable = VK_FALSE;
    ci.rasterization.lineWidth = 1.0f;

    ci.multisample = {};
    ci.multisample.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ci.multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    ci.multisample.sampleShadingEnable = VK_FALSE;
    ci.multisample.minSampleShading = 1.0f;

    ci.depth_stencil = {};
    ci.depth_stencil.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ci.depth_stencil.depthTestEnable = VK_FALSE;
    ci.depth_stencil.depthWriteEnable = VK_FALSE;
    ci.depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
    ci.use_depth_stencil_state = false;

    VkPipelineColorBlendAttachmentState attachment {};
    attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    attachment.blendEnable = VK_TRUE;
    attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    attachment.colorBlendOp = VK_BLEND_OP_ADD;
    attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    ci.color_blend_attachments.assign(color_attachment_count, attachment);

    ci.color_blend = {};
    ci.color_blend.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ci.color_blend.logicOpEnable = VK_FALSE;
    ci.color_blend.logicOp = VK_LOGIC_OP_COPY;

    ci.dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
}

namespace {
    bool has_stage(
        const std::vector<PipelineShaderStageInfo>& stages,
        VkShaderStageFlagBits stage
    ) {
        return std::ranges::any_of(stages, [stage](const auto& value) {
            return value.stage == stage;
        });
    }

    struct CreatedShaderModules {
        VkShaderModule vertex { VK_NULL_HANDLE };
        VkShaderModule fragment { VK_NULL_HANDLE };
        VkShaderModule geometry { VK_NULL_HANDLE };
        VkShaderModule tessellation_control { VK_NULL_HANDLE };
        VkShaderModule tessellation_evaluation { VK_NULL_HANDLE };
    };

    class ShaderModuleScope {
    private:
        std::shared_ptr<Device> device;
        std::vector<VkShaderModule> modules;
    public:
        explicit ShaderModuleScope(std::shared_ptr<Device> target)
        :device(std::move(target)){}

        ~ShaderModuleScope() {
            if(!device || device->get_system_handle() == VK_NULL_HANDLE) return;
            const auto allocator = device->get_instance()->get_vk_allocator();
            for(const auto module : modules) {
                vkDestroyShaderModule(device->get_system_handle(), module, allocator);
            }
        }

        VkShaderModule create(
            ShaderBytecode bytecode,
            alib6::ErrorWrapper& ew
        ) {
            if(bytecode.empty()) return VK_NULL_HANDLE;
            if(bytecode.size() % sizeof(alib6::u32) != 0) {
                ew.report(ave_vk_create_shader_module,
                    "SPIR-V bytecode size ({}) is not divisible by four.", bytecode.size());
                return VK_NULL_HANDLE;
            }

            std::vector<alib6::u32> aligned_code(
                bytecode.size() / sizeof(alib6::u32)
            );
            std::memcpy(aligned_code.data(), bytecode.data(), bytecode.size());
            VkShaderModuleCreateInfo create_info {};
            create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            create_info.codeSize = bytecode.size();
            create_info.pCode = aligned_code.data();

            VkShaderModule result = VK_NULL_HANDLE;
            const VkResult code = vkCreateShaderModule(
                device->get_system_handle(),
                &create_info,
                device->get_instance()->get_vk_allocator(),
                &result
            );
            if(code != VK_SUCCESS) {
                ew.report(ave_vk_create_shader_module,
                    "Failed to create Vulkan ShaderModule ({}).", static_cast<int>(code));
                return VK_NULL_HANDLE;
            }
            modules.push_back(result);
            return result;
        }
    };

    void append_shader_stages(
        CreatePipelineCommonInfo& ci,
        const CreatedShaderModules& modules,
        std::string_view entry_point
    ) {
        ci.shader_stages.clear();
        const auto append = [&](VkShaderStageFlagBits stage, VkShaderModule module) {
            if(module == VK_NULL_HANDLE) return;
            ci.shader_stages.push_back({
                .stage = stage,
                .module = module,
                .entry_point = std::string(entry_point)
            });
        };
        append(VK_SHADER_STAGE_VERTEX_BIT, modules.vertex);
        append(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
            modules.tessellation_control);
        append(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
            modules.tessellation_evaluation);
        append(VK_SHADER_STAGE_GEOMETRY_BIT, modules.geometry);
        append(VK_SHADER_STAGE_FRAGMENT_BIT, modules.fragment);
    }

    ShaderBytecode as_bytecode(const std::string& value) {
        return {
            reinterpret_cast<const alib6::u8*>(value.data()),
            value.size()
        };
    }

    bool create_graphics_pipeline_common(
        CreatePipelineCommonInfo& ci,
        VkPipelineRenderingCreateInfo* rendering_info,
        VkRenderPass render_pass,
        alib6::u32 subpass,
        std::shared_ptr<Device>& out_device,
        VkPipelineLayout& out_layout,
        VkPipeline& out_pipeline
    ) {
        if(!ci.device || ci.device->get_system_handle() == VK_NULL_HANDLE ||
           ci.shader_stages.empty()) {
            ci.ew.report(ave_vk_create_graphics_pipeline,
                "Cannot create a Graphics Pipeline without a valid Device and shader stages.");
            return false;
        }

        const bool has_vertex = has_stage(ci.shader_stages, VK_SHADER_STAGE_VERTEX_BIT);
        const bool has_fragment = has_stage(ci.shader_stages, VK_SHADER_STAGE_FRAGMENT_BIT);
        const bool has_tess_control = has_stage(
            ci.shader_stages, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
        const bool has_tess_evaluation = has_stage(
            ci.shader_stages, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
        const bool unique_stages = std::ranges::all_of(
            ci.shader_stages,
            [&](const auto& stage) {
                return stage.module != VK_NULL_HANDLE && !stage.entry_point.empty() &&
                    std::ranges::count(ci.shader_stages, stage.stage,
                        &PipelineShaderStageInfo::stage) == 1;
            }
        );
        if(!has_vertex || !has_fragment ||
           has_tess_control != has_tess_evaluation || !unique_stages) {
            ci.ew.report(ave_vk_create_graphics_pipeline,
                "Graphics Pipeline requires unique vertex/fragment stages and a complete tessellation pair.");
            return false;
        }

        std::vector<VkPipelineShaderStageCreateInfo> stages;
        stages.reserve(ci.shader_stages.size());
        for(const auto& source : ci.shader_stages) {
            VkPipelineShaderStageCreateInfo stage {};
            stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stage.flags = source.flags;
            stage.stage = source.stage;
            stage.module = source.module;
            stage.pName = source.entry_point.c_str();
            stage.pSpecializationInfo = source.specialization_info;
            stages.push_back(stage);
        }

        VkPipelineLayoutCreateInfo layout_info {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.flags = ci.layout_flags;
        layout_info.setLayoutCount = static_cast<alib6::u32>(
            ci.descriptor_set_layouts.size());
        layout_info.pSetLayouts = ci.descriptor_set_layouts.data();
        layout_info.pushConstantRangeCount = static_cast<alib6::u32>(
            ci.push_constant_ranges.size());
        layout_info.pPushConstantRanges = ci.push_constant_ranges.data();

        const auto handle = ci.device->get_system_handle();
        const auto allocator = ci.device->get_instance()->get_vk_allocator();
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkResult code = vkCreatePipelineLayout(handle, &layout_info, allocator, &layout);
        if(code != VK_SUCCESS) {
            ci.ew.report(ave_vk_create_pipeline_layout,
                "Failed to create Vulkan PipelineLayout ({}).", static_cast<int>(code));
            return false;
        }

        VkPipelineVertexInputStateCreateInfo vertex_input {};
        vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input.vertexBindingDescriptionCount = static_cast<alib6::u32>(
            ci.vertex_bindings.size());
        vertex_input.pVertexBindingDescriptions = ci.vertex_bindings.data();
        vertex_input.vertexAttributeDescriptionCount = static_cast<alib6::u32>(
            ci.vertex_attributes.size());
        vertex_input.pVertexAttributeDescriptions = ci.vertex_attributes.data();

        auto input_assembly = ci.input_assembly;
        input_assembly.sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        auto tessellation = ci.tessellation;
        tessellation.sType =
            VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        auto viewport = ci.viewport;
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        auto rasterization = ci.rasterization;
        rasterization.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        auto multisample = ci.multisample;
        multisample.sType =
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        auto depth_stencil = ci.depth_stencil;
        depth_stencil.sType =
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        auto color_blend = ci.color_blend;
        color_blend.sType =
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend.attachmentCount = static_cast<alib6::u32>(
            ci.color_blend_attachments.size());
        color_blend.pAttachments = ci.color_blend_attachments.data();
        VkPipelineDynamicStateCreateInfo dynamic_state {};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = static_cast<alib6::u32>(
            ci.dynamic_states.size());
        dynamic_state.pDynamicStates = ci.dynamic_states.data();

        VkGraphicsPipelineCreateInfo pipeline_info {};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.flags = ci.flags;
        pipeline_info.stageCount = static_cast<alib6::u32>(stages.size());
        pipeline_info.pStages = stages.data();
        pipeline_info.pVertexInputState = &vertex_input;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pTessellationState = has_tess_control ? &tessellation : nullptr;
        pipeline_info.pViewportState = &viewport;
        pipeline_info.pRasterizationState = &rasterization;
        pipeline_info.pMultisampleState = &multisample;
        const bool require_depth_stencil = ci.use_depth_stencil_state ||
            (rendering_info && (rendering_info->depthAttachmentFormat != VK_FORMAT_UNDEFINED ||
                                rendering_info->stencilAttachmentFormat != VK_FORMAT_UNDEFINED));
        pipeline_info.pDepthStencilState = require_depth_stencil
            ? &depth_stencil : nullptr;
        pipeline_info.pColorBlendState = &color_blend;
        pipeline_info.pDynamicState = ci.dynamic_states.empty()
            ? nullptr : &dynamic_state;
        pipeline_info.layout = layout;
        pipeline_info.basePipelineHandle = ci.base_pipeline;
        pipeline_info.basePipelineIndex = ci.base_pipeline_index;

        if(rendering_info) {
            pipeline_info.pNext = rendering_info;
            pipeline_info.renderPass = VK_NULL_HANDLE;
            pipeline_info.subpass = 0;
        } else {
            pipeline_info.pNext = nullptr;
            pipeline_info.renderPass = render_pass;
            pipeline_info.subpass = subpass;
        }

        VkPipeline pipeline = VK_NULL_HANDLE;
        code = vkCreateGraphicsPipelines(
            handle, ci.cache, 1, &pipeline_info, allocator, &pipeline);
        if(code != VK_SUCCESS) {
            ci.ew.report(ave_vk_create_graphics_pipeline,
                "Failed to create Vulkan Graphics Pipeline ({}).", static_cast<int>(code));
            vkDestroyPipelineLayout(handle, layout, allocator);
            return false;
        }

        out_device = ci.device;
        out_layout = layout;
        out_pipeline = pipeline;
        return true;
    }
}

// ---------------- Pipeline base implementation ----------------

Pipeline::~Pipeline() {
    destroy();
}

void Pipeline::destroy() noexcept {
    if(device && device->get_system_handle() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device->get_system_handle());
        const auto handle = device->get_system_handle();
        const auto allocator = device->get_instance()->get_vk_allocator();
        if(pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(handle, pipeline, allocator);
        }
        if(layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(handle, layout, allocator);
        }
    }
    pipeline = VK_NULL_HANDLE;
    layout = VK_NULL_HANDLE;
    device.reset();
}

// ---------------- DynamicPipeline implementation ----------------

DynamicPipeline::DynamicPipeline() {
    type = PipelineType::DynamicGraphics;
    bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
}

bool DynamicPipeline::initialize(CreatePipelineInfo<pipeline_type::Dynamic> ci) {
    if(!ci.device || !ci.device->supports_dynamic_rendering()) {
        ci.ew.report(ave_vk_create_graphics_pipeline,
            "A DynamicPipeline requires a valid Device supporting dynamic rendering.");
        return false;
    }

    VkPipelineRenderingCreateInfo rendering_info {};
    rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering_info.viewMask = ci.view_mask;
    rendering_info.colorAttachmentCount = static_cast<alib6::u32>(
        ci.color_attachment_formats.size());
    rendering_info.pColorAttachmentFormats =
        ci.color_attachment_formats.data();
    rendering_info.depthAttachmentFormat = ci.depth_attachment_format;
    rendering_info.stencilAttachmentFormat = ci.stencil_attachment_format;

    return create_graphics_pipeline_common(
        ci, &rendering_info, VK_NULL_HANDLE, 0,
        device, layout, pipeline
    );
}

std::shared_ptr<DynamicPipeline> DynamicPipeline::create(
    CreatePipelineInfo<pipeline_type::Dynamic> ci
) {
    auto result = std::shared_ptr<DynamicPipeline>(new DynamicPipeline());
    if(!result->initialize(std::move(ci))) return {};
    return result;
}

// ---------------- LegacyPipeline implementation ----------------

LegacyPipeline::LegacyPipeline() {
    type = PipelineType::LegacyGraphics;
    bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
}

bool LegacyPipeline::initialize(CreatePipelineInfo<pipeline_type::Legacy> ci) {
    if(!ci.render || !*ci.render) {
        ci.ew.report(ave_vk_create_graphics_pipeline,
            "A LegacyPipeline requires a valid LegacyRender.");
        return false;
    }
    ci.device = ci.render->get_device();

    return create_graphics_pipeline_common(
        ci, nullptr, ci.render->get_render_pass(), ci.subpass,
        device, layout, pipeline
    );
}

std::shared_ptr<LegacyPipeline> LegacyPipeline::create(
    CreatePipelineInfo<pipeline_type::Legacy> ci
) {
    auto result = std::shared_ptr<LegacyPipeline>(new LegacyPipeline());
    if(!result->initialize(std::move(ci))) return {};
    return result;
}

// ---------------- LegacyRender::create_graphics_pipeline ----------------

std::shared_ptr<LegacyPipeline> LegacyRender::create_graphics_pipeline(
    GraphicsShaderBytecode shaders,
    ConfigureLegacyPipeline configure
) {
    CreatePipelineInfo<pipeline_type::Legacy> ci;
    ci.render = shared_from_this();
    ci.device = get_device();
    ci.subpass = 0;
    default_configure_graphics_pipeline(
        ci,
        get_subpass_color_attachment_count(ci.subpass)
    );
    if(subpass_has_depth_stencil(ci.subpass)) {
        ci.depth_stencil.depthTestEnable = VK_TRUE;
        ci.depth_stencil.depthWriteEnable = VK_TRUE;
        ci.depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
        ci.use_depth_stencil_state = true;
    }
    const bool incomplete_tessellation =
        shaders.tessellation.control.empty() !=
        shaders.tessellation.evaluation.empty();
    if(shaders.vertex.empty() || shaders.fragment.empty() ||
       incomplete_tessellation || shaders.entry_point.empty()) {
        ci.ew.report(ave_vk_create_shader_module,
            "Graphics shader bytecode requires vertex/fragment and a complete tessellation pair.");
        return {};
    }

    ShaderModuleScope module_scope(get_device());
    CreatedShaderModules modules;
    modules.vertex = module_scope.create(shaders.vertex, ci.ew);
    modules.fragment = module_scope.create(shaders.fragment, ci.ew);
    modules.geometry = module_scope.create(shaders.geometry, ci.ew);
    modules.tessellation_control = module_scope.create(
        shaders.tessellation.control, ci.ew);
    modules.tessellation_evaluation = module_scope.create(
        shaders.tessellation.evaluation, ci.ew);
    if(modules.vertex == VK_NULL_HANDLE || modules.fragment == VK_NULL_HANDLE ||
       (!shaders.geometry.empty() && modules.geometry == VK_NULL_HANDLE) ||
       (shaders.tessellation.complete() &&
        (modules.tessellation_control == VK_NULL_HANDLE ||
         modules.tessellation_evaluation == VK_NULL_HANDLE))) {
        return {};
    }

    append_shader_stages(ci, modules, shaders.entry_point);
    if(shaders.tessellation.complete()) {
        ci.input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
        ci.tessellation.patchControlPoints =
            shaders.tessellation.patch_control_points;
    }
    if(configure) configure(ci);
    return LegacyPipeline::create(std::move(ci));
}

std::shared_ptr<LegacyPipeline> LegacyRender::create_graphics_pipeline(
    ShaderBytecode vertex,
    ShaderBytecode fragment,
    ShaderBytecode geometry,
    TessellationShaderBytecode tessellation,
    ConfigureLegacyPipeline configure
) {
    return create_graphics_pipeline(GraphicsShaderBytecode {
        .vertex = vertex,
        .fragment = fragment,
        .geometry = geometry,
        .tessellation = tessellation
    }, std::move(configure));
}

std::shared_ptr<LegacyPipeline> LegacyRender::create_graphics_pipeline(
    GraphicsShaderPaths shaders,
    ConfigureLegacyPipeline configure
) {
    const bool incomplete_tessellation =
        shaders.tessellation.control.empty() !=
        shaders.tessellation.evaluation.empty();
    if(shaders.vertex.empty() || shaders.fragment.empty() || incomplete_tessellation) {
        alib6::ErrorWrapper{}.report(ave_vk_create_shader_module,
            "Graphics shader paths require vertex/fragment and a complete tessellation pair.");
        return {};
    }

    std::string vertex;
    std::string fragment;
    std::string geometry;
    std::string tessellation_control;
    std::string tessellation_evaluation;
    const auto read = [](std::string_view path, std::string& output) {
        if(path.empty()) return true;
        return alib6::io::read_all(path, output) !=
            std::numeric_limits<alib6::usize>::max();
    };
    if(!read(shaders.vertex, vertex) ||
       !read(shaders.fragment, fragment) ||
       !read(shaders.geometry, geometry) ||
       !read(shaders.tessellation.control, tessellation_control) ||
       !read(shaders.tessellation.evaluation, tessellation_evaluation)) {
        alib6::ErrorWrapper{}.report(ave_vk_create_shader_module,
            "Failed to read one or more SPIR-V shader files.");
        return {};
    }

    return create_graphics_pipeline(GraphicsShaderBytecode {
        .vertex = as_bytecode(vertex),
        .fragment = as_bytecode(fragment),
        .geometry = as_bytecode(geometry),
        .tessellation = {
            .control = as_bytecode(tessellation_control),
            .evaluation = as_bytecode(tessellation_evaluation),
            .patch_control_points = shaders.tessellation.patch_control_points
        },
        .entry_point = std::move(shaders.entry_point)
    }, std::move(configure));
}

std::shared_ptr<LegacyPipeline> LegacyRender::create_graphics_pipeline(
    std::string_view vertex,
    std::string_view fragment,
    std::string_view geometry,
    TessellationShaderPaths tessellation,
    ConfigureLegacyPipeline configure
) {
    return create_graphics_pipeline(GraphicsShaderPaths {
        .vertex = vertex,
        .fragment = fragment,
        .geometry = geometry,
        .tessellation = tessellation
    }, std::move(configure));
}

// ---------------- DynamicRender::create_graphics_pipeline ----------------

std::shared_ptr<DynamicPipeline> DynamicRender::create_graphics_pipeline(
    GraphicsShaderBytecode shaders,
    ConfigureDynamicPipeline configure
) {
    CreatePipelineInfo<pipeline_type::Dynamic> ci;
    ci.device = get_device();
    ci.color_attachment_formats = color_attachment_formats;
    ci.depth_attachment_format = depth_attachment_format;
    ci.stencil_attachment_format = stencil_attachment_format;
    default_configure_graphics_pipeline(
        ci,
        static_cast<alib6::u32>(ci.color_attachment_formats.size())
    );
    if(ci.depth_attachment_format != VK_FORMAT_UNDEFINED) {
        ci.depth_stencil.depthTestEnable = VK_TRUE;
        ci.depth_stencil.depthWriteEnable = VK_TRUE;
        ci.depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
        ci.use_depth_stencil_state = true;
    }
    const bool incomplete_tessellation =
        shaders.tessellation.control.empty() !=
        shaders.tessellation.evaluation.empty();
    if(shaders.vertex.empty() || shaders.fragment.empty() ||
       incomplete_tessellation || shaders.entry_point.empty()) {
        ci.ew.report(ave_vk_create_shader_module,
            "Graphics shader bytecode requires vertex/fragment and a complete tessellation pair.");
        return {};
    }

    ShaderModuleScope module_scope(get_device());
    CreatedShaderModules modules;
    modules.vertex = module_scope.create(shaders.vertex, ci.ew);
    modules.fragment = module_scope.create(shaders.fragment, ci.ew);
    modules.geometry = module_scope.create(shaders.geometry, ci.ew);
    modules.tessellation_control = module_scope.create(
        shaders.tessellation.control, ci.ew);
    modules.tessellation_evaluation = module_scope.create(
        shaders.tessellation.evaluation, ci.ew);
    if(modules.vertex == VK_NULL_HANDLE || modules.fragment == VK_NULL_HANDLE ||
       (!shaders.geometry.empty() && modules.geometry == VK_NULL_HANDLE) ||
       (shaders.tessellation.complete() &&
        (modules.tessellation_control == VK_NULL_HANDLE ||
         modules.tessellation_evaluation == VK_NULL_HANDLE))) {
        return {};
    }

    append_shader_stages(ci, modules, shaders.entry_point);
    if(shaders.tessellation.complete()) {
        ci.input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
        ci.tessellation.patchControlPoints =
            shaders.tessellation.patch_control_points;
    }
    if(configure) configure(ci);
    return DynamicPipeline::create(std::move(ci));
}

std::shared_ptr<DynamicPipeline> DynamicRender::create_graphics_pipeline(
    ShaderBytecode vertex,
    ShaderBytecode fragment,
    ShaderBytecode geometry,
    TessellationShaderBytecode tessellation,
    ConfigureDynamicPipeline configure
) {
    return create_graphics_pipeline(GraphicsShaderBytecode {
        .vertex = vertex,
        .fragment = fragment,
        .geometry = geometry,
        .tessellation = tessellation
    }, std::move(configure));
}

std::shared_ptr<DynamicPipeline> DynamicRender::create_graphics_pipeline(
    GraphicsShaderPaths shaders,
    ConfigureDynamicPipeline configure
) {
    const bool incomplete_tessellation =
        shaders.tessellation.control.empty() !=
        shaders.tessellation.evaluation.empty();
    if(shaders.vertex.empty() || shaders.fragment.empty() || incomplete_tessellation) {
        alib6::ErrorWrapper{}.report(ave_vk_create_shader_module,
            "Graphics shader paths require vertex/fragment and a complete tessellation pair.");
        return {};
    }

    std::string vertex;
    std::string fragment;
    std::string geometry;
    std::string tessellation_control;
    std::string tessellation_evaluation;
    const auto read = [](std::string_view path, std::string& output) {
        if(path.empty()) return true;
        return alib6::io::read_all(path, output) !=
            std::numeric_limits<alib6::usize>::max();
    };
    if(!read(shaders.vertex, vertex) ||
       !read(shaders.fragment, fragment) ||
       !read(shaders.geometry, geometry) ||
       !read(shaders.tessellation.control, tessellation_control) ||
       !read(shaders.tessellation.evaluation, tessellation_evaluation)) {
        alib6::ErrorWrapper{}.report(ave_vk_create_shader_module,
            "Failed to read one or more SPIR-V shader files.");
        return {};
    }

    return create_graphics_pipeline(GraphicsShaderBytecode {
        .vertex = as_bytecode(vertex),
        .fragment = as_bytecode(fragment),
        .geometry = as_bytecode(geometry),
        .tessellation = {
            .control = as_bytecode(tessellation_control),
            .evaluation = as_bytecode(tessellation_evaluation),
            .patch_control_points = shaders.tessellation.patch_control_points
        },
        .entry_point = std::move(shaders.entry_point)
    }, std::move(configure));
}

std::shared_ptr<DynamicPipeline> DynamicRender::create_graphics_pipeline(
    std::string_view vertex,
    std::string_view fragment,
    std::string_view geometry,
    TessellationShaderPaths tessellation,
    ConfigureDynamicPipeline configure
) {
    return create_graphics_pipeline(GraphicsShaderPaths {
        .vertex = vertex,
        .fragment = fragment,
        .geometry = geometry,
        .tessellation = tessellation
    }, std::move(configure));
}

// ---------------- Renderer render backend creation ----------------

std::shared_ptr<LegacyRender> Renderer::create_legacy_render(
    ConfigureLegacyRender configure,
    LegacyRenderCreateStatus* status,
    alib6::ErrorWrapper ew
) {
    if(!device || !swapchain) {
        ew.report(ave_vk_create_render_pass,
            "Cannot create LegacyRender without a valid Device and Swapchain.");
        return nullptr;
    }
    WithLegacyRenderInput input(device, swapchain, images);
    CreateLegacyRenderInfo ci;
    ci.swapchain = swapchain;
    ci.ew = ew;
    if(configure) {
        configure(input, ci);
    } else {
        default_configure_legacy_render(input, ci);
    }
    legacy_render = LegacyRender::create(std::move(ci), status);
    if(legacy_render) {
        (void)invalidate_graphics_cache();
    }
    return legacy_render;
}

std::shared_ptr<DynamicRender> Renderer::create_dynamic_render(
    ConfigureDynamicRender configure,
    alib6::ErrorWrapper ew
) {
    if(!device || !swapchain) {
        ew.report(ave_vk_create_device,
            "Cannot create DynamicRender without a valid Device and Swapchain.");
        return nullptr;
    }
    WithDynamicRenderInput input(device, swapchain, images);
    CreateDynamicRenderInfo ci;
    ci.swapchain = swapchain;
    ci.ew = ew;
    if(configure) {
        configure(input, ci);
    } else {
        default_configure_dynamic_render(input, ci);
    }
    dynamic_render = DynamicRender::create(std::move(ci));
    if(dynamic_render) {
        (void)invalidate_graphics_cache();
    }
    return dynamic_render;
}

// ---------------- Renderer::create_dynamic_graphics_pipeline ----------------

std::shared_ptr<DynamicPipeline> Renderer::create_dynamic_graphics_pipeline(
    GraphicsShaderBytecode shaders,
    ConfigureDynamicPipeline configure
) {
    panic_debug(!dynamic_render,
        "Renderer is not in dynamic rendering mode, cannot create dynamic graphics pipeline.");
    return dynamic_render->create_graphics_pipeline(std::move(shaders), std::move(configure));
}

std::shared_ptr<DynamicPipeline> Renderer::create_dynamic_graphics_pipeline(
    ShaderBytecode vertex,
    ShaderBytecode fragment,
    ShaderBytecode geometry,
    TessellationShaderBytecode tessellation,
    ConfigureDynamicPipeline configure
) {
    panic_debug(!dynamic_render,
        "Renderer is not in dynamic rendering mode, cannot create dynamic graphics pipeline.");
    return dynamic_render->create_graphics_pipeline(
        vertex, fragment, geometry, tessellation, std::move(configure));
}

std::shared_ptr<DynamicPipeline> Renderer::create_dynamic_graphics_pipeline(
    GraphicsShaderPaths shaders,
    ConfigureDynamicPipeline configure
) {
    panic_debug(!dynamic_render,
        "Renderer is not in dynamic rendering mode, cannot create dynamic graphics pipeline.");
    return dynamic_render->create_graphics_pipeline(shaders, std::move(configure));
}

std::shared_ptr<DynamicPipeline> Renderer::create_dynamic_graphics_pipeline(
    std::string_view vertex,
    std::string_view fragment,
    std::string_view geometry,
    TessellationShaderPaths tessellation,
    ConfigureDynamicPipeline configure
) {
    panic_debug(!dynamic_render,
        "Renderer is not in dynamic rendering mode, cannot create dynamic graphics pipeline.");
    return dynamic_render->create_graphics_pipeline(
        vertex, fragment, geometry, tessellation, std::move(configure));
}

// ---------------- Renderer::create_legacy_graphics_pipeline ----------------

std::shared_ptr<LegacyPipeline> Renderer::create_legacy_graphics_pipeline(
    GraphicsShaderBytecode shaders,
    ConfigureLegacyPipeline configure
) {
    panic_debug(!legacy_render,
        "Renderer is not in legacy rendering mode, cannot create legacy graphics pipeline.");
    return legacy_render->create_graphics_pipeline(std::move(shaders), std::move(configure));
}

std::shared_ptr<LegacyPipeline> Renderer::create_legacy_graphics_pipeline(
    ShaderBytecode vertex,
    ShaderBytecode fragment,
    ShaderBytecode geometry,
    TessellationShaderBytecode tessellation,
    ConfigureLegacyPipeline configure
) {
    panic_debug(!legacy_render,
        "Renderer is not in legacy rendering mode, cannot create legacy graphics pipeline.");
    return legacy_render->create_graphics_pipeline(
        vertex, fragment, geometry, tessellation, std::move(configure));
}

std::shared_ptr<LegacyPipeline> Renderer::create_legacy_graphics_pipeline(
    GraphicsShaderPaths shaders,
    ConfigureLegacyPipeline configure
) {
    panic_debug(!legacy_render,
        "Renderer is not in legacy rendering mode, cannot create legacy graphics pipeline.");
    return legacy_render->create_graphics_pipeline(shaders, std::move(configure));
}

std::shared_ptr<LegacyPipeline> Renderer::create_legacy_graphics_pipeline(
    std::string_view vertex,
    std::string_view fragment,
    std::string_view geometry,
    TessellationShaderPaths tessellation,
    ConfigureLegacyPipeline configure
) {
    panic_debug(!legacy_render,
        "Renderer is not in legacy rendering mode, cannot create legacy graphics pipeline.");
    return legacy_render->create_graphics_pipeline(
        vertex, fragment, geometry, tessellation, std::move(configure));
}

// ---------------- Renderer::create_graphics_pipeline (graceful degradation) ----------------

std::shared_ptr<Pipeline> Renderer::create_graphics_pipeline(
    GraphicsShaderBytecode shaders,
    ConfigureGraphicsPipeline configure
) {
    if(dynamic_render) {
        return dynamic_render->create_graphics_pipeline(
            std::move(shaders),
            configure ? ConfigureDynamicPipeline([configure](CreatePipelineInfo<pipeline_type::Dynamic>& ci) {
                configure(ci);
            }) : ConfigureDynamicPipeline{}
        );
    }
    if(legacy_render) {
        return legacy_render->create_graphics_pipeline(
            std::move(shaders),
            configure ? ConfigureLegacyPipeline([configure](CreatePipelineInfo<pipeline_type::Legacy>& ci) {
                configure(ci);
            }) : ConfigureLegacyPipeline{}
        );
    }
    panic_debug(false, "Renderer has neither dynamic_render nor legacy_render initialized.");
    return {};
}

std::shared_ptr<Pipeline> Renderer::create_graphics_pipeline(
    ShaderBytecode vertex,
    ShaderBytecode fragment,
    ShaderBytecode geometry,
    TessellationShaderBytecode tessellation,
    ConfigureGraphicsPipeline configure
) {
    return create_graphics_pipeline(GraphicsShaderBytecode {
        .vertex = vertex,
        .fragment = fragment,
        .geometry = geometry,
        .tessellation = tessellation
    }, std::move(configure));
}

std::shared_ptr<Pipeline> Renderer::create_graphics_pipeline(
    GraphicsShaderPaths shaders,
    ConfigureGraphicsPipeline configure
) {
    if(dynamic_render) {
        return dynamic_render->create_graphics_pipeline(
            shaders,
            configure ? ConfigureDynamicPipeline([configure](CreatePipelineInfo<pipeline_type::Dynamic>& ci) {
                configure(ci);
            }) : ConfigureDynamicPipeline{}
        );
    }
    if(legacy_render) {
        return legacy_render->create_graphics_pipeline(
            shaders,
            configure ? ConfigureLegacyPipeline([configure](CreatePipelineInfo<pipeline_type::Legacy>& ci) {
                configure(ci);
            }) : ConfigureLegacyPipeline{}
        );
    }
    panic_debug(false, "Renderer has neither dynamic_render nor legacy_render initialized.");
    return {};
}

std::shared_ptr<Pipeline> Renderer::create_graphics_pipeline(
    std::string_view vertex,
    std::string_view fragment,
    std::string_view geometry,
    TessellationShaderPaths tessellation,
    ConfigureGraphicsPipeline configure
) {
    return create_graphics_pipeline(GraphicsShaderPaths {
        .vertex = vertex,
        .fragment = fragment,
        .geometry = geometry,
        .tessellation = tessellation
    }, std::move(configure));
}

std::shared_ptr<Pipeline> Renderer::create_graphics_pipeline(
    GraphicsPipelineConfig config
) {
    GraphicsShaderPaths paths {
        .vertex = config.vert,
        .fragment = config.frag,
        .geometry = config.geom,
        .entry_point = config.entry_point
    };

    return create_graphics_pipeline(paths, [cfg = std::move(config)](CreatePipelineCommonInfo& ci) {
        ci.vertex_bindings.clear();
        ci.vertex_bindings.reserve(cfg.bindings.size());
        for(const auto& b : cfg.bindings) {
            ci.vertex_bindings.push_back(static_cast<VkVertexInputBindingDescription>(b));
        }

        ci.vertex_attributes.clear();
        ci.vertex_attributes.reserve(cfg.attributes.size());
        for(const auto& a : cfg.attributes) {
            ci.vertex_attributes.push_back(static_cast<VkVertexInputAttributeDescription>(a));
        }

        ci.input_assembly.topology = cfg.topology;
        ci.rasterization.polygonMode = cfg.polygon_mode;
        ci.rasterization.cullMode = cfg.cull_mode;
        ci.rasterization.frontFace = cfg.front_face;

        ci.depth_stencil.depthTestEnable = cfg.depth_test ? VK_TRUE : VK_FALSE;
        ci.depth_stencil.depthWriteEnable = cfg.depth_write ? VK_TRUE : VK_FALSE;
        ci.depth_stencil.depthCompareOp = cfg.depth_compare_op;
        ci.use_depth_stencil_state = ci.use_depth_stencil_state || cfg.depth_test || cfg.depth_write;

        if(!ci.color_blend_attachments.empty()) {
            for(auto& att : ci.color_blend_attachments) {
                att.blendEnable = cfg.blend_enable ? VK_TRUE : VK_FALSE;
                att.srcColorBlendFactor = cfg.src_color_blend;
                att.dstColorBlendFactor = cfg.dst_color_blend;
                att.colorBlendOp = cfg.color_blend_op;
                att.srcAlphaBlendFactor = cfg.src_alpha_blend;
                att.dstAlphaBlendFactor = cfg.dst_alpha_blend;
                att.alphaBlendOp = cfg.alpha_blend_op;
            }
        }

        if(!cfg.descriptor_set_layouts.empty()) {
            ci.descriptor_set_layouts = cfg.descriptor_set_layouts;
        }
        if(!cfg.push_constant_ranges.empty()) {
            ci.push_constant_ranges = cfg.push_constant_ranges;
        }

        if(cfg.configure) {
            cfg.configure(ci);
        }
    });
}

}

