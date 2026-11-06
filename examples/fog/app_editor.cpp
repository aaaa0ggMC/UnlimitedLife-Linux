#include <fstream>
#include <glm/gtc/constants.hpp>
#include "app.h"
#include "ui_bridge.h"

bool valid_control_field(const SceneFogControlField& f) {
    const auto scalar = [](float v, float lo, float hi) {
        return std::isfinite(v) && v >= lo && v <= hi;
    };
    const auto vector = [&](const SceneVec& v, float lo, float hi) {
        return std::ranges::all_of(v, [&](float x) { return scalar(x, lo, hi); });
    };
    if(!scalar(f.period, 0.1f, 900.0f) || !scalar(f.noise_scale, 0.1f, 32.0f)
        || !scalar(f.noise_strength, 0, 1) || f.octaves < 1 || f.octaves > 5
        || f.seed < 0 || f.seed > 65535 || f.points.size() > fog::max_fog_control_points) return false;
    for(const auto& p : f.points) {
        if(!vector(p.position, 0, 1) || !scalar(p.radius, 0.02f, 1)
            || !scalar(p.density, 0, 4) || !scalar(p.softness, 0.05f, 1)
            || !vector(p.amplitude, 0, 0.5f) || !scalar(p.phase, 0, glm::two_pi<float>())
            || p.frequency < 1 || p.frequency > 8) return false;
    }
    return true;
}

fog::FogControlField control_field_component(const SceneFogControlField& f) {
    fog::FogControlField result;
    result.enabled = f.enabled; result.animated = f.animated; result.period = f.period;
    result.noise_scale = f.noise_scale; result.noise_strength = f.noise_strength;
    result.octaves = f.octaves; result.seed = f.seed;
    result.points.clear();
    for(const auto& p : f.points) result.points.push_back({
        {p.position[0], p.position[1], p.position[2]}, p.radius, p.density, p.softness,
        {p.amplitude[0], p.amplitude[1], p.amplitude[2]}, p.phase, p.frequency});
    return result; // cycle is runtime-only and starts at zero.
}

