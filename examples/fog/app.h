/**
 * @file app.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 声明啥的
 * @version 5.0
 * @date 2026-10-14
 * 
 * @copyright Copyright (c) 2026
 * @parblock audit history (AI 别写这里)
 * 1. OK
 * 2. OK
 * @endparblock
 */
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include "app_comp.h"
#include <AVE/AVE.h>
#include <AVE/alib.h>
#include <alib5/aecs.h>
#include <alib5/adata.h>
#include <alib5/data/reflect.h>
#include <alib5/compact/load_config.h>

int check_radiance_gpu();
int check_editor();

using namespace alib5::attr;

constexpr std::string_view config_file = "config.fog.json";
/// 场景描述文件：运行期从 assets 读取；几何字段齐全，局部雾数组可选。
constexpr std::string_view scene_file = "scene.json";
/// 输入采样 + 世界更新的固定节拍（Hz）：一秒 120 次
constexpr double world_update_rate = 120.0;
/// 单个节拍的秒数：update_camera / update_environment 的 dt
constexpr float world_update_dt = static_cast<float>(1.0 / world_update_rate);

/// 应用配置
struct AppConfig {
    
    struct Window {
        ave::u32 width = 800;
        ave::u32 height = 600;
    };

    struct Fog {
        /// 雾/散射色
        float r = 0.55f;
        float g = 0.68f;
        float b = 0.78f;
        /// 可选全局近端裁剪距离；运行期 near_cutoff 默认关闭，介质连续到相机。
        float start = 8.0f;
        /// 仅为读取旧配置保留；不再限制雾积分，也不暴露为控件。
        [[=schema::optional()]]
        float end = 55.0f;
        /// 全空间基础密度 
        float density = 0.05f;
        /// 近地面层附加密度
        float mist_density = 0.02f;
        /// 高度衰减 k：附加项的层厚 ≈ 1/k（块）
        float height_falloff = 0.35f;
        /// 附加密度的参考面高度
        float ground_y = -0.5f;
        /// 世界坐标值噪声频率（越大雾团越碎）
        float noise_scale = 0.09f;
        /// 噪声对附加密度的扰动强度 0..1（0 = 均匀）
        float noise_amount = 0.50f;

        float anim_period = 60.0f;
        /// 动画幅度 0..0.85：瞬时密度在 [1-a, 1+a] 倍之间脉动
        float anim_amplitude = 0.35f;
        /// 行波的空间频率（世界单位；越大波纹越密。天空只取与位置无关的呼吸项）
        float anim_frequency = 0.06f;
    };

    struct Sky {
        /// 可选经纬度全景图，路径相对于 assets
        std::string texture; 
        float exposure = 1.0f;
    };

    struct ShapeConfig {
        std::string path = "test_data/dragon.obj";
    };

    /// ImGui 界面
    struct Ui {
        /// 中文界面字体（相对 assets）；缺失时退回内置字体
        std::string font = "test_data/wqy-microhei.ttf";
        /// 界面整体缩放（字体与内边距一起放大）
        float scale = 1.25f;
    };

    struct Lighting {
        float ambient = 0.24f;
        bool specular = true;
    };

    [[=schema::optional()]]
    Window window;

    [[=schema::optional()]]
    Fog fog;

    [[=schema::optional()]]
    Sky sky;

    [[=schema::optional()]]
    ShapeConfig shape;

    [[=schema::optional()]]
    Ui ui;

    [[=schema::optional()]]
    Lighting lighting;
};

