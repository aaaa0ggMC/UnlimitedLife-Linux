/**
 * @file app_scene.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 场景加载
 * @version 5.0
 * @date 2026-10-14
 * 
 * @copyright Copyright (c) 2026
 * @parblock audit AI不能修改 
 * 1. OK 没变啥
 * @endparblock
 * 
 */
#include "app.h"
#include <cstdlib>

namespace {

[[nodiscard]] inline glm::vec3 to_glm(const SceneVec& v) { return { v[0], v[1], v[2] }; }

} // namespace

auto App::setup_world() -> void {
    sunlight.add<fog::Tag>("sunlight");
    sunlight_ref->ambient = std::clamp(cfg.lighting.ambient, 0.0f, 1.0f);
    sunlight_ref->specular = cfg.lighting.specular;
    const fog::Renderable cube {
        .vertices = cube_vertices, .indices = cube_indices, .index_count = 36, .solid = true
    };

    // 场景描述运行期从 scene.json 读取：字段必须齐全，缺项/类型不符由 validator 报错。
    // FOG_SCENE 可指向精简场景（例如 mascot 只保留 dragon），不修改默认 scene.json。
    const std::string scene_path = [](const char* name, std::string_view fallback) {
        if(const char* value = std::getenv(name); value && *value) return std::string(value);
        return std::string(fallback);
    }("FOG_SCENE", scene_file);
    SceneDesc scene{};
    if(const auto result = alib5::compact::load_config_from_file(scene_path, scene); !result.success) {
        // 系统性错误会命中所有实例，诊断可能上千条：只报前几条 + 总数
        constexpr std::size_t shown = 4;
        std::string report;
        for(const auto& error : result.recorded_errors | std::views::take(shown)) {
            std::format_to(std::back_inserter(report), "\n  - {}", error);
        }
        if(const auto total = result.recorded_errors.size(); total > shown) {
            std::format_to(std::back_inserter(report), "\n  ... and {} more", total - shown);
        }
        panicf("scene file '{}' is invalid ({} diagnostics):{}", scene_path,
                      result.recorded_errors.size(), report);
    }

    // Validate all control fields before creating any scene entities.
    for(const auto& entry : scene.fog_volumes) {
        if(!valid_control_field(entry.control_field))
            panicf("scene.json: invalid fog control field '{}'.", entry.tag);
    }

    // 法线图层与材质名绑定；场景 JSON 不需要增加字段，新材质默认不加法线。
    const auto normal_layer_of = [](std::string_view name) {
        if(name == "grass") return 1;
        if(name == "stone") return 2;
        if(name == "wood") return 3;
        if(name == "leaves") return 4;
        if(name == "sand") return 5;
        if(name == "roof") return 6;
        return 0;
    };
    std::unordered_map<std::string, fog::SurfaceMaterial> materials;
    materials.reserve(scene.materials.size());
    for(const auto& [name, m] : scene.materials) {
        materials.emplace(name, fog::SurfaceMaterial{
            .color = to_glm(m.color),
            .specular = m.specular,
            .shininess = m.shininess,
            .emission = m.emission,
            .voxel = m.voxel,
            .normal_layer = normal_layer_of(name)
        });
    }
    local_lights.clear();
    local_lights.reserve(scene.lights.size());
    for(const auto& entry : scene.lights) {
        if(local_lights.size() >= max_local_lights) {
            lg(alib5::Severity::Warn) << "scene.json: only " << max_local_lights
                << " local point lights are supported per frame; ignoring the rest." << std::endl;
            break;
        }
        local_lights.push_back(LocalLight{
            .tag = entry.tag,
            .position = to_glm(entry.pos),
            .color = to_glm(entry.color),
            .intensity = entry.intensity,
            .range = entry.range,
            .radius = entry.radius,
            .enabled = entry.enabled,
            .cast_shadow = entry.shadow,
        });
    }

    const auto material_of = [&](const std::string& name) -> const fog::SurfaceMaterial& {
        const auto it = materials.find(name);
        if(it == materials.end()) {
            panicf("scene.json: unknown material \"{}\".", name);
        }
        return it->second;
    };

    // 实例化方块：数量与布局全部来自 JSON（走的都是共享 cube 几何的 instance 批次）
    for(const auto& entry : scene.cubes) {
        fog::Renderable renderable = cube;
        renderable.solid = entry.solid;
        renderable.cast_shadow = entry.shadow;
        const auto& material = material_of(entry.material);
        // 自发光方块（火把焰、灯笼玻璃）和显式 no-shadow 的方块（火把杆）走独立绘制：
        // 它们不能把自己所在的点光挡在阴影里。
        fog::ObjectEntity object(em, entry.tag, renderable, to_glm(entry.pos), to_glm(entry.axis),
                                 entry.angle, material, material.emission <= 0.0f && renderable.cast_shadow);
        object.transform().scale = to_glm(entry.scale);
        object.transform().dm_mark();
    }

    // Dragon：摆放同样来自 JSON，模型没加载成功就只留基座
    if(shape_index_count > 0) {
        const auto& dragon = scene.dragon;
        fog::ObjectEntity object(em, dragon.tag, fog::Renderable{
            .vertices = shape_vertices, .indices = shape_indices, .index_count = shape_index_count,
            .solid = dragon.solid
        }, to_glm(dragon.pos), to_glm(dragon.axis), dragon.angle, material_of(dragon.material), false);
        object.transform().scale = to_glm(dragon.scale);
        object.transform().dm_mark();
    }

    for(const auto& entry : scene.fog_volumes) {
        const auto size = to_glm(entry.scale);
        if(entry.shape < 0 || entry.shape > 1 || glm::any(glm::lessThanEqual(size, glm::vec3(0.0f)))
            || !std::isfinite(entry.density) || entry.density < 0.0f) {
            panicf("scene.json: invalid fog volume '{}': check shape, positive scale and density.", entry.tag);
        }
        fog::FogVolumeEntity object(em, entry.tag,
            { entry.shape, entry.density, to_glm(entry.color), entry.albedo, entry.edge_fade, entry.enabled },
            { entry.texture.path, entry.texture.extent, to_glm(entry.texture.tiling),
              to_glm(entry.texture.offset), entry.texture.strength, entry.texture.repeat },
            to_glm(entry.pos), size, to_glm(entry.axis), entry.angle);
        object.add<fog::FogControlField>(control_field_component(entry.control_field));
    }

    // 天空盒：场景唯一，直接持有，绘制时无需再用 view 遍历
    skybox.emplace(em, fog::Skybox{
        .exposure = std::max(cfg.sky.exposure, 0.0f), .textured = !cfg.sky.texture.empty()
    }, cube);
    lg << "Local point lights: " << local_lights.size() << " (cube shadow budget "
       << max_shadow_lights << ")." << std::endl;
    lg << "Scene ready: " << em.entity_count() << " entities, "
       << em.view<fog::Transform, fog::Renderable, fog::SurfaceMaterial>().count()
       << " renderables. Voxel island + sun shadow + point light cube shadows + froxel scattering." << std::endl;
}

