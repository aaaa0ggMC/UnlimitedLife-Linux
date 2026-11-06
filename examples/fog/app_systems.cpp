/**
 * @file app_systems.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 
 * @version 5.0
 * @date 2026-10-14
 * 
 * @copyright Copyright (c) 2026
 * @parblock audit history (AI 别写这里)
 * 1. OK 修正了大量范式错误
 * 2. OK
 * @endparblock
 * 
 */
#include "app.h"
#include <cstring>

namespace {
glm::vec3 safe_direction(glm::vec3 direction, glm::vec3 fallback) {
    return glm::dot(direction, direction) > 1e-8f ? glm::normalize(direction) : fallback;
}

bool same_volume_state(const CameraFogUbo& a, const CameraFogUbo& b) {
    return a.fog_color == b.fog_color && a.fog_params == b.fog_params
        && a.fog_extra == b.fog_extra && glm::vec2(a.fog_anim.y, a.fog_anim.z) == glm::vec2(b.fog_anim.y, b.fog_anim.z)
        && a.fog_anim.w == b.fog_anim.w
        && a.light_direction == b.light_direction && a.light_color == b.light_color
        && a.froxel_grid == b.froxel_grid && a.shadow_params == b.shadow_params
        && a.local_light_info == b.local_light_info
        && std::equal(std::begin(a.local_light_position_range), std::end(a.local_light_position_range),
                      std::begin(b.local_light_position_range))
        && std::equal(std::begin(a.local_light_color_intensity), std::end(a.local_light_color_intensity),
                      std::begin(b.local_light_color_intensity))
        && std::equal(std::begin(a.local_light_params), std::end(a.local_light_params),
                      std::begin(b.local_light_params))
        && a.volume_params == b.volume_params && a.fog_shape == b.fog_shape
        && a.local_fog_info == b.local_fog_info
        && a.light_view_proj[0] == b.light_view_proj[0] && a.light_view_proj[1] == b.light_view_proj[1]
        && a.proj == b.proj;
}
} // namespace

//// 相机系统：鼠标视角 + WASD/QE 平移 ////
auto App::update_camera(float dt) -> void {
    auto& tilt = camera.tilt();
    auto& transform = camera.transform();

    // 只有按下 M 锁定鼠标（且界面没占用鼠标）后才读输入、移动视野，避免与 ImGui 抢输入；
    // 视角状态 → Transform 的同步始终执行，这样界面里改 yaw/pitch 也能立刻看到效果。
    const bool active = camera_active();
    if(active && window) {
        // GLFW 只在切换模式时写一次光标状态；Wayland（KWin）下合成器在指针锁定
        // 激活后可能重新显示光标，这里按 tick 持续重设以保证锁定期间光标始终隐藏。
        window->reapply_cursor_mode();
    }
    const auto [mdx, mdy] = active
        ? input.get_mouse_delta()
        : std::pair { 0.0, 0.0 };
    tilt.add_mouse_delta(mdx, mdy);
    transform.set_yaw_pitch(tilt.yaw_deg, tilt.pitch_deg);

    if(active) transform.translate(fog::camera_move_delta(transform, input, dt));
}

//// 环境输入系统：雾参数与太阳高光开关 ////
auto App::update_environment(float dt) -> void {
    auto& fog_params = fog.params();
    auto& light = sunlight_ref.get();
    if(input.is_key_just_pressed(ave::KeyCode::B)) light.specular = !light.specular;
    if(input.is_key_just_pressed(ave::KeyCode::F1)) ui_visible = !ui_visible;

    if(input.is_key_down(ave::KeyCode::G)) fog_params.start += dt * 5.0f;
    if(input.is_key_down(ave::KeyCode::F)) fog_params.start -= dt * 5.0f;
    fog_params.start = std::clamp(fog_params.start, 0.0f, 400.0f);

    if(input.is_key_just_pressed(ave::KeyCode::Left)) {
        cfg.fog.anim_period = std::max(cfg.fog.anim_period * 0.8f, 5.0f);
        lg(alib5::Severity::Info) << "Fog anim period = " << cfg.fog.anim_period << " s" << std::endl;
    }
    if(input.is_key_just_pressed(ave::KeyCode::Right)) {
        cfg.fog.anim_period = std::min(cfg.fog.anim_period * 1.25f, 900.0f);
        lg(alib5::Severity::Info) << "Fog anim period = " << cfg.fog.anim_period << " s" << std::endl;
    }
    surface_time += dt;
    const bool step = std::exchange(animation_step, false);
    if(!animation_paused || step) {
        fog_phase += dt * 6.2831853f / std::max(cfg.fog.anim_period, 1.0f);
        for(auto&& [entity, field] : em.view<fog::FogControlField>()) {
            if(!field.enabled || !field.animated) continue;
            field.cycle = std::fmod(field.cycle + double(dt) / std::max(field.period, 0.1f), 1.0);
        }
    }
}