using SceneVec = std::array<float, 3>;
/// scene.json: materials.<name>
struct SceneMaterial {
    SceneVec color;
    float specular;
    float shininess;
    float emission;
    int voxel;
};
/// scene.json: cubes[] 元素与 dragon
struct SceneEntry {
    std::string tag;
    SceneVec pos;
    SceneVec axis;
    float angle;
    SceneVec scale;
    std::string material;
    bool solid;
    /// 是否参与太阳/点光 shadow pass；自发光/装饰件可关闭。
    [[=schema::optional()]]
    bool shadow { true };
};
struct SceneFogTexture {
    std::string path;
    [[=schema::optional()]]
    std::array<int, 3> extent { 32, 32, 32 };
    [[=schema::optional()]]
    SceneVec tiling { 1.0f, 1.0f, 1.0f };
    [[=schema::optional()]]
    SceneVec offset { 0.0f, 0.0f, 0.0f };
    [[=schema::optional()]]
    float strength { 1.0f };
    [[=schema::optional()]]
    bool repeat { false };
};
struct SceneFogControlPoint {
    SceneVec position { 0.5f, 0.5f, 0.5f };
    float radius { 0.25f }, density { 1.0f }, softness { 0.75f };
    SceneVec amplitude { 0.05f, 0.05f, 0.05f };
    float phase { 0.0f };
    int frequency { 1 };
};
struct SceneFogControlField {
    [[=schema::optional()]]
    bool enabled { false };
    [[=schema::optional()]]
    bool animated { false };
    [[=schema::optional()]]
    float period { 12.0f };
    [[=schema::optional()]]
    float noise_scale { 6.0f }; // [0.1, 32], matches GPU field generation
    [[=schema::optional()]]
    float noise_strength { 0.65f };
    [[=schema::optional()]]
    int octaves { 3 }; // [1, 5]
    [[=schema::optional()]]
    int seed { 1 }; // [0, 65535]
    [[=schema::optional()]]
    std::vector<SceneFogControlPoint> points { SceneFogControlPoint{} };
};
bool valid_control_field(const SceneFogControlField& field);
fog::FogControlField control_field_component(const SceneFogControlField& field);
struct SceneFogVolume {
    std::string tag;
    SceneVec pos;
    SceneVec scale;
    [[=schema::optional()]]
    SceneVec axis { 0.0f, 1.0f, 0.0f };
    [[=schema::optional()]]
    float angle { 0.0f };
    [[=schema::optional()]]
    int shape { 0 };
    [[=schema::optional()]]
    float density { 0.15f };
    [[=schema::optional()]]
    SceneVec color { 0.75f, 0.82f, 0.90f };
    [[=schema::optional()]]
    float albedo { 0.90f };
    [[=schema::optional()]]
    float edge_fade { 0.20f };
    [[=schema::optional()]]
    bool enabled { true };
    [[=schema::optional()]]
    SceneFogTexture texture;
    [[=schema::optional()]]
    SceneFogControlField control_field;
};
/// scene.json 中的点光源：火把/灯笼等局部照明。
struct SceneLight {
    std::string tag;
    SceneVec pos;
    [[=schema::optional()]]
    SceneVec color { 1.0f, 0.48f, 0.16f };
    [[=schema::optional()]]
    float intensity { 30.0f };
    [[=schema::optional()]]
    float range { 12.0f };
    [[=schema::optional()]]
    float radius { 0.08f };
    [[=schema::optional()]]
    bool enabled { true };
    [[=schema::optional()]]
    bool shadow { true };
};
/// scene.json 顶层：材质表、实例块、dragon 摆放和可选局部雾/点光实体。
struct SceneDesc {
    std::unordered_map<std::string, SceneMaterial> materials;
    std::vector<SceneEntry> cubes;
    SceneEntry dragon;
    [[=schema::optional()]]
    std::vector<SceneFogVolume> fog_volumes;
    [[=schema::optional()]]
    std::vector<SceneLight> lights;
};


constexpr unsigned max_local_lights = 8;
static_assert(max_local_lights == 8);
/// 场景预设数量：0 默认，1-3 原有雾景，4-7 新增动态云海。
constexpr int scene_preset_count = 8;
/// 每帧最多为 N 盏点光渲染 cube shadow（按相机距离挑选），其余点光只照明。
constexpr unsigned max_shadow_lights = 5;
constexpr unsigned point_shadow_faces = 6;
constexpr unsigned point_shadow_resolution = 512;