auto App::setup_instances() -> void {
    std::vector<PushConstant> opaque;
    std::vector<PushConstant> water;
    em.view<fog::Transform, fog::Renderable, fog::SurfaceMaterial, fog::StaticInstance>().for_each(
        [&](fog::Transform& transform, fog::Renderable& renderable, fog::SurfaceMaterial& material, auto&) {
            if(!renderable.vertices.same_region(cube_vertices)
                || !renderable.indices.same_region(cube_indices)
                || renderable.index_count != 36) {
                panicf("Static instance batch accepts only the shared cube geometry.");
            }
            auto instance = make_draw_data(transform, renderable, material);
            (material.voxel == 5 ? water : opaque).push_back(instance);
        }
    );
    if(opaque.size() > std::numeric_limits<std::uint32_t>::max()
        || water.size() > std::numeric_limits<std::uint32_t>::max()) {
        panicf("Static instance count exceeds indexed draw limits.");
    }

    const auto upload = [&](std::vector<PushConstant> instances,
                            std::optional<ave::VMABuffer>& buffer,
                            std::uint32_t& count, const char* label) {
        count = static_cast<std::uint32_t>(instances.size());
        // 即使空批次也创建有效 descriptor backing，不进行零尺寸分配。
        if(instances.empty()) instances.push_back(PushConstant{ .model = glm::mat4(1.0f) });
        buffer.emplace(ave::CreateVMABufferInfo{
            .allocator = allocator,
            .size = sizeof(PushConstant) * instances.size(),
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .host_access = ave::BufferHostAccess::DeviceOnly
        });
        // 实例数据量与模型大小无关：不能借用可能只有一个 cube 大小的几何 staging。
        ave::VMABuffer staging(ave::CreateVMABufferInfo{
            .allocator = allocator,
            .size = sizeof(PushConstant) * instances.size(),
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .host_access = ave::BufferHostAccess::SequentialWrite
        });
        if(!buffer->upload(instances, 0, ave::MapBufferInfo{
            .staging = &staging, .upload_context = renderer->get_upload_context()
        })) panicf("{} upload failed.", label);
        lg << label << ": " << count << " instances, "
           << sizeof(PushConstant) * count << " bytes uploaded." << std::endl;
    };
    upload(std::move(opaque), instance_buffer, instance_count, "Static batch");
    upload(std::move(water), water_instance_buffer, water_instance_count, "Water batch");
    // upload() 已等待复制完成，启动用的 staging 不需要常驻。
    staging_buffer.reset();
}