auto App::render_scene() -> void {
    const auto [fb_w, fb_h] = window->get_framebuffer_size();
    if(fb_w <= 0 || fb_h <= 0) return;
    apply_weather();
    volume.pixel_size = std::clamp(volume.pixel_size, 4, 32);
    volume.depth_slices = std::clamp(volume.depth_slices, 16, 96);
    const glm::uvec2 desired_extent {
        std::clamp((fb_w + volume.pixel_size - 1) / volume.pixel_size, 1, 192),
        std::clamp((fb_h + volume.pixel_size - 1) / volume.pixel_size, 1, 128)
    };
    if(desired_extent != froxel_extent || unsigned(volume.depth_slices) != froxel_depth) setup_ubo();
    if(inspector_dirty) {
        // The POD UI edits fields directly; invalidate the dependent matrix caches as well.
        em.view<fog::Transform>().for_each([](auto& transform) { transform.dm_mark(); });
        em.view<fog::Projector>().for_each([](auto& projector) { projector.dm_mark(); });
        inspector_dirty = false;
        ++geometry_revision;
        ++history_epoch;
    }
    if(instances_dirty) {
        // Only edits of actual StaticInstance entities touch the shared geometry SSBO.
        if(renderer->wait_idle() != VK_SUCCESS) panicf("Cannot update scene instances safely.");
        refresh_instances();
        instances_dirty = false;
        ++geometry_revision;
        ++history_epoch;
    }
    sync_local_fog();

    auto& fog_params = fog.params();
    const auto& light = sunlight_ref.get();

    camera.projection().set_viewport(static_cast<float>(fb_w), static_cast<float>(fb_h));
    const glm::mat4& view = camera.view_matrix();
    const glm::mat4& proj = camera.projection_matrix();

    // 上传相机 + 雾参数 
    const auto sun_direction = safe_direction(light.direction, glm::vec3(0.0f, 1.0f, 0.0f));
    const auto up_for = [](glm::vec3 direction) {
        return std::abs(direction.y) > 0.95f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    };
    // mascot 只画一只 4 单位长的龙：把太阳阴影收紧到它身上，2048 阴影图才能保住鳞片级细节。
    const glm::vec3 shadow_center = mascot.enabled ? mascot.target : glm::vec3(0.0f, 8.0f, 0.0f);
    const float extent = mascot.enabled
        ? std::clamp(volume.sun_extent, 3.0f, 12.0f)
        : std::clamp(volume.sun_extent, 8.0f, 120.0f);
    auto sun_projection = glm::orthoRH_ZO(-extent, extent, -extent, extent, 0.1f, 240.0f);
    sun_projection[1][1] *= -1.0f;
    const auto sun_view = glm::lookAtRH(shadow_center + sun_direction * 120.0f, shadow_center, up_for(sun_direction));
    const auto format = renderer->swapchain->get_surface_format().format;
    const bool srgb_target = format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB
        || format == VK_FORMAT_B8G8R8_SRGB || format == VK_FORMAT_R8G8B8_SRGB
        || format == VK_FORMAT_A8B8G8R8_SRGB_PACK32;
    sun_shadow_matrix = sun_projection * sun_view;

    // 局部点光：场景数据 + 每帧的阴影预算。只对最近的 N 盏渲染 cube shadow。
    std::array<glm::vec4, max_local_lights> local_position_range {};
    std::array<glm::vec4, max_local_lights> local_color_intensity {};
    std::array<glm::vec4, max_local_lights> local_params {};
    const unsigned light_count = static_cast<unsigned>(std::min<std::size_t>(local_lights.size(), max_local_lights));
    for(unsigned i = 0; i < light_count; ++i) {
        auto& light = local_lights[i];
        light.range = std::max(light.range, 1.0f);
        light.radius = std::clamp(light.radius, 0.01f, 2.0f);
        local_position_range[i] = glm::vec4(light.position, light.range);
        local_color_intensity[i] = glm::vec4(light.color,
            (light.enabled && local_lights_enabled) ? std::max(light.intensity, 0.0f) : 0.0f);
        local_params[i] = glm::vec4(light.radius, -1.0f, light.enabled ? 1.0f : 0.0f, 0.0f);
    }
    std::array<unsigned, max_shadow_lights> chosen;
    chosen.fill(max_local_lights);
    if(volume.shadows && local_light_shadows && local_lights_enabled) {
        const glm::vec3 eye = camera.transform().position;
        std::array<std::pair<float, unsigned>, max_local_lights> candidates {};
        unsigned candidate_count = 0;
        for(unsigned i = 0; i < light_count; ++i) {
            const auto& light = local_lights[i];
            if(!light.enabled || !light.cast_shadow) continue;
            candidates[candidate_count++] = { glm::dot(eye - light.position, eye - light.position), i };
        }
        std::sort(candidates.begin(), candidates.begin() + candidate_count);
        const unsigned shadow_count = std::min<unsigned>(candidate_count, max_shadow_lights);
        for(unsigned slot = 0; slot < shadow_count; ++slot) {
            const unsigned index = candidates[slot].second;
            chosen[slot] = index;
            local_params[index].y = static_cast<float>(slot * point_shadow_faces);
        }
        shadowed_lights = chosen;
    } else {
        shadowed_lights = chosen;
    }
    const unsigned shadow_count = static_cast<unsigned>(std::count_if(
        shadowed_lights.begin(), shadowed_lights.end(),
        [](unsigned index) { return index < max_local_lights; }));
    CameraFogUbo ubo {
        .view = view,
        .proj = proj,
        .fog_color = glm::vec4(fog_params.color, 1.0f),
        .fog_params = glm::vec4(fog_params.start, camera.projection().far_plane, cfg.fog.mist_density, cfg.fog.noise_amount),
        .camera_position = glm::vec4(camera.transform().position, 1.0f),
        .light_direction = glm::vec4(sun_direction, light.ambient),
        .light_color = glm::vec4(light.color, light.specular ? 1.0f : 0.0f),
        .fog_extra = glm::vec4(cfg.fog.height_falloff, cfg.fog.ground_y, cfg.fog.noise_scale, cfg.fog.density),
        .fog_anim = glm::vec4(fog_phase, cfg.fog.anim_amplitude, cfg.fog.anim_frequency, volume.near_cutoff ? 1.0f : 0.0f),
        .inverse_view_proj = glm::inverse(proj * view),
        .froxel_grid = glm::vec4(froxel_extent.x, froxel_extent.y, froxel_depth, 1.0f),
        .light_view_proj = { sun_shadow_matrix },
        .previous_view = view,
        .previous_view_proj = proj * view,
        .shadow_params = glm::vec4(volume.shadows ? 1.0f : 0.0f, volume.shadow_bias, volume.normal_bias,
            float(std::clamp(volume.surface_shadow_radius, 0, 4))),
        .local_light_info = glm::vec4(light_count, shadow_count,
            local_lights_enabled ? 1.0f : 0.0f, local_light_shadows ? 1.0f : 0.0f),
        .volume_params = glm::vec4(volume.anisotropy, volume.scattering_albedo, volume.sunlight_path, volume.light_steps),
        .fog_shape = glm::vec4(volume.weather, volume.layer_center, volume.layer_thickness, volume.wind_speed),
        .temporal = glm::vec4(0.0f, 0.0f, volume.jitter ? float(render_frame % 1024) * 0.61803398875f : -1.0f, volume.debug),
        // Keep stochastic froxel prefixes out of the deterministic per-pixel correction.
        // Gate on the requested jitter mode, not temporal.z (which also changes on
        // history invalidation), so the correction cannot toggle frame by frame.
        .local_fog_info = glm::vec4(local_fog_snapshot->counts.x,
            volume.surface_refinement && !volume.jitter ? 1.0f : 0.0f,
            0.0f, mascot.enabled ? 1.0f : 0.0f),
        .display_params = glm::vec4(volume.exposure_ev, volume.tone_mapping ? 1.0f : 0.0f, srgb_target ? 0.0f : 1.0f, 0.0f),
        .surface_params = glm::vec4(surface_time, volume.surface_wind, volume.water_normal,
            volume.water_alpha),
    };
#ifdef __ANDROID__
    // Reserved component: bypass sampling uninitialized froxel storage in no-volume runs.
    ubo.local_fog_info.z = android_no_volume ? 1.0f : 0.0f;
#endif
    for(unsigned i = 0; i < max_local_lights; ++i) {
        ubo.local_light_position_range[i] = local_position_range[i];
        ubo.local_light_color_intensity[i] = local_color_intensity[i];
        ubo.local_light_params[i] = local_params[i];
    }

    auto graphics = renderer->acquire_context();
    if(!graphics) return;

    const auto image_index = graphics.get_image_index();
    if(uploaded_local_fog.at(image_index) != local_fog_revision) {
        if(!local_fog_buffers.at(image_index)->upload(*local_fog_snapshot)) panicf("Local fog parameters upload failed.");
        uploaded_local_fog.at(image_index) = local_fog_revision;
    }
    if(uploaded_control_fog.at(image_index) != control_fog_revisions) {
        if(!control_fog_buffers.at(image_index)->upload(control_fog_snapshot)) panicf("Control fog upload failed.");
        uploaded_control_fog.at(image_index) = control_fog_revisions;
    }
    read_gpu_timings(image_index);
    auto& history = volume_history.at(image_index);
    const unsigned output = 1 - history.read_index;
    const double now = std::chrono::duration<double>(std::chrono::steady_clock::now() - render_start).count();
    if(volume.temporal && history.camera && history.epoch == history_epoch) {
        const auto& previous = *history.camera;
        const float distance = glm::length(glm::vec3(ubo.camera_position - previous.camera_position));
        const auto previous_forward = glm::vec3(glm::inverse(previous.view) * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
        const float alignment = glm::dot(previous_forward, camera.transform().view_direction());
        if(now - history.time <= 1.0 && distance < 5.0f && alignment > 0.85f
            && same_volume_state(ubo, previous)) {
            ubo.previous_view = previous.view;
            ubo.previous_view_proj = previous.proj * previous.view;
            const float age = float(std::max<std::uint64_t>(render_frame - history.frame, 1));
            ubo.temporal.x = std::pow(std::clamp(volume.history_weight, 0.0f, 0.97f), age)
                * std::exp(-distance * 0.4f) * std::pow(std::max(alignment, 0.0f), 8.0f);
            ubo.temporal.y = 1.0f;
        }
    }
    // Without valid accumulation, random injection would expose unfiltered speckle.
    if(ubo.temporal.y < 0.5 || !volume.temporal) ubo.temporal.z = -1.0f;
    auto& uploaded = uploaded_ubos.at(image_index);
    if(!uploaded || std::memcmp(&*uploaded, &ubo, sizeof(ubo)) != 0) {
        if(!ubo_buffers.at(image_index)->upload(ubo)) panicf("Camera UBO upload failed.");
        uploaded = ubo;
    }
    
    const auto draw_object = [&](
        fog::Transform& transform, 
        fog::Renderable& renderable, 
        fog::SurfaceMaterial& material
    ){
        graphics.push_constant(make_draw_data(transform, renderable, material));
        graphics.bind_vertex_buffer(renderable.vertices);
        graphics.bind_index_buffer<std::uint32_t>(renderable.indices);
        graphics.draw_indexed(renderable.index_count);
    };

    // mascot 截帧：只保留 dragon，背景 alpha=0；RGB 清黑，缩小后按预乘 alpha 还原。
    const bool transparent = mascot.enabled;
    graphics.begin(transparent ? 0.0f : fog_params.color.r,
        transparent ? 0.0f : fog_params.color.g,
        transparent ? 0.0f : fog_params.color.b,
        transparent ? 0.0f : 1.0f,
        [&](VkCommandBuffer commands) {
            record_volume(commands, image_index, output);
        });
    graphics.bind_pipeline(*pipeline);
    graphics.bind_descriptor_set(0, descriptor_cubes.at(image_index));
    /// 独立的可绘制对象
    em.view<fog::Transform, fog::Renderable, fog::SurfaceMaterial, fog::DirectDraw>().for_each(
        [&](auto& transform, auto& renderable, auto& material, auto&) { draw_object(transform, renderable, material); }
    );

    /// 一个个方块
    if(instance_count > 0) {
        graphics.bind_pipeline(*instanced_pipeline);
        graphics.bind_descriptor_set(0, descriptor_cubes.at(image_index));
        graphics.bind_vertex_buffer(cube_vertices);
        graphics.bind_index_buffer<std::uint32_t>(cube_indices);
        graphics.draw_indexed(36, instance_count);
    }

    /// 天空盒（mascot 截帧时只输出局部雾的透明薄纱，不输出天空本体）
    if(skybox && (!mascot.enabled || mascot.halo)) {
        graphics.bind_pipeline(*sky_pipeline);
        graphics.bind_descriptor_set(0, descriptor_sky.at(image_index));
        auto& sky_params = skybox->params();
        auto& sky_renderable = skybox->renderable();
        graphics.push_constant(SkyPushConstant{
            .params = glm::vec4(sky_params.exposure, sky_params.textured ? 1.0f : 0.0f,
                mascot.enabled ? 1.0f : 0.0f, 0.0f)
        });
        graphics.bind_vertex_buffer(sky_renderable.vertices);
        graphics.bind_index_buffer<std::uint32_t>(sky_renderable.indices);
        graphics.draw_indexed(sky_renderable.index_count);
    }

    /// 透明水面：在不透明几何和天空之后混合，不写深度。
    if(water_instance_count > 0) {
        graphics.bind_pipeline(*water_pipeline);
        graphics.bind_descriptor_set(0, descriptor_water.at(image_index));
        graphics.bind_vertex_buffer(cube_vertices);
        graphics.bind_index_buffer<std::uint32_t>(cube_indices);
        graphics.draw_indexed(36, water_instance_count);
    }

    /// ImGui 界面：直接录进当前渲染作用域（dynamic rendering 或 legacy render pass）
    if(!mascot.enabled) render_ui(graphics.get_command_buffer());
    if(gpu_queries->pool) gpu_queries->pool->write(graphics.get_command_buffer(),
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, image_index * 6 + 5);

    graphics.end();
    if(graphics.get_result() == VK_SUCCESS || graphics.get_result() == VK_SUBOPTIMAL_KHR) {
        generated_control_fog.at(image_index) = control_fog_revisions;
        for(std::size_t light_index = 0; light_index < shadow_cache.at(image_index).size(); ++light_index) {
            shadow_cache.at(image_index)[light_index] = {
                ubo.light_view_proj[light_index], geometry_revision,
                volume.shadows, true };
        }
        history.camera = ubo;
        history.epoch = history_epoch;
        history.frame = render_frame;
        history.time = now;
        history.read_index = output;
        gpu_queries->submitted.at(image_index) = true;
        ++render_frame;
    } else {
        // A failed submit/present must never make unexecuted history look valid.
        for(auto& item : volume_history) item.camera.reset();
        for(auto& revisions : generated_control_fog) revisions.fill(0);
        for(auto& image : shadow_cache) for(auto& item : image) item.valid = false;
    }
}