struct alignas(16) CameraFogUbo {
    glm::mat4 view;
    glm::mat4 proj;
    /// RGB 雾气颜色
    glm::vec4 fog_color;
    /// x: 可选近端裁剪 ; y: 相机视锥远端（网格覆盖）; z: sigma_mist ; w: 噪声扰动强度
    glm::vec4 fog_params; 
    glm::vec4 camera_position;
    /// xyz = 指向太阳，w = ambient
    glm::vec4 light_direction;
    /// rgb = radiance，w = specular 开关
    glm::vec4 light_color;
    /// x: 高度衰减 ; y: 附加密度参考面高度 ; z: 噪声频率 ; w: 全空间基础密度 sigma_base
    glm::vec4 fog_extra;
    /// x: 动画相位 ; y: 动画幅度 ; z: 行波频率 ; w: 全局近端裁剪开关
    glm::vec4 fog_anim;
    glm::mat4 inverse_view_proj;
    /// xyz = grid dimensions, w = logarithmic depth mapping scale (world units).
    glm::vec4 froxel_grid;
    glm::mat4 light_view_proj[2];
    glm::mat4 previous_view;
    glm::mat4 previous_view_proj;
    /// enabled, depth bias, world-space normal offset, spot enabled
    glm::vec4 shadow_params;
    /// x: local light count, y: shadowed light count, z: lighting enabled, w: shadows enabled
    glm::vec4 local_light_info;
    /// 局部点光：xyz 位置, w 范围
    glm::vec4 local_light_position_range[8];
    /// rgb 颜色, w 强度（0 关闭）
    glm::vec4 local_light_color_intensity[8];
    /// x: 发射体半径, y: cube shadow base layer（<0 无阴影）, z: enabled, w: reserved
    glm::vec4 local_light_params[8];
    glm::vec4 volume_params; // anisotropy, scattering albedo, sunlight medium path, light steps
    glm::vec4 fog_shape; // weather, layer center, layer thickness, wind speed
    glm::vec4 temporal; // history weight, valid, frame sample, debug mode
    /// x: active count, y: near-light surface refinement, z: reserved, w: mascot fog veil（忽略太阳阴影）
    glm::vec4 local_fog_info;
    glm::vec4 display_params; // exposure EV, tone mapping enabled, manual sRGB encoding, reserved
    /// x: 表面动画时间（秒）, y: 风吹树叶强度, z: 水面法线强度, w: reserved
    glm::vec4 surface_params;
};
static_assert(ave::is_std430_compatible_v<CameraFogUbo>);
static_assert(offsetof(CameraFogUbo, inverse_view_proj) == 240);
static_assert(offsetof(CameraFogUbo, froxel_grid) == 304);
static_assert(offsetof(CameraFogUbo, light_view_proj) == 320);
static_assert(offsetof(CameraFogUbo, local_light_info) == 592);
static_assert(offsetof(CameraFogUbo, local_light_position_range) == 608);
static_assert(offsetof(CameraFogUbo, local_light_color_intensity) == 736);
static_assert(offsetof(CameraFogUbo, local_light_params) == 864);
static_assert(offsetof(CameraFogUbo, temporal) == 1024);
static_assert(offsetof(CameraFogUbo, local_fog_info) == 1040);
static_assert(offsetof(CameraFogUbo, display_params) == 1056);
static_assert(offsetof(CameraFogUbo, surface_params) == 1072);
static_assert(sizeof(CameraFogUbo) == 1088);

constexpr unsigned max_local_fog_volumes = 16;
constexpr unsigned max_local_fog_textures = 4;
constexpr unsigned fog_texture_size = 32;
constexpr unsigned fog_texture_tile = fog_texture_size + 2;

struct alignas(16) GpuFogVolume {
    glm::mat4 world_to_local { 1.0f };
    glm::vec4 scattering { 0.0f }; // RGB color, albedo
    glm::vec4 params { 0.0f }; // density, edge fade, shape, texture strength
    glm::vec4 tex_tiling { 0.0f }; // xyz, repeat
    glm::vec4 tex_offset { 0.0f };
    glm::vec4 atlas_offset { 0.0f }; // xyz, textured
    glm::vec4 atlas_scale { 0.0f }; // xyz texture scale, w control-density slot + 1 (zero = legacy)
    glm::vec4 bounds { 0.0f }; // world-space center, conservative sphere radius
};
static_assert(ave::is_std430_compatible_v<GpuFogVolume>);
static_assert(sizeof(GpuFogVolume) == 176);
struct alignas(16) LocalFogUpload {
    glm::uvec4 counts { 0u }; // active volume count, control-field slot mask, reserved
    std::array<GpuFogVolume, max_local_fog_volumes> entries {};
};
static_assert(offsetof(LocalFogUpload, entries) == 16);
static_assert(sizeof(LocalFogUpload) == 2832);

constexpr unsigned fog_control_grid_size = 64;
constexpr VkDeviceSize fog_control_grid_bytes = VkDeviceSize(fog_control_grid_size)
    * fog_control_grid_size * fog_control_grid_size * max_local_fog_volumes * sizeof(float);

