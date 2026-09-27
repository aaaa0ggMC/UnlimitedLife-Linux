#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "app_data.h"

import alib6;
import std;
import ave;

namespace avetest {

struct alignas(16) CameraUbo {
    glm::mat4 vp;
};
static_assert(ave::is_std430_compatible_v<CameraUbo>);

struct alignas(16) PushConstant {
    glm::mat4 model;
};
static_assert(ave::is_std430_compatible_v<PushConstant>);

// 小朋友们不要学我偶
struct RenderCache {
    ave::Pipeline & pipeline;
    uint32_t index_count { 36 };
};

struct App {
    alib6::Logger logger;
    alib6::LogFactory lg;
    alib6::LogFactory vklg;

    ave::Context context;
    std::unique_ptr<ave::Window> window;
    std::shared_ptr<ave::VMAAllocator> allocator;
    std::unique_ptr<ave::VMABuffer> staging_buffer;
    std::optional<ave::VMABuffer> buffer;
    std::optional<ave::VMABuffer> ubo_buffer;
    VkDescriptorSet descriptor_set { VK_NULL_HANDLE };
    ave::VMABufferSlice vertices_data;
    ave::VMABufferSlice indices_data;
    std::shared_ptr<ave::Image> texture;
    std::shared_ptr<ave::Sampler> sampler;
    std::shared_ptr<ave::Pipeline> pipeline;
    std::optional<ave::Renderer> renderer;
    uint32_t index_count { 0 };

    App();
    // 图形资源（含 DescriptorPool/CommandPool 等）均由 Renderer 及其 RAII 组件自行析构，
    // 这里不需要任何图形相关清理。
    ~App() = default;

    void setup();
    void setup_vertex_data();
    void setup_texture();
    void setup_ubo();
    void run();
    void render_frame(RenderCache rc, const PushConstant& pc);
};



} // namespace avetest
