/**
 * @file app_ui.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 模块侧的界面接线：把 ECS 组件填成 ui_bridge 的字段描述表，并驱动 ImGui 后端
 *
 * 本 TU 是"模块世界"（import std/ave），只 include ui_bridge.h 这个纯 POD 头，
 * 不碰 imgui 的经典头 —— 原因见 ui_bridge.h 顶部说明。
 * @version 5.0
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 * @parblock audit AI不准修改 
 * 1. OK 10-14 都是些基础实现，能出错我吃（当我没说）
 * @endparblock
 * 
 */
#include "app.h"
#include "ui_bridge.h"
#include <deque>
#include <glm/gtc/constants.hpp>

namespace {

    namespace ui = fog::ui;

    constexpr const char* preset_names[] = {
        "默认", "局部白雾", "贴地晨雾", "暖光迷雾",
        "日出云海", "风暴云海", "黄昏云河", "月夜流雾"
    };

    //// 字段表：每渲染帧重建；Frame 只持有指针，容器本身活过一帧 ////
    std::vector<ui::Field> g_globals;
    std::vector<ui::Field> g_fields;
    std::vector<ui::Entity> g_entities;
    std::deque<std::string> g_field_groups;
    std::deque<std::pair<alib5::ecs::Entity, fog::FogControlField>> g_control_edits;
    char g_filter[64] {};

    //// 声明式登记：字段用花括号表排（{标签, 目标, 步长, 下限, 上限}），不再成片 push_back ////

    /// 浮点字段的一行
    struct FloatRow {
        const char* label;
        float* value;
        float speed;
        float lo;
        float hi;
    };

    void add_floats(std::vector<ui::Field>& out, const char* group, std::initializer_list<FloatRow> rows) {
        out.reserve(out.size() + rows.size());
        for(const FloatRow& row : rows) {
            out.push_back(ui::Field{ .group = group, .label = row.label, .value = row.value,
                                     .speed = row.speed, .lo = row.lo, .hi = row.hi,
                                     .kind = ui::Kind::Float });
        }
    }

    void add_color(std::vector<ui::Field>& out, const char* group, const char* label, glm::vec3& color) {
        out.push_back(ui::Field{ .group = group, .label = label, .value = &color.x,
                                 .kind = ui::Kind::Color3 });
    }

    void add_bool(std::vector<ui::Field>& out, const char* group, const char* label, bool* value) {
        out.push_back(ui::Field{ .group = group, .label = label, .value = value,
                                 .kind = ui::Kind::Bool });
    }

    void add_int(std::vector<ui::Field>& out, const char* group, const char* label, int* value,
                 float speed, int lo, int hi) {
        out.push_back(ui::Field{ .group = group, .label = label, .value = value, .speed = speed,
                                 .lo = static_cast<float>(lo), .hi = static_cast<float>(hi),
                                 .kind = ui::Kind::Int });
    }

    /// 点光标签池：ui_frame 重建字段时保持 c_str 稳定。
    std::vector<std::string> g_light_labels;

    /// 只读一行（检查器里标注组件用）
    void add_note(std::vector<ui::Field>& out, const char* text) {
        out.push_back(ui::Field{ .text = text, .kind = ui::Kind::Text });
    }

    /// 带分组名的只读一行：面板按分组过滤，没有分组名的 note 会被跳过
    void add_group_note(std::vector<ui::Field>& out, const char* group, const char* text) {
        out.push_back(ui::Field{ .group = group, .text = text, .kind = ui::Kind::Text });
    }

    //// 每个组件一份登记函数，主循环里一行一个组件 ////

    void register_transform(std::vector<ui::Field>& out, fog::Transform& transform) {
        add_floats(out, "Transform", {
            { "position x", &transform.position.x, 0.05f, -200.0f, 200.0f },
            { "position y", &transform.position.y, 0.05f, -200.0f, 200.0f },
            { "position z", &transform.position.z, 0.05f, -200.0f, 200.0f },
            { "scale x", &transform.scale.x, 0.05f, 0.01f, 64.0f },
            { "scale y", &transform.scale.y, 0.05f, 0.01f, 64.0f },
            { "scale z", &transform.scale.z, 0.05f, 0.01f, 64.0f },
        });
    }