struct alignas(16) GpuFogControlPoint {
    glm::vec4 position_radius { 0.0f };
    glm::vec4 density_softness { 0.0f };
};
struct alignas(16) GpuFogControlField {
    glm::uvec4 info { 0u }; // point count, seed, octaves, reserved
    glm::vec4 noise { 0.0f }; // scale, strength, reserved
    glm::vec4 noise_offset { 0.0f }; // periodic displacement
    std::array<GpuFogControlPoint, fog::max_fog_control_points> points {};
};
static_assert(sizeof(GpuFogControlPoint) == 32);
static_assert(offsetof(GpuFogControlField, points) == 48);
static_assert(sizeof(GpuFogControlField) == 1072);
using FogControlUpload = std::array<GpuFogControlField, max_local_fog_volumes>;
static_assert(sizeof(FogControlUpload) == 17152);
using FogControlRevisions = std::array<std::uint64_t, max_local_fog_volumes>;

struct FogTextureKey {
    std::string path;
    std::array<int, 3> extent;
    bool operator==(const FogTextureKey&) const = default;
};

/// Desktop runtime controls; independent from the existing on-disk config schema.
struct VolumeSettings {
    int pixel_size { 8 };
    int depth_slices { 64 };
    bool temporal { true };
    bool jitter { true };
    float history_weight { 0.90f };
    float anisotropy { 0.45f };
    float scattering_albedo { 0.90f };
    int light_steps { 4 };
    float sunlight_path { 40.0f };
    int weather { 0 }; // 0 custom, 1 dense, 2 patchy, 3 elevated layer
    float layer_center { 18.0f };
    float layer_thickness { 5.0f };
    float wind_speed { 0.35f };
    bool shadows { true };
    float shadow_bias { 0.001f };
    float normal_bias { 0.035f };
    float sun_extent { 48.0f };
    /// 表面着色 PCF 半径：0 = 单 tap 硬阴影，2 = 5×5（桌面默认）。
    /// 桌面保持 5×5；移动端在启动时由 fog-android 的 jni_bridge 置 0，
    /// 因为 5 个投影点光 × 25 tap + 太阳 25 tap = 150 次阴影采样/fragment，
    /// 是移动端 scene+UI 的主要开销。经 CameraFogUbo::shadow_params.w 下发。
    int surface_shadow_radius { 2 };
    int debug { 0 }; // 0 composite, 1 T, 2 scattering, 3 sun shadow
    // Experimental per-pixel integration is expensive and has no temporal history.
    // Opt in only; it must not be mixed with jittered froxel prefixes.
    bool surface_refinement { false };
    float exposure_ev { 0.0f };
    bool tone_mapping { true };
    bool near_cutoff { false };
    /// 树叶摇摆强度与水面法线强度（UI 可调）。
    float surface_wind { 1.0f };
    float water_normal { 1.0f };
    /// 水面 alpha；1 为不透明，0.68 左右能看见河床。
    float water_alpha { 0.60f };
};

/// 点光：火把/灯笼等；位置与颜色来自 scene.json，可在 UI 中开关与调参。
struct LocalLight {
    std::string tag;
    glm::vec3 position { 0.0f };
    glm::vec3 color { 1.0f };
    float intensity { 0.0f };
    float range { 12.0f };
    float radius { 0.08f };
    bool enabled { true };
    bool cast_shadow { true };
};

struct FogPresetVolume {
    std::string tag;
    fog::Transform transform;
    fog::FogVolume medium;
    std::optional<fog::FogDensityTexture> texture;
    std::optional<fog::FogControlField> controls;
};

/// In-session tab snapshots, including edits and animation phase, never scene geometry.
struct FogScenePresetState {
    AppConfig::Fog global;
    glm::vec3 fog_color { 1.0f };
    float fog_start { 0.0f };
    VolumeSettings volume;
    fog::DirectionalLight sun;
    std::vector<LocalLight> lights;
    bool local_lights_enabled { true };
    bool local_light_shadows { true };
    float sky_exposure { 1.0f };
    float phase { 0.0f };
    bool paused { false };
    std::vector<FogPresetVolume> locals;
};

struct VolumeHistory {
    std::optional<CameraFogUbo> camera;
    std::uint64_t epoch { 0 };
    std::uint64_t frame { 0 };
    double time { 0.0 };
    unsigned read_index { 0 };
};

