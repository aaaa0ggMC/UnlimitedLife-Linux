#include "app.h"

auto App::setup() -> void {
    setup_create_window();
}

auto App::setup_create_window() -> void {
    // 初始化窗口
    window.emplace(ave::CreateWindowInfo {
        .ctx = context,
        .title = "Volumetric Fog by aaaa0ggmc",
        .width = 800,
        .height = 600
    });
    // 处理Swapchain重建
    window->on<ave::AfterWindowFramebufferResizeEvent>(
        [this](const auto &){
            
        }
    );
}