    void register_surface_material(std::vector<ui::Field>& out, fog::SurfaceMaterial& material) {
        add_color(out, "SurfaceMaterial", "color", material.color);
        add_floats(out, "SurfaceMaterial", {
            { "specular", &material.specular, 0.005f, 0.0f, 1.0f },
            { "shininess", &material.shininess, 0.5f, 1.0f, 512.0f },
            { "emission", &material.emission, 0.01f, 0.0f, 8.0f },
        });
        add_int(out, "SurfaceMaterial", "voxel", &material.voxel, 0.1f, 0, 8);
    }

    void register_renderable(std::vector<ui::Field>& out, const fog::Renderable& renderable) {
        add_note(out, renderable.index_count == 36 ? "Renderable: 共享 cube 几何（走实例批）"
                                                   : "Renderable: 独立几何");
        add_note(out, renderable.solid ? "solid: true" : "solid: false");
    }

    void register_camera_tilt(std::vector<ui::Field>& out, fog::CameraTilt& tilt) {
        add_floats(out, "CameraTilt", {
            { "yaw", &tilt.yaw_deg, 0.5f, -360.0f, 360.0f },
            { "pitch", &tilt.pitch_deg, 0.5f, -89.0f, 89.0f },
            { "sensitivity", &tilt.sensitivity, 0.001f, 0.001f, 1.0f },
        });
    }

    void register_projector(std::vector<ui::Field>& out, fog::Projector& projector) {
        add_floats(out, "Projector", {
            { "fov", &projector.fov_rad, 0.01f, 0.17f, 2.1f },
            { "near", &projector.near_plane, 0.01f, 0.001f, 10.0f },
            { "far", &projector.far_plane, 1.0f, 1.0f, 2000.0f },
        });
    }

    void register_fog_params(std::vector<ui::Field>& out, fog::FogParams& params) {
        add_color(out, "FogParams", "color", params.color);
        add_floats(out, "FogParams", {
            { "start", &params.start, 0.25f, 0.0f, 400.0f },
        });
    }

    void register_skybox(std::vector<ui::Field>& out, fog::Skybox& sky) {
        add_floats(out, "Skybox", { { "exposure", &sky.exposure, 0.02f, 0.0f, 4.0f } });
    }

    void register_directional_light(std::vector<ui::Field>& out, fog::DirectionalLight& light) {
        add_color(out, "DirectionalLight", "color", light.color);
        add_floats(out, "DirectionalLight", { { "ambient", &light.ambient, 0.005f, 0.0f, 1.0f } });
        add_bool(out, "DirectionalLight", "specular", &light.specular);
    }

    void register_local_fog(std::vector<ui::Field>& out, fog::FogVolume& medium) {
        add_bool(out, "FogVolume", "enabled", &medium.enabled);
        add_int(out, "FogVolume", "shape (0 box / 1 ellipsoid)", &medium.shape, 0.1f, 0, 1);
        add_color(out, "FogVolume", "scattering color", medium.color);
        add_floats(out, "FogVolume", {
            { "density", &medium.density, 0.005f, 0.0f, 3.0f },
            { "albedo", &medium.albedo, 0.005f, 0.0f, 1.0f },
            { "edge fade (normalized)", &medium.edge_fade, 0.005f, 0.0f, 1.0f },
        });
    }