struct ShadowCache {
    glm::mat4 projection { 1.0f };
    std::uint64_t geometry_revision { 0 };
    bool enabled { false };
    bool valid { false };
    bool reusable(const glm::mat4& next, std::uint64_t revision, bool active) const {
        return valid && enabled == active && (!active || (geometry_revision == revision && projection == next));
    }
};

/// 单个点光的 cube shadow 缓存：位置/范围/几何变化才重绘。
struct PointShadowCache {
    glm::vec3 position { 0.0f };
    float range { 0.0f };
    std::uint64_t geometry_revision { 0 };
    bool valid { false };
    bool reusable(unsigned light, glm::vec3 next_position, float next_range,
                  std::uint64_t revision) const {
        return valid && light == shadow_light && geometry_revision == revision
            && position == next_position && range == next_range;
    }
    unsigned shadow_light { max_local_lights };
};

struct FogGpuQueries {
    FogGpuQueries() = default;
    FogGpuQueries(const FogGpuQueries&) = delete;
    FogGpuQueries& operator=(const FogGpuQueries&) = delete;
    std::shared_ptr<ave::TimestampQueryPool> pool;
    std::vector<bool> submitted;
};

/// 答辩吉祥物截帧：只画 dragon，透明背景，渲染若干帧后把交换链图像读回 TGA。
struct MascotCapture {
    bool enabled { false };
    std::string output;
    /// 绕 dragon 中心的水平环绕角（度）与抬升角（度）
    float orbit_deg { 40.0f };
    float elevation_deg { 12.0f };
    float distance { 8.5f };
    float fov_deg { 38.0f };
    /// 时域雾需要几帧收敛；最后一帧写盘
    int frames { 20 };
    /// 是否在龙体外再画一层局部雾薄纱（默认只保留龙体上的部分遮挡）
    bool halo { false };
    glm::vec3 target { -0.06f, 2.41f, -0.97f };
};

/// 每个 draw 一个 model 矩阵（push constant）
struct alignas(16) PushConstant {
    glm::mat4 model;
    /// solid、specular、shininess、voxel 类型
    glm::vec4 params { 0.0f };
    /// rgb = 颜色，w = emission
    glm::vec4 base_color { 1.0f };
    /// x = 方块法线图层（0 = 关闭）
    glm::vec4 material_params { 0.0f };
};
static_assert(ave::is_std430_compatible_v<PushConstant>);
static_assert(sizeof(PushConstant) == 112);

struct alignas(16) SkyPushConstant {
    /// x = exposure，y = 是否使用全景图
    glm::vec4 params; 
};
static_assert(ave::is_std430_compatible_v<SkyPushConstant>);


inline PushConstant make_draw_data(fog::Transform& transform, const fog::Renderable& renderable,
                                  const fog::SurfaceMaterial& material) {
    return {
        .model = transform.build_model_matrix(),
        .params = glm::vec4(renderable.solid ? 1.0f : 0.0f, material.specular,
                            material.shininess, static_cast<float>(material.voxel)),
        .base_color = glm::vec4(material.color, material.emission),
        .material_params = glm::vec4(static_cast<float>(material.normal_layer), 0.0f, 0.0f, 0.0f)
    };
}

/// 桌面端外部控制通道：设 FOG_CONTROL=<unix socket 路径>（或 "1" = /tmp/fog-control-<pid>.sock）
/// 后 run() 会起一个监听线程，一行一条命令：
///   ping                -> pong（等待监听就绪）
///   quit                -> bye，主循环下一帧干净退出，照常打印 FPS Detective 报告
///   shot <path>         -> ok，主循环在安全点把交换链图像读回写成 TGA
///   ui on|off           -> ok，切换 ImGui 界面显隐（等价于 F1）
///   cam x y z [yaw [pitch]] -> ok，把相机摆到指定位姿（角度单位：度）
///   preset <0..7>       -> ok，切换场景预设（7 = 月夜流雾）
/// 未设置 FOG_CONTROL 时完全不启动，行为与之前一致（Wayland 下无需注入按键即可测量/截帧）。
struct ControlChannel {
    /// 监听线程请求的退出（主循环每帧检查，等价于 ESC）
    std::atomic<bool> quit_requested { false };
    /// 实际监听路径；空 = 未启用
    std::string socket_path;

