#pragma once
#include <vulkan/vulkan.h>

import alib6;
import std;
import ave;

namespace avetest {

struct Vertex {
    float pos[2];
    float color[3];
};

// 小朋友们不要学我偶
struct RenderCache {
    ave::Pipeline & pipeline;
    ave::Buffer & buffer;
};

struct App {
    alib6::Logger logger;
    alib6::LogFactory lg;
    alib6::LogFactory vklg;

    ave::Context context;
    std::unique_ptr<ave::Window> window;
    std::optional<ave::Buffer> vertex_buffer;
    std::shared_ptr<ave::Pipeline> pipeline;
    std::optional<ave::Renderer> renderer;

    App();
    ~App() = default;

    bool setup();
    bool setup_vertex_data();
    void run();
    void render_frame(RenderCache rc);
};



} // namespace avetest