namespace {
struct LocalFogDocument {
    int version { 1 };
    std::vector<SceneFogVolume> fog_volumes;
};

SceneVec to_scene(glm::vec3 v) { return {v.x, v.y, v.z}; }
glm::vec3 to_glm(const SceneVec& v) { return {v[0], v[1], v[2]}; }

bool bounded(const SceneVec& v, float lo, float hi) {
    return std::ranges::all_of(v, [=](float f) { return std::isfinite(f) && f >= lo && f <= hi; });
}

bool valid_document(const LocalFogDocument& doc) {
    if(doc.version != 1 || doc.fog_volumes.size() > max_local_fog_volumes) return false;
    std::vector<FogTextureKey> textures;
    for(const auto& v : doc.fog_volumes) {
        if(!valid_control_field(v.control_field)) return false;
        if(v.tag.size() > 256 || !bounded(v.pos, -100000.0f, 100000.0f)
            || !bounded(v.scale, 0.001f, 10000.0f) || !bounded(v.axis, -1.0f, 1.0f)
            || !std::isfinite(v.angle) || std::abs(v.angle) > 6.284f
            || (v.angle != 0.0f && glm::length(to_glm(v.axis)) < 0.0001f)
            || v.shape < 0 || v.shape > 1 || !std::isfinite(v.density) || v.density < 0 || v.density > 100
            || !bounded(v.color, 0, 1) || !std::isfinite(v.albedo) || v.albedo < 0 || v.albedo > 1
            || !std::isfinite(v.edge_fade) || v.edge_fade < 0 || v.edge_fade > 1
            || !bounded(v.texture.tiling, -10000, 10000) || !bounded(v.texture.offset, -10000, 10000)
            || !std::isfinite(v.texture.strength) || v.texture.strength < 0 || v.texture.strength > 1) return false;
        if(v.texture.path.empty()) continue;
        FogTextureKey key {v.texture.path, v.texture.path == "builtin:cloud"
            ? std::array{32, 32, 32} : v.texture.extent};
        std::uintmax_t size = 1;
        for(int d : key.extent) {
            if(d < 1 || d > 128) return false;
            size *= d;
        }
        if(key.path != "builtin:cloud") {
            std::error_code error;
            if(!std::filesystem::is_regular_file(key.path, error) || error
                || std::filesystem::file_size(key.path, error) != size || error) return false;
        }
        // Validate fallback textures even when overridden, but only active textures occupy atlas slots.
        if(!v.control_field.enabled && std::ranges::find(textures, key) == textures.end())
            textures.push_back(key);
    }
    return textures.size() <= max_local_fog_textures;
}

LocalFogDocument capture_local_fog(alib5::ecs::EntityManager& em) {
    LocalFogDocument doc;
    for(auto&& [entity, transform, medium] : em.view<fog::Transform, fog::FogVolume>()) {
        SceneFogVolume v;
        const auto* tag = em.get_component_raw<fog::Tag>(entity);
        v.tag = tag ? tag->tag : "local-fog";
        v.pos = to_scene(transform.position);
        v.scale = to_scene(transform.scale);
        const auto q = glm::normalize(transform.rotation);
        v.angle = glm::angle(q);
        if(std::abs(std::sin(v.angle * 0.5f)) > 1e-5f) v.axis = to_scene(glm::axis(q));
        v.shape = medium.shape; v.density = medium.density; v.color = to_scene(medium.color);
        v.albedo = medium.albedo; v.edge_fade = medium.edge_fade; v.enabled = medium.enabled;
        if(const auto* f = em.get_component_raw<fog::FogControlField>(entity)) {
            auto& saved = v.control_field;
            saved.enabled = f->enabled; saved.animated = f->animated; saved.period = f->period;
            saved.noise_scale = f->noise_scale; saved.noise_strength = f->noise_strength;
            saved.octaves = f->octaves; saved.seed = f->seed;
            saved.points.clear();
            for(const auto& p : f->points) saved.points.push_back({to_scene(p.position),
                p.radius, p.density, p.softness, to_scene(p.amplitude), p.phase, p.frequency});
        }
        if(const auto* texture = em.get_component_raw<fog::FogDensityTexture>(entity)) {
            v.texture = {texture->path, texture->extent, to_scene(texture->tiling),
                to_scene(texture->offset), texture->strength, texture->repeat};
        }
        doc.fog_volumes.push_back(std::move(v));
    }
    std::ranges::sort(doc.fog_volumes, {}, &SceneFogVolume::tag);
    return doc;
}
} // namespace

auto App::edit_local_fog(int action) -> void {
    // Resolve current generation, never trust a retained component pointer after ECS mutation.
    alib5::ecs::Entity target;
    for(auto&& [entity, medium] : em.view<fog::FogVolume>())
        if(entity.id == local_fog_target) { target = entity; break; }
    if(!em.is_valid(target)) { editor_status = "操作失败：局部雾实体已不存在"; return; }
    auto* transform = em.get_component_raw<fog::Transform>(target);
    if(!transform) return;
    if(action == 4) {
        if(em.view<fog::FogVolume>().count() >= max_local_fog_volumes) {
            editor_status = "最多保留 16 个局部雾实体（含禁用项）"; return;
        }
        // Copy everything before adding components: pool growth may invalidate references.
        const auto copied_transform = *transform;
        const auto medium = *em.get_component_raw<fog::FogVolume>(target);
        const auto* source_texture = em.get_component_raw<fog::FogDensityTexture>(target);
        const auto texture = source_texture ? *source_texture : fog::FogDensityTexture{};
        std::optional<fog::FogControlField> control;
        if(const auto* f = em.get_component_raw<fog::FogControlField>(target)) control = *f;
        fog::FogVolumeEntity copy(em, std::format("local-fog-copy-{}", ++local_fog_serial), medium, texture,
            copied_transform.position + glm::vec3(1.0f, 0.0f, 0.0f), copied_transform.scale);
        auto* result = em.get_component_raw<fog::Transform>(copy.get_entity());
        result->rotation = copied_transform.rotation;
        result->dm_mark();
        if(control) copy.add<fog::FogControlField>(*control);
        local_fog_target = copy.get_entity().id;
    } else if(action == 5) {
        em.destroy_entity(target);
        local_fog_target = 0;
    } else if(action == 6) {
        transform->position = camera.transform().position + camera.transform().view_direction() * 8.0f;
        transform->dm_mark();
    } else if(action == 7) {
        camera.transform().position = transform->position - camera.transform().view_direction()
            * std::max(4.0f, glm::length(transform->scale));
        camera.transform().dm_mark();
    }
    ++history_epoch;
    editor_status = "局部雾操作完成；保存可跨重启保留布局";
}