    void register_control_field(std::vector<ui::Field>& out, fog::FogControlField& field) {
        const auto first = out.size();
        constexpr auto group = "FogControlField";
        add_bool(out, group, "enabled", &field.enabled);
        add_bool(out, group, "animated", &field.animated);
        add_floats(out, group, {
            {"period (seconds)", &field.period, 0.1f, 0.1f, 900.0f},
            {"noise scale", &field.noise_scale, 0.05f, 0.1f, 32.0f},
            {"noise strength", &field.noise_strength, 0.01f, 0.0f, 1.0f},
        });
        add_int(out, group, "octaves", &field.octaves, 0.1f, 1, 5);
        add_int(out, group, "seed", &field.seed, 1.0f, 0, 65535);
        for(std::size_t i = 0; i < field.points.size(); ++i) {
            auto& p = field.points[i];
            // deque keeps c_str addresses stable as later entities/groups are registered.
            g_field_groups.push_back(std::format("Control point {}", i));
            const auto* name = g_field_groups.back().c_str();
            add_floats(out, name, {
                {"position x", &p.position.x, 0.005f, 0, 1},
                {"position y", &p.position.y, 0.005f, 0, 1},
                {"position z", &p.position.z, 0.005f, 0, 1},
                {"radius", &p.radius, 0.005f, 0.02f, 1},
                {"density", &p.density, 0.01f, 0, 4},
                {"softness", &p.softness, 0.005f, 0.05f, 1},
                {"amplitude x", &p.amplitude.x, 0.005f, 0, 0.5f},
                {"amplitude y", &p.amplitude.y, 0.005f, 0, 0.5f},
                {"amplitude z", &p.amplitude.z, 0.005f, 0, 0.5f},
                {"phase", &p.phase, 0.01f, 0, glm::two_pi<float>()},
            });
            add_int(out, name, "frequency", &p.frequency, 0.1f, 1, 8);
        }
        for(auto i = first; i < out.size(); ++i) out[i].clamp_value = true;
    }

    void register_fog_texture(std::vector<ui::Field>& out, fog::FogDensityTexture& texture) {
        out.push_back(ui::Field{ .group = "FogDensityTexture", .text = texture.path.c_str(), .kind = ui::Kind::Text });
        add_bool(out, "FogDensityTexture", "repeat UV", &texture.repeat);
        add_floats(out, "FogDensityTexture", {
            { "texture strength", &texture.strength, 0.005f, 0.0f, 1.0f },
            { "tiling x", &texture.tiling.x, 0.01f, 0.01f, 16.0f },
            { "tiling y", &texture.tiling.y, 0.01f, 0.01f, 16.0f },
            { "tiling z", &texture.tiling.z, 0.01f, 0.01f, 16.0f },
            { "UV offset x", &texture.offset.x, 0.01f, -16.0f, 16.0f },
            { "UV offset y", &texture.offset.y, 0.01f, -16.0f, 16.0f },
            { "UV offset z", &texture.offset.z, 0.01f, -16.0f, 16.0f },
        });
    }

} // namespace

//// 后端初始化 ////
auto App::setup_imgui() -> void {
    if(!renderer || !window) return;

    auto& renderer_ref = *renderer;
    const bool dynamic_rendering = static_cast<bool>(renderer_ref.dynamic_render);
    const auto& requests = renderer_ref.device->get_queue_requests();
    const std::uint32_t queue_family = requests.empty() ? 0u : requests.front().family_index;

    // 与 AVE 的 dynamic rendering 附件保持一致（legacy render pass 模式下由 render pass 决定，留 UNDEFINED）
    const std::int32_t depth_format = renderer_ref.dynamic_render
        ? static_cast<std::int32_t>(renderer_ref.dynamic_render->get_depth_attachment_format())
        : static_cast<std::int32_t>(VK_FORMAT_UNDEFINED);
    const std::int32_t stencil_format = renderer_ref.dynamic_render
        ? static_cast<std::int32_t>(renderer_ref.dynamic_render->get_stencil_attachment_format())
        : static_cast<std::int32_t>(VK_FORMAT_UNDEFINED);

    const ui::InitDesc desc {
#if defined(__ANDROID__)
        .platform_window = window->get_native_handle(),
#else
        .platform_window = window->get_system_handle(),
#endif
        .instance = renderer_ref.instance->get_system_handle(),
        .physical_device = renderer_ref.device->get_physical_device(),
        .device = renderer_ref.device->get_system_handle(),
        .queue_family = queue_family,
        .queue = renderer_ref.device->get_queue(queue_family),
        .allocator = renderer_ref.device->get_instance()->get_vk_allocator(),
        .color_format = static_cast<std::int32_t>(renderer_ref.swapchain->get_surface_format().format),
        .depth_format = depth_format,
        .stencil_format = stencil_format,
        .image_count = renderer_ref.swapchain->get_image_count(),
        .dynamic_rendering = dynamic_rendering,
        .legacy_render_pass = dynamic_rendering
            ? nullptr
            : static_cast<void*>(renderer_ref.legacy_render->get_render_pass()),
        .font_path = cfg.ui.font.c_str(),
        .scale = cfg.ui.scale,
    };

    if(!ui::backend_init(desc)) {
        vklg(alib5::Severity::Warn) << "ImGui init failed, UI disabled." << std::endl;
        ui_visible = false;
        return;
    }
    lg << "ImGui ready (F1 toggles the UI; menu bar switches Stats/Fog/Lighting/Sky/Inspector)."
       << std::endl;
}

