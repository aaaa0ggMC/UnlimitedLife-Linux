#include "app.h"

namespace avetest {

App::App()
    : logger(),
      lg(logger, "avetest"),
      vklg(logger, "Vulkan") {
    logger.append_mod<alib6::lot::Console>("console");
}

bool App::setup() {
    window = std::make_unique<ave::Window>(ave::CreateWindowInfo{
        .ctx = context,
        .title = "Hello from AVE! (Vertex Buffer Triangle)",
        .width = 800,
        .height = 600,
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

    lg << "Dynamic Rendering: "
       << report[ave::RenderBuildStageId::create_device].content["dynamic_rendering"].to<std::string_view>()
       << std::endl;

    if(!setup_vertex_data()) {
        lg(alib6::LogLevel::Error) << "Failed to setup vertex data." << std::endl;
        return false;
    }

    // 使用我们新增的聚合 GraphicsPipelineConfig 配置管线
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

    if(!pipeline) {
        lg(alib6::LogLevel::Error) << "Failed to create graphics pipeline." << std::endl;
        return false;
    }

    return true;
}

bool App::setup_vertex_data() {
    const std::vector<Vertex> vertices = {
        { {  0.0f, -0.5f }, { 1.0f, 0.0f, 0.0f } }, // 顶点 0：红
        { {  0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f } }, // 顶点 1：绿
        { { -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } }  // 顶点 2：蓝
    };

    // 1. 创建 HOST_VISIBLE 顶点缓冲
    vertex_buffer.emplace(ave::CreateBufferInfo{
        .device = renderer->device,
        .size = sizeof(Vertex) * vertices.size(),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
    });

    if (!*vertex_buffer) {
        return false;
    }

    // 2. 使用 upload 便捷模板接口直接上传顶点数据（自动 map -> memcpy -> upload -> unmap）
    if(!vertex_buffer->upload(vertices)) {
        lg(alib6::LogLevel::Error) << "Failed to upload vertices to GPU." << std::endl;
        return false;
    }
    lg << "Uploaded " << vertices.size() << " vertices to GPU via vertex_buffer->upload()." << std::endl;

    return true;
}

} // namespace avetest