auto App::save_local_fog(const std::filesystem::path& path) -> void {
    try {
        const auto doc = capture_local_fog(em);
        if(!valid_document(doc)) { editor_status = "保存失败：局部雾参数或纹理不合法"; return; }
        // 验证器严格区分 Int/Double，而写出器对整值 double 会省略小数点，
        // 因此固定小数位保留浮点类型；47 位同时保证所有有限 float 子正规数可往返。
        const auto text = alib5::to_adata(doc).str(alib5::data::JSON({.float_precision = 47}));
        const auto temporary = std::filesystem::path(path.string() + ".tmp");
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << text;
        output.close();
        if(!output) { editor_status = "保存失败：无法完整写入临时文件"; return; }
        if(std::filesystem::exists(path)) std::filesystem::copy_file(path, path.string() + ".bak",
            std::filesystem::copy_options::overwrite_existing);
        std::filesystem::rename(temporary, path);
        editor_status = "已保存局部雾到 fog-local.json；上次存档保留为 .bak";
    } catch(const std::exception&) { editor_status = "保存失败：文件读写错误，原存档保留"; }
}

auto App::load_local_fog(const std::filesystem::path& path) -> void {
    try {
        if(std::filesystem::file_size(path) > 1024 * 1024) {
            editor_status = "载入失败：文件超过 1 MiB"; return;
        }
        LocalFogDocument doc;
        const auto result = alib5::compact::load_config_from_file(path.string(), doc);
        if(!result.success || !valid_document(doc)) {
            editor_status = "载入失败：版本、参数或纹理无效；当前布局未改变"; return;
        }
        std::vector<alib5::ecs::Entity> old;
        for(auto&& [entity, medium] : em.view<fog::FogVolume>()) old.push_back(entity);
        for(auto entity : old) em.destroy_entity(entity);
        for(const auto& v : doc.fog_volumes) {
            fog::FogVolumeEntity object(em, v.tag,
                {v.shape, v.density, to_glm(v.color), v.albedo, v.edge_fade, v.enabled},
                {v.texture.path, v.texture.extent, to_glm(v.texture.tiling), to_glm(v.texture.offset),
                 v.texture.strength, v.texture.repeat}, to_glm(v.pos), to_glm(v.scale), to_glm(v.axis), v.angle);
            object.add<fog::FogControlField>(control_field_component(v.control_field));
        }
        local_fog_target = 0;
        ++history_epoch;
        editor_status = "已载入局部雾布局；不影响几何、全局环境或灯光";
    } catch(const std::exception&) { editor_status = "载入失败：文件读写错误"; }
}

