#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

import ave;
import std;
import alib6;

using namespace alib6::attr;

constexpr std::string_view config_file = "config.fog.json";
/// 应用配置
struct AppConfig {
    struct Window {
        alib6::u32 width = 800;
        alib6::u32 height = 600;
    };

    /// 全局线性雾参数：距离在 [start, end] 之间线性过渡到 color
    struct Fog {
        float r = 0.55f;
        float g = 0.68f;
        float b = 0.78f;
        float start = 5.0f;
        float end = 25.0f;
    };

    [[=schema::optional()]]
    Window window;

    [[=schema::optional()]]
    Fog fog;
};

/// 相机 + 全局线性雾参数的 UBO（std430）
struct alignas(16) CameraFogUbo {
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 fog_color;   // rgb = 雾色
    glm::vec4 fog_params;  // x = start，y = end
};
static_assert(ave::is_std430_compatible_v<CameraFogUbo>);

/// 每个 draw 一个 model 矩阵（push constant）
struct alignas(16) PushConstant {
    glm::mat4 model;
};
static_assert(ave::is_std430_compatible_v<PushConstant>);

#include "app_comp.h"

struct App {
    AppConfig cfg;

    //// 基础 Infra ////
    /// 日志
    alib6::Logger logger;
    alib6::LogFactory lg;
    /// Vulkan 校验层日志
    alib6::LogFactory vklg;

    //// Vulkan 部分 ////
    /// 一些共用的内存组件
    ave::Context context;
    /// 程序窗口
    std::optional<ave::Window> window;
    /// 输入状态机（bind 到窗口，事件在 poll_events 时驱动）
    ave::Input input;

    // 图形资源：按声明顺序逆序析构，renderer 声明在最后（最先析构），
    // 其余资源各自以 shared_ptr 持有 Device / VMA allocation，可独立安全释放。
    std::shared_ptr<ave::VMAAllocator> allocator;
    std::unique_ptr<ave::VMABuffer> staging_buffer;
    std::optional<ave::VMABuffer> geometry_buffer;
    ave::VMABufferSlice cube_vertices;
    ave::VMABufferSlice cube_indices;
    ave::VMABufferSlice ground_vertices;
    ave::VMABufferSlice ground_indices;
    std::optional<ave::VMABuffer> ubo_buffer;
    /// 立方体纹理（ice.png）与地面纹理（wall.jpg 砖墙）
    std::shared_ptr<ave::Image> cube_texture;
    std::shared_ptr<ave::Image> ground_texture;
    std::shared_ptr<ave::Sampler> sampler;
    std::shared_ptr<ave::Pipeline> pipeline;
    VkDescriptorSet descriptor_cubes { VK_NULL_HANDLE };
    VkDescriptorSet descriptor_ground { VK_NULL_HANDLE };
    std::optional<ave::Renderer> renderer;

    //// ECS ////
    /// 实体管理器（在 renderer 之后声明 → 先于 renderer 析构）
    alib6::ecs::EntityManager em;
    /// 相机实体包装（长期持有，组件 ref 已缓存）
    fog::CameraEntity camera { em };
    /// 全局雾实体包装（长期持有；配置值仅作种子）
    fog::FogEntity fog {
        em,
        glm::vec3(cfg.fog.r, cfg.fog.g, cfg.fog.b),
        cfg.fog.start,
        cfg.fog.end
    };

    App(const AppConfig & cfg);
    void setup();
    int run();

private:
    void setup_create_window();
    void setup_renderer();
    void setup_geometry();
    void setup_pipeline();
    void setup_ubo();
    void setup_textures();
    void setup_world();
    void update_camera(float dt);
    void update_fog(float dt);
    void render_scene();
};


//// 提供日志便利 ////
constexpr auto lc_quote = alib6::lot::color(
    alib6::lot::Color::Cyan
);
constexpr auto lc_reset = alib6::lot::color(
    alib6::lot::Color::None
);

/// 引用
template<class T>
struct quote {
    T t;

    template<class R>
    quote(R && s):t(std::forward<R>(s)){}

    template<class Stream>
    Stream&& self_forward(Stream && ctx){
        std::move(ctx) << lc_quote << t << lc_reset;
        return std::move(ctx);
    }
};
template<class T> quote(T&&) -> quote<T>;

/// 扁平化输出
template<class T>
auto flat(T && value) {
    return alib6::to_adata(std::forward<T>(value)).str(
        alib6::data::Flat()
    );
}