    /// 读取 FOG_CONTROL 并启动监听；未设置时直接返回
    void start();
    /// 请求线程停止并清理套接字文件
    void stop();
    /// 取走待执行的截帧路径；有请求返回 true
    [[nodiscard]] bool consume_shot(std::string& path);
    /// 取走待应用的界面显隐；有请求返回 true
    [[nodiscard]] bool consume_ui(bool& visible);
    /// 取走待应用的相机位姿描述；有请求返回 true
    [[nodiscard]] bool consume_camera(std::string& spec);
    /// 取走待切换的场景预设索引；有请求返回 true
    [[nodiscard]] bool consume_preset(int& preset);
    /// 是否有退出请求
    [[nodiscard]] bool quit() const { return quit_requested.load(std::memory_order_relaxed); }

private:
    /// 待执行请求：Vulkan/场景状态只能在主线程改，故这里只记录
    std::mutex request_mutex;
    std::string pending_shot;
    std::optional<bool> pending_ui;
    std::optional<std::string> pending_camera;
    std::optional<int> pending_preset;
    bool shot_pending { false };
    /// 监听线程（不可 join = 未启用或已停止）
    std::jthread listener;

#ifndef __ANDROID__
    void serve(int listen_fd, std::stop_token token);
    void handle_client(int client);
#endif
};

struct App {
    friend int check_editor();
    AppConfig cfg;

