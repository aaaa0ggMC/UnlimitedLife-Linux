#include "app.h"

namespace avetest_tri {

App::App()
    : logger(),
      lg(logger, "avetest_tri"),
      vklg(logger, "Vulkan") {
    logger.append_mod<alib6::lot::Console>("console");
}

void App::setup() {
    window = std::make_unique<ave::Window>(ave::CreateWindowInfo{
        .ctx = context,
        .title = "AVE triangle (LearnVulkan2 comparison)",
        .width = 800,
        .height = 600,
    });

    window->on<ave::AfterWindowFramebufferResizeEvent>(
    [this](ave::AfterWindowFramebufferResizeEvent&) {
        if(ave::recreate_swapchain_from_window(*renderer, *window)) {
            lg << "Recreated swapchain." << std::endl;
        }
    });

    ave::ProfileWith with;
    with.configure_instance = [](ave::WithGlobalInput&, ave::CreateInstanceInfo& ci) {
        ci.application_name = "avetest_tri";
        ci.api_version = ave::ave_vk_1_4;
    };
    with.configure_debug_messenger.emplace();
    // 基准用：AVE_RENDER_MODE=legacy 强制 RenderPass（LegacyRender），默认优先 dynamic rendering
    {
        const char* mode = std::getenv("AVE_RENDER_MODE");
        with.try_dynamic_rendering = !(mode && std::string_view(mode) == "legacy");
    }
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

    {
        const auto [fb_w, fb_h] = window->get_framebuffer_size();
        lg << "Framebuffer: " << fb_w << "x" << fb_h << std::endl;
    }
    lg << "Render backend: " << (renderer->dynamic_render ? "dynamic rendering" : "legacy render pass")
       << ", present mode: " << static_cast<int>(renderer->swapchain->get_present_mode())
       << " (1=MAILBOX, 2=FIFO, 0=IMMEDIATE)." << std::endl;

    // 与 LearnVulkan2 一致：顶点由着色器内硬编码，不需要顶点输入 / 描述符 / 常量
    pipeline = renderer->create_graphics_pipeline({
        .vert = "avetest_tri/shaders/tri-vert.spv",
        .frag = "avetest_tri/shaders/tri-frag.spv",
        .cull_mode = VK_CULL_MODE_NONE,
        .depth_test = false,
        .depth_write = false,
    });
}

} // namespace avetest_tri
