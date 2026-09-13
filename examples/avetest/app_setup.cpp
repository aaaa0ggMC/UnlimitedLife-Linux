#include "app.h"

namespace avetest {

App::App()
    : logger(),
      lg(logger, "avetest"),
      vklg(logger, "Vulkan") {
    logger.append_mod<alib6::lot::Console>("console");
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
    with.try_dynamic_rendering = false;
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

    lg << "Dynamic Rendering: "
       << report[ave::RenderBuildStageId::create_device].content["dynamic_rendering"].to<std::string_view>()
       << std::endl;

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
        .constant_attributes = push_layout.attributes
    });
}

void App::setup_vertex_data() {
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

    vertex_count = static_cast<uint32_t>(vertices.size());

    // 1. 创建 HOST_VISIBLE 顶点缓冲
    vertex_buffer.emplace(ave::CreateBufferInfo{
        .device = renderer->device,
        .size = sizeof(Vertex) * vertices.size(),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
    });

    // 2. 使用 upload 便捷模板接口直接上传顶点数据（错误由 ErrorWrapper 自动拦截处理）
    vertex_buffer->upload(vertices);
    lg << "Uploaded " << vertices.size() << " vertices to GPU via vertex_buffer->upload()." << std::endl;
}

} // namespace avetest
