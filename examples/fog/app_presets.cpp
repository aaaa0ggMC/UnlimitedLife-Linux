#include "app.h"

namespace {
FogPresetVolume cloud(std::string tag, glm::vec3 position, glm::vec3 size,
                     float density, glm::vec3 color, float period, int seed) {
    FogPresetVolume result;
    result.tag = std::move(tag);
    result.transform.position = position;
    result.transform.scale = size;
    result.transform.dm_mark();
    result.medium.density = density;
    result.medium.color = color;
    result.medium.albedo = 0.96f;
    result.medium.edge_fade = 0.25f;
    result.controls.emplace();
    auto& field = *result.controls;
    field.animated = true;
    field.period = period;
    field.seed = seed;
    field.noise_scale = 4.0f;
    field.noise_strength = 0.55f;
    const std::array<glm::vec3, 6> positions {
        glm::vec3(0.25f, 0.40f, 0.30f), glm::vec3(0.50f, 0.55f, 0.35f),
        glm::vec3(0.75f, 0.42f, 0.40f), glm::vec3(0.30f, 0.60f, 0.65f),
        glm::vec3(0.55f, 0.40f, 0.70f), glm::vec3(0.75f, 0.58f, 0.70f)
    };
    field.points.clear();
    for(unsigned i = 0; i < positions.size(); ++i) {
        fog::FogControlPoint point;
        point.position = positions[i];
        point.radius = 0.34f;
        point.density = 1.6f;
        point.softness = 0.85f;
        point.amplitude = glm::vec3(0.08f, 0.10f, 0.07f);
        point.phase = float(i) * 1.0f;
        point.frequency = 1 + int(i % 2);
        field.points.push_back(point);
    }
    return result;
}

/// 大范围、低速翻滚的云带：在普通 cloud 上放大控制点振幅，弱化边缘。
FogPresetVolume bank(std::string tag, glm::vec3 position, glm::vec3 size, float density,
                     glm::vec3 color, float period, int seed, float noise_scale,
                     float noise_strength, float amplitude) {
    auto result = cloud(std::move(tag), position, size, density, color, period, seed);
    result.medium.edge_fade = 0.38f;
    result.medium.albedo = 0.98f;
    if(result.controls) {
        result.controls->noise_scale = noise_scale;
        result.controls->noise_strength = noise_strength;
        for(auto& point : result.controls->points) {
            point.radius = 0.40f;
            point.amplitude = glm::vec3(amplitude);
        }
    }
    return result;
}

FogScenePresetState make_preset(int index, const FogScenePresetState& baseline) {
    auto result = baseline;
    result.locals.clear();
    result.volume.weather = 0;
    result.volume.near_cutoff = false;
    result.volume.debug = 0;
    result.volume.tone_mapping = true;
    result.volume.exposure_ev = 0.0f;
    result.volume.scattering_albedo = 0.95f;
    result.global.density = 0.0015f;
    result.global.mist_density = 0.002f;
    result.global.height_falloff = 0.6f;
    result.global.ground_y = 0.0f;
    result.global.noise_amount = 0.3f;
    result.global.anim_amplitude = 0.1f;
    result.fog_color = glm::vec3(0.93f, 0.95f, 0.98f);
    result.sun.color = glm::vec3(1.0f);
    result.sun.ambient = 0.38f;
    result.sun.direction = glm::normalize(glm::vec3(-0.4f, 0.8f, 0.3f));
    for(auto& light : result.lights) light.enabled = false;
    result.sky_exposure = 1.0f;
    result.phase = 0.0f;
    result.paused = false;
    if(index == 1) {
        result.volume.anisotropy = 0.25f;
        result.volume.surface_wind = 0.8f;
        result.locals.push_back(cloud("preset-white-front", {3, 3.5f, 4}, {10, 6, 9},
            0.9f, {0.97f, 0.98f, 1.0f}, 14.0f, 17));
        result.locals.push_back(cloud("preset-white-island", {-5, 4, -3}, {9, 5, 10},
            0.7f, {0.97f, 0.98f, 1.0f}, 18.0f, 31));
    } else if(index == 2) {
        result.global.density = 0.001f;
        result.global.mist_density = 0.018f;
        result.global.height_falloff = 1.2f;
        result.sun.color = glm::vec3(1.0f, 0.91f, 0.79f);
        result.sun.direction = glm::normalize(glm::vec3(-0.8f, 0.3f, 0.4f));
        result.volume.anisotropy = 0.35f;
        result.volume.surface_wind = 0.45f;
        result.locals.push_back(cloud("preset-ground-east", {3, 1.8f, 4}, {18, 2.4f, 12},
            0.8f, {0.9f, 0.95f, 1.0f}, 30.0f, 53));
        result.locals.push_back(cloud("preset-ground-west", {-7, 2.4f, -5}, {14, 2.8f, 13},
            0.65f, {0.93f, 0.96f, 1.0f}, 36.0f, 71));
    } else if(index == 3) {
        result.global.density = 0.008f;
        result.sun.color = glm::vec3(1.0f, 0.68f, 0.38f);
        result.sun.ambient = 0.18f;
        result.sun.direction = glm::normalize(glm::vec3(-0.9f, 0.22f, 0.3f));
        result.volume.anisotropy = 0.6f;
        result.volume.shadows = true;
        result.volume.surface_wind = 1.2f;
        for(auto& light : result.lights) {
            light.enabled = true;
            light.intensity *= 1.4f;
        }
        result.locals.push_back(cloud("preset-warm-beams", {-2, 4, 2}, {18, 9, 18},
            0.32f, {0.96f, 0.94f, 0.9f}, 24.0f, 97));
    } else if(index == 4) {
        // 晨光云海：低背景雾 + 几团分离的局部云带，云带内部有独立翻涌。
        result.global.density = 0.0008f;
        result.global.mist_density = 0.0008f;
        result.global.anim_period = 46.0f;
        result.global.anim_amplitude = 0.45f;
        result.global.noise_amount = 0.52f;
        result.fog_color = glm::vec3(0.93f, 0.87f, 0.79f);
        result.sun.color = glm::vec3(1.0f, 0.78f, 0.55f);
        result.sun.ambient = 0.30f;
        result.sun.direction = glm::normalize(glm::vec3(-0.62f, 0.38f, 0.36f));
        result.volume.anisotropy = 0.72f;
        result.volume.exposure_ev = 0.12f;
        result.volume.wind_speed = 0.85f;
        result.volume.surface_wind = 1.5f;
        for(auto& light : result.lights) {
            light.enabled = true;
            light.intensity *= 0.9f;
        }
        result.locals.push_back(bank("preset-dawn-bridge", {8, 2.4f, 0}, {13, 3.6, 8},
            0.62f, {0.98f, 0.92f, 0.84f}, 14.0f, 401, 4.5f, 0.92f, 0.30f));
        result.locals.push_back(bank("preset-dawn-dragon", {-1, 3.2f, 1}, {10, 3.2, 8},
            0.50f, {1.0f, 0.94f, 0.86f}, 18.0f, 402, 4.0f, 0.88f, 0.26f));
        result.locals.push_back(bank("preset-dawn-house", {-6, 2.2f, -4}, {9, 3.0, 7},
            0.42f, {0.96f, 0.90f, 0.84f}, 12.0f, 403, 4.6f, 0.90f, 0.30f));
        result.locals.push_back(bank("preset-dawn-high", {3, 5.4f, -4}, {15, 2.5, 9},
            0.20f, {0.91f, 0.84f, 0.78f}, 22.0f, 404, 3.2f, 0.75f, 0.20f));
    } else if(index == 5) {
        // 风暴云带：多团暗色局部云快速翻滚，保留火把/灯笼做尺度参照。
        result.global.density = 0.0010f;
        result.global.mist_density = 0.0010f;
        result.global.anim_period = 14.0f;
        result.global.anim_amplitude = 0.72f;
        result.global.noise_amount = 0.9f;
        result.fog_color = glm::vec3(0.31f, 0.37f, 0.45f);
        result.sun.color = glm::vec3(0.55f, 0.68f, 0.88f);
        result.sun.ambient = 0.16f;
        result.sun.direction = glm::normalize(glm::vec3(-0.70f, 0.35f, 0.22f));
        result.volume.anisotropy = 0.50f;
        result.volume.exposure_ev = -0.12f;
        result.volume.wind_speed = 2.2f;
        result.volume.surface_wind = 2.2f;
        for(auto& light : result.lights) {
            light.enabled = true;
            light.intensity *= 1.35f;
        }
        result.locals.push_back(bank("preset-storm-a", {-8, 2.8f, 2}, {12, 5, 10},
            0.72f, {0.33f, 0.40f, 0.50f}, 8.0f, 501, 5.6f, 0.98f, 0.34f));
        result.locals.push_back(bank("preset-storm-b", {6, 3.6f, -2}, {14, 5, 11},
            0.68f, {0.38f, 0.45f, 0.55f}, 10.0f, 502, 5.2f, 0.96f, 0.32f));
        result.locals.push_back(bank("preset-storm-c", {-1, 5.4f, -6}, {11, 4, 9},
            0.50f, {0.40f, 0.47f, 0.57f}, 12.0f, 503, 4.6f, 0.94f, 0.30f));
        result.locals.push_back(bank("preset-storm-d", {3, 1.8f, 6}, {10, 4, 8},
            0.58f, {0.36f, 0.43f, 0.53f}, 9.0f, 504, 5.0f, 0.92f, 0.30f));
        result.locals.push_back(bank("preset-storm-e", {12, 4.6f, 1}, {9, 4, 8},
            0.42f, {0.43f, 0.49f, 0.58f}, 14.0f, 505, 4.2f, 0.88f, 0.26f));
    } else if(index == 6) {
        // 黄昏云河：两三条长条局部云带横穿场景，强前向散射。
        result.global.density = 0.0008f;
        result.global.mist_density = 0.0008f;
        result.global.anim_period = 30.0f;
        result.global.anim_amplitude = 0.55f;
        result.global.noise_amount = 0.7f;
        result.fog_color = glm::vec3(0.94f, 0.64f, 0.45f);
        result.sun.color = glm::vec3(1.0f, 0.55f, 0.28f);
        result.sun.ambient = 0.22f;
        result.sun.direction = glm::normalize(glm::vec3(-0.86f, 0.18f, -0.18f));
        result.volume.anisotropy = 0.82f;
        result.volume.exposure_ev = 0.10f;
        result.volume.wind_speed = 1.5f;
        result.volume.surface_wind = 1.2f;
        for(auto& light : result.lights) {
            light.enabled = true;
            light.intensity *= 1.10f;
        }
        result.locals.push_back(bank("preset-dusk-river-a", {0, 2.6f, 0}, {22, 2.6, 7},
            0.56f, {1.0f, 0.70f, 0.50f}, 13.0f, 601, 3.8f, 0.88f, 0.30f));
        result.locals.push_back(bank("preset-dusk-river-b", {7, 3.8f, -2}, {14, 2.4, 6},
            0.46f, {1.0f, 0.78f, 0.58f}, 16.0f, 602, 3.4f, 0.82f, 0.28f));
        result.locals.push_back(bank("preset-dusk-river-c", {-7, 2.0f, 2}, {12, 2.6, 6},
            0.40f, {0.98f, 0.74f, 0.54f}, 18.0f, 603, 3.0f, 0.78f, 0.26f));
    } else if(index == 7) {
        // 月夜流雾：贴着灯火的小体积冷雾，灯光在雾中形成明显光晕。
        result.global.density = 0.0006f;
        result.global.mist_density = 0.0006f;
        result.global.anim_period = 60.0f;
        result.global.anim_amplitude = 0.35f;
        result.global.noise_amount = 0.55f;
        result.fog_color = glm::vec3(0.26f, 0.35f, 0.52f);
        result.sun.color = glm::vec3(0.45f, 0.58f, 0.88f);
        result.sun.ambient = 0.08f;
        result.sun.direction = glm::normalize(glm::vec3(-0.30f, 0.50f, -0.55f));
        result.volume.anisotropy = 0.55f;
        result.volume.exposure_ev = -0.32f;
        result.volume.wind_speed = 0.6f;
        result.volume.surface_wind = 0.7f;
        for(auto& light : result.lights) {
            light.enabled = true;
            light.intensity *= 1.20f;
        }
        result.locals.push_back(bank("preset-night-lantern", {9, 1.5f, 0}, {8, 2.4, 6},
            0.52f, {0.42f, 0.53f, 0.74f}, 22.0f, 701, 3.6f, 0.72f, 0.24f));
        result.locals.push_back(bank("preset-night-dragon", {-2, 1.8f, 2}, {8, 2.6, 6},
            0.42f, {0.40f, 0.51f, 0.72f}, 26.0f, 702, 3.2f, 0.68f, 0.22f));
        result.locals.push_back(bank("preset-night-torch", {3, 1.8f, -4}, {7, 2.2, 5},
            0.36f, {0.45f, 0.56f, 0.76f}, 20.0f, 703, 3.0f, 0.66f, 0.20f));
    }
    return result;
}
} // namespace