/// 实例批重建：Inspector 改了 Transform / SurfaceMaterial 后，重新生成并上传（不重建设备缓冲）
auto App::refresh_instances() -> void {
    if(!instance_buffer || !water_instance_buffer) return;

    std::vector<PushConstant> opaque;
    std::vector<PushConstant> water;
    opaque.reserve(instance_count);
    water.reserve(water_instance_count);
    em.view<fog::Transform, fog::Renderable, fog::SurfaceMaterial, fog::StaticInstance>().for_each(
        [&](fog::Transform& transform, fog::Renderable& renderable, fog::SurfaceMaterial& material, auto&) {
            if(!renderable.vertices.same_region(cube_vertices)
                || !renderable.indices.same_region(cube_indices)
                || renderable.index_count != 36) {
                panicf("Static instance batch accepts only the shared cube geometry.");
            }
            auto instance = make_draw_data(transform, renderable, material);
            (material.voxel == 5 ? water : opaque).push_back(instance);
        }
    );
    // 编辑 voxel 可能让方块在水/不透明批次之间移动：重新分配并重建描述符。
    if(opaque.size() != instance_count || water.size() != water_instance_count) {
        setup_instances();
        setup_ubo();
        return;
    }

    const auto upload = [&](std::vector<PushConstant>& instances,
                            std::optional<ave::VMABuffer>& buffer, const char* label) {
        if(instances.empty()) instances.push_back(PushConstant{ .model = glm::mat4(1.0f) });
        ave::VMABuffer staging(ave::CreateVMABufferInfo{
            .allocator = allocator,
            .size = sizeof(PushConstant) * instances.size(),
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .host_access = ave::BufferHostAccess::SequentialWrite
        });
        if(!buffer->upload(instances, 0, ave::MapBufferInfo{
            .staging = &staging, .upload_context = renderer->get_upload_context()
        })) panicf("{} refresh upload failed.", label);
    };
    upload(opaque, instance_buffer, "Static batch");
    upload(water, water_instance_buffer, "Water batch");
}
