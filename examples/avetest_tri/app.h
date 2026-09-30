#pragma once
#include <vulkan/vulkan.h>

import alib6;
import std;
import ave;

namespace avetest_tri {

/**
 * avetest 的精简 fork：与 LearnVulkan2 当前场景对齐（无顶点缓冲、无描述符、无 push constant、
 * 单个硬编码三角形），用于对比 AVE 封装相对原生 Vulkan 的开销。
 * 着色器为 LearnVulkan2 的同一份 SPIR-V。
 */
struct App {
    alib6::Logger logger;
    alib6::LogFactory lg;
    alib6::LogFactory vklg;

    ave::Context context;
    std::unique_ptr<ave::Window> window;
    std::shared_ptr<ave::Pipeline> pipeline;
    std::optional<ave::Renderer> renderer;

    App();
    ~App() = default;

    void setup();
    void run();
    void render_frame();
};

} // namespace avetest_tri