auto App::switch_scene_preset(int preset) -> void {
    if(preset < 0 || preset >= int(scene_presets.size()) || preset == scene_preset) return;
    apply_weather(); // Capture a pending weather selection before leaving this tab.
    FogScenePresetState current;
    current.global = cfg.fog;
    current.fog_color = fog.params().color;
    current.fog_start = fog.params().start;
    current.volume = volume;
    current.sun = sunlight_ref.get();
    current.lights = local_lights;
    current.local_lights_enabled = local_lights_enabled;
    current.local_light_shadows = local_light_shadows;
    current.sky_exposure = skybox ? skybox->params().exposure : cfg.sky.exposure;
    current.phase = fog_phase;
    current.paused = animation_paused;
    for(auto&& [entity, transform, medium] : em.view<fog::Transform, fog::FogVolume>()) {
        FogPresetVolume entry;
        if(const auto* tag = em.get_component_raw<fog::Tag>(entity)) entry.tag = tag->tag;
        else entry.tag = "local-fog";
        entry.transform = transform;
        entry.medium = medium;
        if(const auto* texture = em.get_component_raw<fog::FogDensityTexture>(entity)) entry.texture = *texture;
        if(const auto* field = em.get_component_raw<fog::FogControlField>(entity)) entry.controls = *field;
        current.locals.push_back(std::move(entry));
    }
    scene_presets[scene_preset] = std::move(current);
    if(!scene_presets[preset]) scene_presets[preset] = make_preset(preset, *scene_presets[0]);
    const auto& next = *scene_presets[preset];
    std::vector<alib5::ecs::Entity> old;
    for(auto&& [entity, medium] : em.view<fog::FogVolume>()) old.push_back(entity);
    for(auto entity : old) em.destroy_entity(entity);
    for(const auto& entry : next.locals) {
        fog::FogVolumeEntity object(em, entry.tag, entry.medium, {}, entry.transform.position, entry.transform.scale);
        auto* transform = em.get_component_raw<fog::Transform>(object.get_entity());
        *transform = entry.transform; // Preserve quaternion exactly, without angle-unit conversion.
        transform->dm_mark();
        if(entry.texture) object.add<fog::FogDensityTexture>(*entry.texture);
        if(entry.controls) object.add<fog::FogControlField>(*entry.controls);
    }
    cfg.fog = next.global;
    fog.params().color = next.fog_color;
    fog.params().start = next.fog_start;
    fog.params().dm_mark();
    volume = next.volume;
    applied_weather = volume.weather; // Restoring a tab must not reapply weather defaults over its edits.
    sunlight_ref.get() = next.sun;
    local_lights = next.lights;
    local_lights_enabled = next.local_lights_enabled;
    local_light_shadows = next.local_light_shadows;
    if(skybox) skybox->params().exposure = next.sky_exposure;
    fog_phase = next.phase;
    animation_paused = next.paused;
    animation_step = false;
    local_fog_target = 0;
    scene_preset = preset;
    ++history_epoch;
    editor_status = "已切换场景预设；各页修改会在本次会话内保留";
}
