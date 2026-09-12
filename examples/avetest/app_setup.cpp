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
    [&,this](ave::AfterWindowFramebufferResizeEvent& ev) {
        if(ev.width > 0 && ev.height > 0 && renderer.has_value()) {
            ave::RenderBuildReport report;    
            ave::recreate_swapchain_from_window(*renderer, *window, &report);  
            lg << "Recreated swapchain." << std::endl;        
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

    // 使用聚合 GraphicsPipelineConfig 配置管线（错误由 ErrorWrapper 自动处理）
    pipeline = renderer->create_graphics_pipeline({
        .vert = "avetest/shaders/vertex-vert.spv",
        .frag = "avetest/shaders/simple-frag.spv",
        .bindings = {
            { .stride = sizeof(Vertex) }
        },
        .attributes = {
            { .location = 0, .format = VK_FORMAT_R32G32_SFLOAT,    .offset = offsetof(Vertex, pos) },
            { .location = 1, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, color) }
        }
    });
}

void App::setup_vertex_data() {
    const std::vector<Vertex> vertices = {
        // 三角形 1（上方正立）
        { {  0.0f,  -0.75f }, { 1.0f, 0.2f, 0.2f } }, // 顶：红
        { {  0.35f, -0.15f }, { 1.0f, 0.9f, 0.1f } }, // 右下：黄
        { { -0.35f, -0.15f }, { 1.0f, 0.5f, 0.1f } }, // 左下：橙

        // 三角形 2（右下方正立）
        { {  0.55f,  0.05f }, { 0.1f, 0.9f, 1.0f } }, // 顶：浅蓝
        { {  0.90f,  0.75f }, { 0.1f, 0.3f, 1.0f } }, // 右下：深蓝
        { {  0.20f,  0.75f }, { 0.1f, 1.0f, 0.4f } }, // 左下：青绿

        // 三角形 3（左下方正立）
        { { -0.55f,  0.05f }, { 0.7f, 0.2f, 1.0f } }, // 顶：紫罗兰
        { { -0.20f,  0.75f }, { 1.0f, 0.3f, 0.7f } }, // 右下：粉红
        { { -0.90f,  0.75f }, { 0.4f, 0.1f, 0.8f } }, // 左下：深紫

        // 三角形 4（中央倒立）
        { { -0.25f,  0.05f }, { 0.2f, 0.8f, 0.8f } }, // 左上：蓝绿
        { {  0.25f,  0.05f }, { 0.8f, 1.0f, 0.2f } }, // 右上：黄绿
        { {  0.00f,  0.55f }, { 1.0f, 1.0f, 1.0f } }, // 底：白
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
