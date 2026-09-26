#include "app.h"

namespace avetest {

App::App()
    : logger(),
      lg(logger, "avetest"),
      vklg(logger, "Vulkan") {
    logger.append_mod<alib6::lot::Console>("console");
}

App::~App() {
    if(descriptor_pool != VK_NULL_HANDLE && renderer && renderer->device) {
        // 主循环退出时最后一帧可能仍在 GPU 上执行，其命令缓冲仍引用本 pool 分配的
        // descriptor set。必须先等待设备空闲，再销毁 pool，否则会触发校验错误
        // VUID-vkDestroyDescriptorPool-descriptorPool-00303。
        if(renderer->wait_idle() != VK_SUCCESS) {
            lg << "Failed to wait for the device to become idle during shutdown."
               << std::endl;
        }
        vkDestroyDescriptorPool(
            renderer->device->get_system_handle(),
            descriptor_pool,
            renderer->device->get_instance()->get_vk_allocator()
        );
        descriptor_pool = VK_NULL_HANDLE;
    }
}

void App::setup() {
    window = std::make_unique<ave::Window>(ave::CreateWindowInfo{
        .ctx = context,
        .title = "Hello from AVE! (Vertex Buffer Triangle)",
        .width = 800,
        .height = 600,
    });

    window->on<ave::AfterWindowFramebufferResizeEvent>(
    [this](ave::AfterWindowFramebufferResizeEvent& ev) {
        if(ev.width > 0 && ev.height > 0 && renderer.has_value()) {
            if(ave::recreate_swapchain_from_window(*renderer, *window)) {
                lg << "Recreated swapchain." << std::endl;
            }
        }
    });

    ave::ProfileWith with;
    with.configure_instance = [](ave::WithGlobalInput&, ave::CreateInstanceInfo& ci) {
        ci.application_name = "avetest";
        ci.api_version = ave::ave_vk_1_4;
    };
    with.configure_debug_messenger.emplace();
    with.try_dynamic_rendering = true;
    with.configure_debug_messenger->on_message = [this](
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT,
        const VkDebugUtilsMessengerCallbackDataEXT& data
    ) {
        vklg(ave::to_log_level(severity))
            << (data.pMessage ? data.pMessage : "<no message>")
            << std::endl;
        return false;
    };

    ave::RenderBuildReport report;
    renderer = ave::RenderProfile::from_window(context, *window)
        .with(with)
        .with_result(report)
        .build();

    // lg << alib6::to_adata(report) << std::endl;
    setup_vertex_data();

    auto vertex_input = ave::vertex_layout<Vertex>().build();
    auto push_layout = ave::constant_layout<PushConstant>().build();

    lg << "Vertex Input Layout Table:\n" << vertex_input.to_table() << std::endl;
    lg << "Push Constant Layout Table:\n" << push_layout.to_table() << std::endl;

    // 使用聚合 GraphicsPipelineConfig 配置管线（错误由 ErrorWrapper 自动处理）
    pipeline = renderer->create_graphics_pipeline({
        .vert = "avetest/shaders/vertex-vert.spv",
        .frag = "avetest/shaders/simple-frag.spv",
        .bindings = vertex_input.bindings,
        .attributes = vertex_input.attributes,
        .cull_mode = VK_CULL_MODE_NONE,
        .depth_test = true,
        .depth_write = true,
        .depth_compare_op = VK_COMPARE_OP_LESS,
        .descriptor_bindings = {
            ave::DescriptorBinding::ubo(0, VK_SHADER_STAGE_VERTEX_BIT)
        },
        .constant_attributes = push_layout.attributes
    });

    setup_ubo();
}

void App::setup_vertex_data() {
    allocator = ave::VMAAllocator::create_shared({ .device = renderer->device });
    // 顶点缓冲区配置为纯 GPU 端 DeviceOnly 显存（高性能，CPU 不可直接写入）
    buffer.emplace(ave::CreateVMABufferInfo{
        .allocator = allocator,
        .size = 1024 * 1024,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .host_access = ave::BufferHostAccess::DeviceOnly,
    });
    
    const std::vector<Vertex> vertices = {
        // 前表面 (+Z, normal: 0, 0, 1)
        { { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } },
        { {  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f, 1.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f } },
        { { -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f, 1.0f } },
        { { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } },

        // 后表面 (-Z, normal: 0, 0, -1)
        { { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, 0.0f } },
        { { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } },
        { {  0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f, 0.0f } },
        { {  0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f, 0.0f } },
        { {  0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
        { { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, 0.0f } },

        // 左表面 (-X, normal: -1, 0, 0)
        { { -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f, 1.0f } },
        { { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } },
        { { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, 0.0f } },
        { { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, 0.0f } },
        { { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } },
        { { -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f, 1.0f } },

        // 右表面 (+X, normal: 1, 0, 0)
        { {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f } },
        { {  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f, 1.0f } },
        { {  0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
        { {  0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
        { {  0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f, 0.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f } },

        // 上表面 (+Y, normal: 0, 1, 0)
        { { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } },
        { { -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f, 1.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f } },
        { {  0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f, 0.0f } },
        { { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } },

        // 下表面 (-Y, normal: 0, -1, 0)
        { { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, 0.0f } },
        { {  0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
        { {  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f, 1.0f } },
        { {  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f, 1.0f } },
        { { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } },
        { { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, 0.0f } },
    };

    // 创建临时的 CPU 可见中介缓冲 (Staging Buffer)
    staging_buffer = std::make_unique<ave::VMABuffer>(ave::CreateVMABufferInfo{
        .allocator = allocator,
        .size = sizeof(Vertex) * vertices.size(),
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .host_access = ave::BufferHostAccess::SequentialWrite,
    });

    // 一行代码无感通过 staging 上传到 DeviceOnly 顶点缓冲区切片
    vertices_data = buffer->alloc(vertices, {
        .map_info = {
            .staging = staging_buffer.get(),
            .upload_context = renderer->get_upload_context()
        }
    }); 
    vertex_count = static_cast<uint32_t>(vertices.size());

    lg << "Uploaded " << vertices.size() << " vertices to DeviceOnly GPU buffer via staging buffer." << std::endl;
}

void App::setup_ubo() {
    ubo_buffer.emplace(ave::CreateVMABufferInfo{
        .allocator = allocator,
        .size = sizeof(CameraUbo),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .host_access = ave::BufferHostAccess::RandomAccess,
        .persistent_mapping = true,
    });

    VkDescriptorPoolSize pool_size {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
    };
    VkDescriptorPoolCreateInfo pool_ci {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };
    const auto handle = renderer->device->get_system_handle();
    const auto allocator_vk = renderer->device->get_instance()->get_vk_allocator();

    vkCreateDescriptorPool(handle, &pool_ci, allocator_vk, &descriptor_pool);

    VkDescriptorSetLayout layout = pipeline->get_descriptor_set_layout(0);
    VkDescriptorSetAllocateInfo alloc_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };
    vkAllocateDescriptorSets(handle, &alloc_info, &descriptor_set);

    VkDescriptorBufferInfo buffer_info {
        .buffer = ubo_buffer->get_system_handle(),
        .offset = 0,
        .range = sizeof(CameraUbo),
    };
    VkWriteDescriptorSet write {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &buffer_info,
    };
    vkUpdateDescriptorSets(handle, 1, &write, 0, nullptr);

    lg << "Initialized Camera UBO buffer and allocated DescriptorSet from Pipeline layout." << std::endl;
}

} // namespace avetest