//// 每帧：登记字段 + 构建界面 ////
auto App::ui_frame() -> void {
    if(!ui_visible) return;

    auto& fog_params = fog.params();
    auto& light = sunlight_ref.get();

    //// Controller：全局参数（雾密度场 / 雾 / 光照 / 天空）////
    g_globals.clear();
    g_light_labels.clear();
    g_light_labels.reserve(local_lights.size() * 5 + 4);
    add_floats(g_globals, "雾密度场", {
        { "全空间 sigma_base", &cfg.fog.density, 0.001f, 0.0f, 0.5f },
        { "近地面 sigma_mist", &cfg.fog.mist_density, 0.005f, 0.0f, 2.0f },
        { "高度衰减 k", &cfg.fog.height_falloff, 0.005f, 0.02f, 3.0f },
        { "参考面 y", &cfg.fog.ground_y, 0.05f, -20.0f, 20.0f },
        { "噪声频率", &cfg.fog.noise_scale, 0.002f, 0.001f, 1.0f },
        { "噪声强度", &cfg.fog.noise_amount, 0.01f, 0.0f, 1.0f },
        { "呼吸周期 s (Left 快 / Right 慢)", &cfg.fog.anim_period, 1.0f, 5.0f, 900.0f },
        { "动画幅度", &cfg.fog.anim_amplitude, 0.005f, 0.0f, 0.85f },
        { "行波频率", &cfg.fog.anim_frequency, 0.001f, 0.001f, 0.5f },
    });
    add_color(g_globals, "雾", "雾色", fog_params.color);
    add_bool(g_globals, "雾", "启用全局近端裁剪（默认关闭）", &volume.near_cutoff);
    if(volume.near_cutoff) {
        add_floats(g_globals, "雾", {
            { "近端裁剪距离 start", &fog_params.start, 0.25f, 0.0f, 400.0f },
        });
    } else {
        // 关闭时全局介质连续到相机，没有可编辑的积分终点。
        add_group_note(g_globals, "雾", "start 未启用：全局雾从相机连续起算，无近端空洞");
    }
    add_group_note(g_globals, "雾", "表面积分到物体；网格自动覆盖相机视锥；天空按光学收敛继续积分");
    add_int(g_globals, "体积雾·网格", "天气 0自定义 / 1浓雾 / 2团雾 / 3高空", &volume.weather, 0.1f, 0, 3);
    add_int(g_globals, "体积雾·网格", "像素步长（修改重建网格）", &volume.pixel_size, 0.1f, 4, 32);
    add_int(g_globals, "体积雾·网格", "深度切片（修改重建网格）", &volume.depth_slices, 0.1f, 16, 96);
    add_int(g_globals, "体积雾·网格", "调试 0合成 / 1透过率 / 2散射 / 3太阳阴影", &volume.debug, 0.1f, 0, 3);
    add_bool(g_globals, "体积雾·时域", "时域重投影", &volume.temporal);
    add_bool(g_globals, "体积雾·时域", "深度抖动采样", &volume.jitter);
    add_bool(g_globals, "体积雾·时域", "暂停雾动画", &animation_paused);
    add_floats(g_globals, "体积雾·时域", {
        { "历史权重（按历史年龄衰减）", &volume.history_weight, 0.005f, 0.0f, 0.97f },
    });
    add_floats(g_globals, "体积雾·散射", {
        { "各向异性 g", &volume.anisotropy, 0.005f, -0.85f, 0.85f },
        { "单次散射反照率", &volume.scattering_albedo, 0.005f, 0.0f, 1.0f },
        { "太阳介质衰减光程", &volume.sunlight_path, 0.5f, 0.0f, 100.0f },
    });
    add_int(g_globals, "体积雾·散射", "入射光介质步数（0关闭）", &volume.light_steps, 0.1f, 0, 8);
    add_bool(g_globals, "体积雾·边界", "实验：近灯表面精细积分（高开销，默认关闭）", &volume.surface_refinement);
    add_group_note(g_globals, "体积雾·边界", "通常使用 Froxel 注入与时域滤波；关闭纠偏可能保留近灯漏光");
#if defined(__ANDROID__)
    // The Android fragment shader is compiled without fog_surface.glsl: keeping the
    // general interval solver in the shader collapsed fill rate even when unused.
    add_group_note(g_globals, "体积雾·边界", "移动端未编译该路径：此开关在 Android 上无效");
#endif
    if(volume.surface_refinement && volume.jitter) {
        add_group_note(g_globals, "体积雾·边界", "纠偏已暂停：请先关闭深度抖动采样，避免混合前缀导致闪烁");
    }
    if(volume.weather == 3) {
        add_floats(g_globals, "体积雾·天气", {
            { "高空雾层中心 y", &volume.layer_center, 0.1f, -20.0f, 100.0f },
            { "高空雾层厚度", &volume.layer_thickness, 0.1f, 0.1f, 30.0f },
        });
    } else {
        add_group_note(g_globals, "体积雾·天气", "高空雾层参数仅在天气 = 3 时生效");
    }
    add_floats(g_globals, "体积雾·天气", {
        { "世界空间风速", &volume.wind_speed, 0.01f, 0.0f, 3.0f },
    });
    add_bool(g_globals, "体积雾·显示", "统一 HDR 色调映射", &volume.tone_mapping);
    add_floats(g_globals, "体积雾·显示", { { "统一曝光 EV", &volume.exposure_ev, 0.05f, -10.0f, 10.0f } });
    add_color(g_globals, "光照", "太阳色", light.color);
    add_floats(g_globals, "光照", {
        { "太阳方向 x", &light.direction.x, 0.01f, -1.0f, 1.0f },
        { "太阳方向 y", &light.direction.y, 0.01f, -1.0f, 1.0f },
        { "太阳方向 z", &light.direction.z, 0.01f, -1.0f, 1.0f },
        { "环境光", &light.ambient, 0.005f, 0.0f, 1.0f },
    });
    add_bool(g_globals, "光照", "高光", &light.specular);
    add_bool(g_globals, "阴影", "太阳与点光几何阴影", &volume.shadows);
    add_int(g_globals, "阴影", "表面 PCF 半径（0 硬阴影 / 2 是 5×5）", &volume.surface_shadow_radius, 0.1f, 0, 4);
    add_floats(g_globals, "阴影", {
        { "深度比较偏移", &volume.shadow_bias, 0.00005f, 0.0f, 0.01f },
        { "表面法线偏移", &volume.normal_bias, 0.001f, 0.0f, 0.3f },
        { "太阳覆盖半径（固定岛屿中心）", &volume.sun_extent, 0.5f, 8.0f, 120.0f },
    });
    add_floats(g_globals, "表面", {
        { "树叶摇摆强度", &volume.surface_wind, 0.01f, 0.0f, 2.0f },
        { "水面法线强度", &volume.water_normal, 0.01f, 0.0f, 2.0f },
        { "水面不透明度", &volume.water_alpha, 0.01f, 0.05f, 1.0f },
    });
    add_bool(g_globals, "点光源", "启用局部点光（对比用）", &local_lights_enabled);
    add_bool(g_globals, "点光源", "点光 cube 阴影", &local_light_shadows);
    for(auto& light : local_lights) {
        const auto label = [&](std::string_view suffix) {
            g_light_labels.emplace_back(light.tag + " " + std::string(suffix));
            return g_light_labels.back().c_str();
        };
        add_bool(g_globals, "点光源", label("开启"), &light.enabled);
        add_color(g_globals, "点光源", label("颜色"), light.color);
        add_floats(g_globals, "点光源", {
            { label("强度"), &light.intensity, 0.5f, 0.0f, 500.0f },
            { label("范围"), &light.range, 0.25f, 1.0f, 100.0f },
            { label("半径"), &light.radius, 0.005f, 0.01f, 2.0f },
        });
    }
    if(skybox) {
        add_floats(g_globals, "天空", { { "曝光", &skybox->params().exposure, 0.02f, 0.0f, 4.0f } });
    }

    //// Inspector：遍历带 Tag 的实体，把组件字段登记上去 ////
    g_fields.clear();
    g_entities.clear();
    g_field_groups.clear();
    g_control_edits.clear();
    g_entities.reserve(em.get_entity_pool_size());
    g_fields.reserve(em.get_entity_pool_size() * 8);

    if(ui::backend_needs_entities()) for(auto&& [entity, tag] : em.view<fog::Tag>()) {
        const std::size_t first = g_fields.size();

        // 一行一个组件：字段细节都在各自的 register_* 里
        if(auto* c = em.get_component_raw<fog::Transform>(entity))        register_transform(g_fields, *c);
        if(auto* c = em.get_component_raw<fog::SurfaceMaterial>(entity))  register_surface_material(g_fields, *c);
        if(auto* c = em.get_component_raw<fog::Renderable>(entity))       register_renderable(g_fields, *c);
        if(auto* c = em.get_component_raw<fog::CameraTilt>(entity))       register_camera_tilt(g_fields, *c);
        if(auto* c = em.get_component_raw<fog::Projector>(entity))        register_projector(g_fields, *c);
        if(auto* c = em.get_component_raw<fog::FogParams>(entity))        register_fog_params(g_fields, *c);
        if(auto* c = em.get_component_raw<fog::Skybox>(entity))           register_skybox(g_fields, *c);
        if(auto* c = em.get_component_raw<fog::DirectionalLight>(entity)) register_directional_light(g_fields, *c);
        if(auto* c = em.get_component_raw<fog::FogVolume>(entity))        register_local_fog(g_fields, *c);
        if(auto* c = em.get_component_raw<fog::FogDensityTexture>(entity)) register_fog_texture(g_fields, *c);
        const auto* control = em.get_component_raw<fog::FogControlField>(entity);
        if(control) {
            g_control_edits.emplace_back(entity, *control);
            register_control_field(g_fields, g_control_edits.back().second);
        }
        if(em.get_component<fog::StaticInstance>(entity).has_value())                 add_note(g_fields, "组件: StaticInstance（实例批）");
        if(em.get_component<fog::DirectDraw>(entity).has_value())                     add_note(g_fields, "组件: DirectDraw（逐个提交）");

        if(g_fields.size() != first) {
            g_entities.push_back(ui::Entity{
                .tag = tag.tag.c_str(),
                .id = entity.id,
                .fields = nullptr,
                .field_count = g_fields.size() - first,
                .instanced = em.get_component<fog::StaticInstance>(entity).has_value(),
                .local_fog = em.get_component<fog::FogVolume>(entity).has_value(),
                .control_field = control != nullptr,
                .control_point_count = control ? control->points.size() : 0,
            });
        }
    }

    // Do not retain pointers taken while g_fields may still grow/reallocate.
    std::size_t offset = 0;
    for(auto& entity : g_entities) {
        entity.fields = g_fields.data() + offset;
        offset += entity.field_count;
    }

    /// 所有字段/实体都已登记完，指针不再失效
    int control_point_index = -1;
    int scene_preset_request = -1;
    const ui::Frame frame {
        .globals = g_globals.data(),
        .global_count = g_globals.size(),
        .entities = g_entities.data(),
        .entity_count = g_entities.size(),
        .total_entities = em.entity_count(),
        .instance_count = instance_count,
        .volume_stats = volume_status.c_str(),
        .gpu_stats = gpu_status.c_str(),
        .local_fog_count = local_fog_snapshot ? local_fog_snapshot->counts.x : 0u,
        .local_fog_action = &local_fog_action,
        .local_fog_target = &local_fog_target,
        .control_point_index = &control_point_index,
        .scene_preset = scene_preset,
        .scene_preset_request = &scene_preset_request,
        .scene_preset_names = preset_names,
        .scene_preset_count = scene_preset_count,
        .editor_status = editor_status.c_str(),
        .inspector_dirty = &inspector_dirty,
        .instances_dirty = &instances_dirty,
        .filter = g_filter,
        .filter_size = sizeof(g_filter),
    };

    ui::backend_new_frame();
    ui::backend_draw(frame);
    // Apply staged point edits only after every field pointer has been consumed.
    for(auto& [entity, edited] : g_control_edits)
        if(auto* field = em.get_component_raw<fog::FogControlField>(entity)) *field = std::move(edited);
    const int action = std::exchange(local_fog_action, 0);
    if(action == 1 || action == 2) spawn_local_fog(action - 1);
    else if(action == 3 && local_fog_atlas) rebuild_local_fog_atlas(local_fog_textures);
    else if(action >= 4 && action <= 7) edit_local_fog(action);
    else if(action == 8) save_local_fog();
    else if(action == 9) load_local_fog();
    else if(action >= 10 && action <= 12) {
        const int preset = action - 10;
        volume.pixel_size = std::array{16, 8, 4}[preset];
        volume.depth_slices = std::array{32, 64, 96}[preset];
        volume.light_steps = std::array{2, 4, 8}[preset];
        ++history_epoch;
    } else if(action == 13) {
        animation_paused = true;
        animation_step = true;
    } else if(action == 14) {
        spawn_local_fog(2);
    } else if(action >= 15 && action <= 17) {
        alib5::ecs::Entity target;
        for(auto&& [entity, medium] : em.view<fog::FogVolume>())
            if(entity.id == local_fog_target) { target = entity; break; }
        if(!em.is_valid(target)) { editor_status = "操作失败：局部雾实体已不存在"; return; }
        auto* field = em.get_component_raw<fog::FogControlField>(target);
        if(action == 15 && !field) em.add_component<fog::FogControlField>(target);
        else if(action == 16 && field && field->points.size() < fog::max_fog_control_points)
            field->points.emplace_back();
        else if(action == 17 && field && control_point_index >= 0
            && static_cast<std::size_t>(control_point_index) < field->points.size())
            field->points.erase(field->points.begin() + control_point_index);
        ++history_epoch;
    }
    // All inspector pointers and pending local edits are consumed before replacing entities.
    if(scene_preset_request >= 0) switch_scene_preset(scene_preset_request);
}

auto App::render_ui(void* command_buffer) -> void {
    if(!ui_visible) return;
    ui::backend_render(command_buffer);
}

/// ImGui 后端持有 pipeline / 描述符池 / 字体纹理 / 每帧顶点缓冲，必须在设备销毁前释放，
/// 否则 vkDestroyDevice 会报 VUID-vkDestroyDevice-device-05137（泄漏对象）。
/// 但这些对象还可能被已提交、尚未执行完的命令缓冲引用（退出前最后几帧），
/// 所以按本项目约定（销毁不隐式等 GPU）先显式等空闲再拆。
App::~App() {
    if(renderer && renderer->wait_idle() != VK_SUCCESS) {
        vklg(alib5::Severity::Warn)
            << "wait_idle failed at shutdown, ImGui resources may still be in use."
            << std::endl;
    }
    fog::ui::backend_shutdown();
}

auto App::camera_active() const -> bool {
    if(!window) return false;
    // 只有按 M 锁定鼠标（CursorMode::Disabled）后才允许移动视野。
    // 锁定时系统不显示光标，界面也无法被操作，所以不再叠加 ImGui 的占用判断。
    return window->get_cursor_mode() == ave::CursorMode::Disabled;
}