    //// 基础 Infra ////
    /// 日志
    alib5::Logger logger;
    alib5::LogFactory lg;
    /// Vulkan 校验层日志
    alib5::LogFactory vklg;

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
    ave::VMABufferSlice shape_vertices;
    ave::VMABufferSlice shape_indices;
    std::uint32_t shape_index_count { 0 };
    // acquire 等待对应图像的 fence 后才能更新，避免覆盖 GPU 正在读取的 UBO。
    std::vector<std::unique_ptr<ave::VMABuffer>> ubo_buffers;
    std::vector<std::unique_ptr<ave::VMABuffer>> local_fog_buffers;
    std::vector<std::unique_ptr<ave::VMABuffer>> control_fog_buffers;
    std::vector<std::unique_ptr<ave::VMABuffer>> control_density_buffers;
    FogControlUpload control_fog_snapshot {};
    std::vector<std::pair<std::uint64_t, fog::FogControlField>> control_fog_settings;
    FogControlRevisions control_fog_revisions {};
    std::vector<FogControlRevisions> generated_control_fog;
    std::vector<FogControlRevisions> uploaded_control_fog;
    std::shared_ptr<ave::ComputePipeline> control_fog_pipeline;
    std::vector<VkDescriptorSet> descriptor_control_fog;
    std::shared_ptr<ave::Image> local_fog_atlas;
    std::shared_ptr<ave::Sampler> local_fog_sampler;
    std::vector<FogTextureKey> local_fog_textures;
    std::optional<LocalFogUpload> local_fog_snapshot;
    std::vector<std::optional<CameraFogUbo>> uploaded_ubos;
    std::vector<std::uint64_t> uploaded_local_fog;
    std::uint64_t local_fog_revision { 1 };
    std::optional<ave::VMABuffer> instance_buffer;
    std::uint32_t instance_count { 0 };
    /// 水面单独一批，在不透明场景之后做 alpha 混合。
    std::optional<ave::VMABuffer> water_instance_buffer;
    std::uint32_t water_instance_count { 0 };
    std::shared_ptr<ave::Image> cube_texture;
    std::shared_ptr<ave::Image> water_normal_texture;
    /// 0=草顶 1=草侧 2=石 3=木 4=叶 5=沙 6=瓦，对应 binding 16..22。
    std::array<std::shared_ptr<ave::Image>, 7> block_normal_textures;
    std::shared_ptr<ave::Sampler> sampler;
    std::shared_ptr<ave::Sampler> water_normal_sampler;
    std::shared_ptr<ave::Image> sky_texture;
    std::shared_ptr<ave::Sampler> sky_sampler;
    std::shared_ptr<ave::Pipeline> pipeline;
    std::shared_ptr<ave::Pipeline> instanced_pipeline;
    std::shared_ptr<ave::Pipeline> water_pipeline;
    std::shared_ptr<ave::Pipeline> sky_pipeline;
    std::shared_ptr<ave::ComputePipeline> froxel_pipeline;
    std::shared_ptr<ave::ComputePipeline> temporal_pipeline;
    std::shared_ptr<ave::ComputePipeline> integrate_pipeline;
    std::vector<std::unique_ptr<ave::VMABuffer>> froxel_buffers;
    std::vector<std::unique_ptr<ave::VMABuffer>> injection_buffers;
    std::vector<std::array<std::unique_ptr<ave::VMABuffer>, 2>> history_buffers;
    std::vector<VkDescriptorSet> descriptor_froxel;
    std::vector<std::array<VkDescriptorSet, 2>> descriptor_temporal;
    std::vector<std::array<VkDescriptorSet, 2>> descriptor_integrate;
    std::vector<VolumeHistory> volume_history;
    glm::uvec2 froxel_extent { 1u, 1u };
    unsigned froxel_depth { 64 };
    /// 太阳阴影矩阵：CPU 侧同时用于渲染与 UBO。
    glm::mat4 sun_shadow_matrix { 1.0f };
    std::array<std::shared_ptr<ave::LegacyRender>, 1> shadow_renders;
    std::array<std::vector<std::shared_ptr<ave::Image>>, 1> shadow_images;
    std::shared_ptr<ave::Pipeline> shadow_pipeline;
    std::shared_ptr<ave::Pipeline> shadow_instanced_pipeline;
    std::shared_ptr<ave::Sampler> shadow_sampler;
    std::vector<VkDescriptorSet> descriptor_shadow;
    std::vector<std::array<ShadowCache, 1>> shadow_cache;
    /// 点光 cube shadow：每张交换链图像一份 2D array（6*max_shadow_lights 层）。
    std::shared_ptr<ave::Sampler> point_shadow_sampler;
    std::vector<std::shared_ptr<ave::Image>> point_shadow_images;
    std::vector<std::shared_ptr<ave::ImageView>> point_shadow_array_views;
    std::vector<std::vector<std::shared_ptr<ave::ImageView>>> point_shadow_layer_views;
    std::array<std::array<std::shared_ptr<ave::LegacyRender>, point_shadow_faces>, max_shadow_lights>
        point_shadow_renders;
    std::vector<std::array<PointShadowCache, max_shadow_lights>> point_shadow_cache;
    /// 每张交换链图、每个 slot 的 cube 图是否已经清过（允许无灯时也进入可采样布局）。
    std::vector<std::array<bool, max_shadow_lights>> point_shadow_cleared;
    /// 每帧选出的、有阴影预算的点光索引；其余索引为 max_local_lights。
    std::array<unsigned, max_shadow_lights> shadowed_lights = [] {
        std::array<unsigned, max_shadow_lights> value {};
        value.fill(max_local_lights);
        return value;
    }();
    /// froxel 级光源位置切分：每个 cell 一个 8-bit 光源掩码。
    std::shared_ptr<ave::ComputePipeline> light_cull_pipeline;
    std::vector<std::unique_ptr<ave::VMABuffer>> light_mask_buffers;
    std::vector<VkDescriptorSet> descriptor_light_cull;
    std::uint64_t geometry_revision { 1 };
    std::unique_ptr<FogGpuQueries> gpu_queries;
    std::shared_ptr<ave::DescriptorPool> scene_descriptor_pool;
    std::vector<VkDescriptorSet> descriptor_cubes;
    std::vector<VkDescriptorSet> descriptor_water;
    std::vector<VkDescriptorSet> descriptor_sky;
    std::optional<ave::Renderer> renderer;

    //// ECS ////
    /// 实体管理器
    alib5::ecs::EntityManager em;
    /// 相机实体包装
    fog::CameraEntity camera { em };
    /// 场景唯一的天空盒实体（依赖 setup_geometry 出来的 cube 切片，故 setup_world 里创建）
    std::optional<fog::SkyboxEntity> skybox;
    alib5::ecs::EntityWrapper sunlight { em };
    alib5::ecs::ref_t<fog::DirectionalLight> sunlight_ref { sunlight.add<fog::DirectionalLight>() };
    /// 全局雾实体包装
    fog::FogEntity fog {
        em,
        glm::vec3(cfg.fog.r, cfg.fog.g, cfg.fog.b),
        cfg.fog.start
    };

