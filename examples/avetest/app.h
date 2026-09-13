#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

import alib6;
import std;
import ave;

namespace avetest {

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
};

struct alignas(16) PushConstant {
    glm::mat4 mvp;
};
static_assert(ave::is_std430_compatible_v<PushConstant>);

// 小朋友们不要学我偶
struct RenderCache {
    ave::Pipeline & pipeline;
    ave::Buffer & buffer;
    uint32_t vertex_count { 36 };
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
    uint32_t vertex_count { 0 };

    App();
    ~App() = default;

    void setup();
    void setup_vertex_data();
    void run();
    void render_frame(RenderCache rc, const PushConstant& pc);
};



} // namespace avetest