int check_editor() {
    const auto require = [](bool ok, const char* name) {
        if(!ok) throw std::runtime_error(name);
    };
    const auto path = std::filesystem::path("/tmp/opencode")
        / std::format("fog-editor-{}.json", std::chrono::steady_clock::now().time_since_epoch().count());
    try {
        require(fog::ui::check_fields(), "ImGui IDs/collapsed fields");
        App app(AppConfig{});
        ShadowCache cache;
        require(!cache.reusable(glm::mat4(1), 1, false), "first disabled map must initialize");
        cache = {glm::mat4(1), 1, true, true};
        require(cache.reusable(glm::mat4(1), 1, true), "static shadow cache hit");
        require(!cache.reusable(glm::mat4(1), 2, true), "geometry invalidation");
        require(!cache.reusable(glm::mat4(2), 1, true), "projection invalidation");
        require(!cache.reusable(glm::mat4(1), 1, false), "disable invalidation");
        cache.enabled = false;
        require(cache.reusable(glm::mat4(2), 2, false), "disabled map reuse");
        require(!cache.reusable(glm::mat4(1), 1, true), "enable invalidation");
        app.animation_paused = true;
        app.update_environment(world_update_dt);
        require(app.fog_phase == 0, "animation pause");
        app.animation_step = true;
        app.update_environment(world_update_dt);
        const auto phase = app.fog_phase;
        app.update_environment(world_update_dt);
        require(phase > 0 && app.fog_phase == phase, "single step");
        app.spawn_local_fog(1);
        for(auto&& [entity, medium] : app.em.view<fog::FogVolume>()) {
            fog::FogControlField field;
            field.enabled = false; field.animated = true; field.period = 17.0f;
            field.seed = 42; field.noise_scale = 9.0f; field.noise_strength = 0.3f; field.octaves = 5;
            field.points.push_back({{0.2f, 0.3f, 0.4f}, 0.4f, 2.0f, 0.3f,
                {0.1f, 0.2f, 0.3f}, 1.2f, 3});
            field.cycle = 0.375;
            app.em.add_component<fog::FogControlField>(entity, field);
        }
        for(auto&& [entity, field] : app.em.view<fog::FogControlField>()) {
            field.enabled = true;
            field.period = 0.1f;
            field.cycle = 0.375;
            app.animation_paused = true;
            app.update_environment(world_update_dt);
            require(field.cycle == 0.375, "control animation global pause");
            app.animation_step = true;
            app.update_environment(world_update_dt);
            const double stepped = field.cycle;
            require(std::abs(stepped - (0.375 + double(world_update_dt) / field.period)) < 1e-9,
                "control animation single step");
            app.update_environment(world_update_dt);
            require(field.cycle == stepped && !app.animation_step, "control single step consumed");
            field.cycle = 0.98;
            app.animation_paused = false;
            app.update_environment(world_update_dt);
            require(field.cycle >= 0 && field.cycle < 1
                && std::abs(field.cycle - (0.98 + double(world_update_dt) / field.period - 1)) < 1e-9,
                "control animation period wrap");
            field.animated = false;
            const double stopped = field.cycle;
            app.update_environment(world_update_dt);
            require(field.cycle == stopped, "control animation per-field pause");
            field.animated = true;
            field.enabled = false;
            app.update_environment(world_update_dt);
            require(field.cycle == stopped, "disabled control animation pause");
            field.period = 17.0f;
            field.cycle = 0.375;
            app.animation_paused = true;
        }
        auto doc = capture_local_fog(app.em);
        require(doc.fog_volumes.size() == 1 && valid_document(doc), "spawn/validation");
        require(doc.fog_volumes[0].control_field.points.size() == 2, "capture control points");
        require(control_field_component(doc.fog_volumes[0].control_field).cycle == 0, "cycle not persisted");
        {
            auto f = doc.fog_volumes[0].control_field;
            f.points.clear(); require(valid_control_field(f), "zero point empty field");
            f.points.resize(fog::max_fog_control_points); require(valid_control_field(f), "32 point limit");
            f.points.emplace_back(); require(!valid_control_field(f), "33 point rejection");
            f.points.resize(1);
            f.points[0].position[0] = std::numeric_limits<float>::infinity();
            require(!valid_control_field(f), "control infinite position rejection");
            f.points[0] = SceneFogControlPoint{};
            f.points[0].frequency = 9; require(!valid_control_field(f), "frequency rejection");
            f.points[0].frequency = 1;
            f.period = 0; require(!valid_control_field(f), "zero period rejection");
            f.period = 0.1f;
            f.noise_scale = 0.1f; f.octaves = 1; f.seed = 0;
            require(valid_control_field(f), "control noise minimum bounds");
            f.noise_scale = 32.0f; f.octaves = 5; f.seed = 65535;
            require(valid_control_field(f), "control noise maximum bounds");
            f.noise_scale = 0.09f; require(!valid_control_field(f), "noise scale lower rejection");
            f.noise_scale = 32.01f; require(!valid_control_field(f), "noise scale upper rejection");
            f.noise_scale = 6;
            f.octaves = 0; require(!valid_control_field(f), "octaves lower rejection");
            f.octaves = 6; require(!valid_control_field(f), "octaves upper rejection");
            f.octaves = 3;
            f.seed = -1; require(!valid_control_field(f), "seed lower rejection");
            f.seed = 65536; require(!valid_control_field(f), "seed upper rejection");
        }
        for(auto&& [entity, medium] : app.em.view<fog::FogVolume>()) app.local_fog_target = entity.id;
        app.edit_local_fog(4);
        require(app.em.view<fog::FogVolume>().count() == 2, "duplicate");
        for(auto&& [entity, field] : app.em.view<fog::FogControlField>())
            require(field.points.size() == 2 && field.cycle == 0.375, "duplicate control component");
        app.edit_local_fog(6);
        for(auto&& [entity, transform, medium] : app.em.view<fog::Transform, fog::FogVolume>()) {
            if(entity.id != app.local_fog_target) continue;
            require(glm::length(transform.position - app.camera.transform().position
                - app.camera.transform().view_direction() * 8.0f) < 1e-5f, "move in front");
        }
        const auto previous_camera = app.camera.transform().position;
        app.edit_local_fog(7);
        require(app.camera.transform().position != previous_camera, "camera move near");
        app.save_local_fog(path);
        require(std::filesystem::exists(path), "save");
        const auto saved_text = alib5::to_adata(capture_local_fog(app.em)).str(alib5::data::JSON());
        app.edit_local_fog(5);
        require(app.em.view<fog::FogVolume>().count() == 1, "delete");
        app.load_local_fog(path);
        if(app.em.view<fog::FogVolume>().count() != 2) {
            LocalFogDocument diagnostic;
            const auto result = alib5::compact::load_config_from_file(path.string(), diagnostic);
            std::cerr << "Editor load diagnostics: " << app.editor_status << std::endl;
            for(const auto& error : result.recorded_errors) std::cerr << error << std::endl;
        }
        require(app.em.view<fog::FogVolume>().count() == 2, "load");
        require(alib5::to_adata(capture_local_fog(app.em)).str(alib5::data::JSON()) == saved_text, "roundtrip");
        {
            auto invalid = capture_local_fog(app.em);
            invalid.fog_volumes[0].control_field.points.resize(fog::max_fog_control_points + 1);
            { std::ofstream out(path); out << alib5::to_adata(invalid).str(alib5::data::JSON({.float_precision = 47})); }
            app.load_local_fog(path);
            require(alib5::to_adata(capture_local_fog(app.em)).str(alib5::data::JSON()) == saved_text,
                "invalid control field preserves scene");
        }
        app.save_local_fog(path);
        require(std::filesystem::exists(path.string() + ".bak"), "backup");
        { std::ofstream out(path); out << "{invalid"; }
        app.load_local_fog(path);
        require(app.em.view<fog::FogVolume>().count() == 2, "invalid file preserves scene");
        doc.version = 2; require(!valid_document(doc), "version rejection"); doc.version = 1;
        doc.fog_volumes[0].scale[0] = 0; require(!valid_document(doc), "scale rejection");
        doc.fog_volumes[0].scale[0] = 1;
        doc.fog_volumes[0].density = std::numeric_limits<float>::quiet_NaN();
        require(!valid_document(doc), "NaN rejection");
        doc.fog_volumes[0].density = 0.15f;
        doc.fog_volumes[0].texture.path = "/tmp/opencode/nonexistent-fog-texture.raw";
        require(!valid_document(doc), "missing texture rejection");
        doc.fog_volumes[0].texture.path = "builtin:cloud";
        doc.fog_volumes.resize(max_local_fog_volumes + 1, doc.fog_volumes[0]);
        require(!valid_document(doc), "capacity rejection");
        app.local_fog_target = app.camera.get_entity().id;
        app.edit_local_fog(5);
        require(app.camera.is_valid(), "non-fog protection");
        // Preset tabs replace only fog/light state and preserve each tab's live edits.
        const auto default_layout = alib5::to_adata(capture_local_fog(app.em)).str(alib5::data::JSON());
        const auto default_density = app.cfg.fog.density;
        const auto default_camera = app.camera.transform().position;
        const auto default_phase = app.fog_phase;
        const auto default_paused = app.animation_paused;
        for(auto&& [entity, field] : app.em.view<fog::FogControlField>()) field.cycle = 0.375;
        app.switch_scene_preset(1);
        require(app.scene_preset == 1 && app.cfg.fog.density < 0.003f
            && app.cfg.fog.mist_density < 0.003f, "local mist has weak global fog");
        require(app.em.view<fog::FogControlField>().count() == 2, "local mist creates two clouds");
        for(auto&& [entity, field] : app.em.view<fog::FogControlField>())
            require(field.enabled && field.animated && field.points.size() == 6, "preset rolling control fields");
        require(valid_document(capture_local_fog(app.em)), "local mist preset valid");
        app.cfg.fog.density = 0.0023f;
        const auto preset_count = app.em.entity_count();
        app.switch_scene_preset(1);
        require(app.cfg.fog.density == 0.0023f && app.em.entity_count() == preset_count,
            "active tab does not reset edits or spawn duplicates");
        app.switch_scene_preset(2);
        require(valid_document(capture_local_fog(app.em)), "ground mist preset valid");
        for(auto&& [entity, transform, medium] : app.em.view<fog::Transform, fog::FogVolume>())
            require(transform.scale.y < 3.0f, "ground mist remains shallow");
        app.switch_scene_preset(3);
        require((app.local_lights.empty() || (app.local_lights[0].enabled
                && app.local_lights[0].color.r > app.local_lights[0].color.b))
            && valid_document(capture_local_fog(app.em)), "warm light preset valid");
        app.switch_scene_preset(1);
        require(app.cfg.fog.density == 0.0023f, "tab edits survive return");
        app.switch_scene_preset(0);
        require(app.cfg.fog.density == default_density
            && app.camera.transform().position == default_camera && app.fog_phase == default_phase
            && app.animation_paused == default_paused, "default environment and camera preserved");
        for(auto&& [entity, field] : app.em.view<fog::FogControlField>())
            require(field.cycle == 0.375, "tab restores control animation phase");
        require(alib5::to_adata(capture_local_fog(app.em)).str(alib5::data::JSON()) == default_layout,
            "default local fog restored exactly");
        app.switch_scene_preset(4);
        require(app.scene_preset == 4 && app.em.view<fog::FogVolume>().count() >= 3,
            "cloud sea preset has animated banks");
        app.switch_scene_preset(0);
        require(app.scene_preset == 0, "default restored after cloud sea");
        app.switch_scene_preset(-1);
        app.switch_scene_preset(scene_preset_count);
        require(app.scene_preset == 0, "invalid preset ignored");
        std::filesystem::remove(path);
        std::filesystem::remove(path.string() + ".bak");
        std::cout << "Editor PASS: CRUD/camera, JSON roundtrip/backup, invalid input preservation, capacity/non-fog protection, animation, shadow cache, ImGui IDs/collapse" << std::endl;
        return 0;
    } catch(const std::exception& e) {
        std::cerr << "Editor FAIL: " << e.what() << std::endl;
        return 1;
    }
}