    /// 界面开关（F1 切换）；ImGui 本体在经典 TU 里（见 ui_bridge.h）
    bool ui_visible { true };
#ifdef __ANDROID__
    // Explicit ADB diagnostics only; normal Android and desktop rendering are unchanged.
    bool android_benchmark { false };
    bool android_no_volume { false };
    /// Spawn one control-field local fog after setup for repeatable ADB benchmark runs.
    bool android_local_mist { false };
    /// Public diagnostic entry so the JNI bridge does not need access to private helpers.
    void spawn_diagnostic_local_mist() { spawn_local_fog(2); }
#endif
    /// 非交互截帧模式（FOG_MASCOT=1）
    MascotCapture mascot;
    /// 桌面端外部控制通道（FOG_CONTROL），见 ControlChannel 注释
    ControlChannel control;
    /// Inspector 改动过实例部件 → 下一帧重建实例批
    bool instances_dirty { false };
    bool inspector_dirty { false };
    /// 雾动画的累计相位（弧度，按 2pi/周期 累加，避免改周期时相位跳变）
    float fog_phase { 0.0f };
    bool animation_paused { false };
    bool animation_step { false };
    VolumeSettings volume;
    float surface_time { 0.0f };
    std::vector<LocalLight> local_lights;
    bool local_lights_enabled { true };
    bool local_light_shadows { true };
    int applied_weather { 0 };
    std::uint64_t history_epoch { 1 };
    std::uint64_t render_frame { 0 };
    std::chrono::steady_clock::time_point render_start { std::chrono::steady_clock::now() };
    std::string volume_status;
    std::string gpu_status { "GPU timestamps: waiting for completed frame" };
    int local_fog_action { 0 };
    std::uint64_t local_fog_target { 0 };
    std::string editor_status { "局部雾存档：fog-local.json（不会修改 scene.json）" };
    unsigned local_fog_serial { 0 };
    int scene_preset { 0 };
    std::array<std::optional<FogScenePresetState>, scene_preset_count> scene_presets;

    App(const AppConfig & cfg);
    ~App();
    void setup();
    int run();
    /// 控制通道用：切换场景预设（0..scene_preset_count-1，7 = 月夜流雾）
    void apply_scene_preset(int preset) { switch_scene_preset(preset); }
    /// 固定机位渲染 mascot，读回 RGBA 并写 TGA（透明背景）
    int run_mascot();
    void setup_mascot_camera();
    bool capture_swapchain_tga(const std::string& path);

private:
    void setup_create_window();
    void setup_renderer();
    void setup_geometry();
    void setup_pipeline();
    void setup_ubo();
    void setup_shadows();
    void record_shadows(VkCommandBuffer commands, unsigned image);
    void record_point_shadows(VkCommandBuffer commands, unsigned image);
    void record_volume(VkCommandBuffer commands, unsigned image, unsigned output);
    void read_gpu_timings(unsigned image);
    void apply_weather();
    void sync_local_fog();
    void rebuild_local_fog_atlas(const std::vector<FogTextureKey>& textures);
    void spawn_local_fog(int shape);
    void switch_scene_preset(int preset);
    void edit_local_fog(int action);
    void save_local_fog(const std::filesystem::path& path = "fog-local.json");
    void load_local_fog(const std::filesystem::path& path = "fog-local.json");
    void setup_textures();
    void setup_world();
    void setup_instances();
    void update_camera(float dt);
    void update_environment(float dt);
    void render_scene();
    void setup_imgui();
    /// 每渲染帧：起帧 + 把 ECS 字段填进数据桥 + 构建界面
    void ui_frame();
    /// 应用控制通道下发的相机位姿 "x y z [yaw [pitch]]"（角度单位：度）
    void apply_control_camera(std::string_view spec);
    /// 在当前渲染作用域内录制界面
    void render_ui(void* command_buffer);
    /// 是否允许移动视野：鼠标已按 M 锁定，且界面没在占用鼠标
    [[nodiscard]] bool camera_active() const;
    /// 实例批重建（Inspector 改了 Transform / SurfaceMaterial 时用）
    void refresh_instances();
};


//// 提供日志便利 ////
constexpr auto lc_quote = alib5::lot::color(
    alib5::lot::Color::Cyan
);
constexpr auto lc_reset = alib5::lot::color(
    alib5::lot::Color::None
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
    return alib5::to_adata(std::forward<T>(value)).str(
        alib5::data::Flat()
    );
}
