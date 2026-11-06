#include <fstream>
#include "app.h"
#include <cstring>

namespace {
GpuFogControlField control_field_data(const fog::FogControlField& field) {
    GpuFogControlField gpu;
    gpu.info = glm::uvec4(std::min(field.points.size(), std::size_t(fog::max_fog_control_points)),
        std::clamp(field.seed, 0, 65535), std::clamp(field.octaves, 1, 5), 0u);
    gpu.noise = glm::vec4(std::clamp(field.noise_scale, 0.1f, 32.0f),
        std::clamp(field.noise_strength, 0.0f, 1.0f), 0.0f, 0.0f);
    const double phase = field.animated ? field.cycle * 6.283185307179586 : 0.0;
    if(field.animated) gpu.noise_offset = glm::vec4(
        0.35f * float(std::sin(phase)), 0.35f * float(std::cos(phase) - 1.0),
        0.20f * float(std::sin(2.0 * phase)), 0.0f);
    for(unsigned i = 0; i < gpu.info.x; ++i) {
        const auto& point = field.points[i];
        glm::vec3 position = glm::clamp(point.position, glm::vec3(0.0f), glm::vec3(1.0f));
        if(field.animated) {
            // Limit the orbit, not the animated position: no clipped, sticking motion.
            const auto amplitude = glm::min(glm::clamp(point.amplitude, glm::vec3(0.0f), glm::vec3(0.5f)),
                glm::min(position, glm::vec3(1.0f) - position));
            const double angle = phase * std::clamp(point.frequency, 1, 8) + point.phase;
            position += amplitude * glm::vec3(float(std::sin(angle)), float(std::cos(angle)),
                float(std::sin(angle + 1.570796326794897)));
        }
        gpu.points[i].position_radius = glm::vec4(position, std::clamp(point.radius, 0.02f, 1.0f));
        gpu.points[i].density_softness = glm::vec4(std::clamp(point.density, 0.0f, 4.0f),
            std::clamp(point.softness, 0.05f, 1.0f), 0.0f, 0.0f);
    }
    return gpu;
}

FogTextureKey texture_key(const fog::FogDensityTexture& texture) {
    return { texture.path, texture.path == "builtin:cloud" ? std::array<int, 3>{32, 32, 32} : texture.extent };
}

std::vector<std::uint8_t> density_pixels(const FogTextureKey& key) {
    const auto& size = key.extent;
    for(int dimension : size) {
        if(dimension < 1 || dimension > 128) panicf("Fog texture dimensions must be 1..128: {}", key.path);
    }
    const std::size_t count = std::size_t(size[0]) * size[1] * size[2];
    std::vector<std::uint8_t> pixels(count);
    if(key.path == "builtin:cloud") {
        for(int z = 0; z < size[2]; ++z) for(int y = 0; y < size[1]; ++y) for(int x = 0; x < size[0]; ++x) {
            const glm::vec3 p = glm::vec3(x, y, z) * (6.2831853f / 31.0f);
            const float n = 0.5f + 0.25f * std::sin(p.x + std::sin(p.y)) * std::cos(p.z)
                + 0.18f * std::cos(2.0f * p.y + p.z) * std::sin(2.0f * p.x - p.z);
            pixels[(z * size[1] + y) * size[0] + x] = static_cast<std::uint8_t>(std::clamp(n, 0.0f, 1.0f) * 255.0f);
        }
        return pixels;
    }
    std::ifstream file(key.path, std::ios::binary | std::ios::ate);
    if(!file || static_cast<std::streamoff>(file.tellg()) != static_cast<std::streamoff>(count)) {
        panicf("Fog texture must be a raw R8 file with exactly {} bytes: {}", count, key.path);
    }
    file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(pixels.data()), static_cast<std::streamsize>(count)))
        panicf("Cannot read fog density texture: {}", key.path);
    return pixels;
}

float resample_density(const std::vector<std::uint8_t>& pixels, const std::array<int, 3>& size,
                       int x, int y, int z) {
    const glm::vec3 p = glm::vec3(x, y, z) / float(fog_texture_size - 1)
        * glm::vec3(size[0] - 1, size[1] - 1, size[2] - 1);
    const auto base = glm::ivec3(glm::floor(p));
    const auto f = p - glm::vec3(base);
    float value = 0.0f;
    for(int k = 0; k < 2; ++k) for(int j = 0; j < 2; ++j) for(int i = 0; i < 2; ++i) {
        const auto q = glm::min(base + glm::ivec3(i, j, k), glm::ivec3(size[0] - 1, size[1] - 1, size[2] - 1));
        const glm::vec3 w { i ? f.x : 1.0f - f.x, j ? f.y : 1.0f - f.y, k ? f.z : 1.0f - f.z };
        value += pixels[(q.z * size[1] + q.y) * size[0] + q.x] * w.x * w.y * w.z;
    }
    return value;
}
} // namespace

auto App::rebuild_local_fog_atlas(const std::vector<FogTextureKey>& textures) -> void {
    if(textures.size() > max_local_fog_textures) panicf("At most {} unique local fog textures are supported.", max_local_fog_textures);
    if(renderer->wait_idle() != VK_SUCCESS) panicf("Cannot rebuild local fog texture atlas safely.");
    const VkExtent3D extent { fog_texture_tile, fog_texture_tile, fog_texture_tile * max_local_fog_textures };
    const auto limit = renderer->device->get_physical_device_limits().maxImageDimension3D;
    if(extent.width > limit || extent.height > limit || extent.depth > limit)
        panicf("Device 3D image limit cannot host local fog atlas.");
    const auto required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT
        | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    if(!renderer->device->supports_format(VK_FORMAT_R8_UNORM, required))
        panicf("R8 linear 3D fog sampling is not supported.");
    std::vector<std::uint8_t> atlas(std::size_t(extent.width) * extent.height * extent.depth, 255);
    for(unsigned slot = 0; slot < textures.size(); ++slot) {
        const auto pixels = density_pixels(textures[slot]);
        for(unsigned z = 0; z < fog_texture_tile; ++z) for(unsigned y = 0; y < fog_texture_tile; ++y)
            for(unsigned x = 0; x < fog_texture_tile; ++x) {
                const int sx = std::clamp(int(x) - 1, 0, int(fog_texture_size) - 1);
                const int sy = std::clamp(int(y) - 1, 0, int(fog_texture_size) - 1);
                const int sz = std::clamp(int(z) - 1, 0, int(fog_texture_size) - 1);
                const auto value = resample_density(pixels, textures[slot].extent, sx, sy, sz);
                atlas[((slot * fog_texture_tile + z) * extent.height + y) * extent.width + x]
                    = static_cast<std::uint8_t>(std::clamp(value, 0.0f, 255.0f));
            }
    }
    auto image = ave::Image::create({
        .allocator = allocator,
        .image_type = VK_IMAGE_TYPE_3D,
        .format = VK_FORMAT_R8_UNORM,
        .extent = extent,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    });
    if(!image) panicf("Cannot create 3D fog density atlas.");
    if(!local_fog_sampler) local_fog_sampler = ave::Sampler::create({
        .device = renderer->device,
        .address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    });
    if(!local_fog_sampler) panicf("Cannot create local fog sampler.");
    ave::VMABuffer staging(ave::CreateVMABufferInfo{
        .allocator = allocator, .size = atlas.size(), .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .host_access = ave::BufferHostAccess::SequentialWrite,
    });
    if(!staging.upload(atlas)) panicf("Cannot upload local fog atlas staging data.");
    auto* upload = renderer->get_upload_context();
    if(!upload) panicf("Local fog atlas requires an upload context.");
    const auto ticket = upload->submit_copy_to_image({
        .src = staging.get_system_handle(), .dst = image->get_system_handle(), .extent = extent,
        .final_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    });
    if(!ticket || !ticket.wait()) panicf("Cannot finish local fog 3D texture upload.");
    local_fog_atlas = std::move(image);
    local_fog_textures = textures;
    ++history_epoch;
    // Descriptor edits are safe here: all frame fences have completed above.
    if(scene_descriptor_pool) {
        const auto write = ave::DescriptorWrite::image(10, *local_fog_sampler, *local_fog_atlas);
        for(auto set : descriptor_froxel) scene_descriptor_pool->write_set(set, { write });
        for(auto set : descriptor_cubes) scene_descriptor_pool->write_set(set, { write });
        for(const auto& sets : descriptor_integrate)
            for(auto set : sets) scene_descriptor_pool->write_set(set, { write });
    }
}

auto App::sync_local_fog() -> void {
    std::vector<FogTextureKey> textures;
    for(auto&& [entity, texture] : em.view<fog::FogDensityTexture>()) {
        if(texture.path.empty() || !em.get_component<fog::FogVolume>(entity).has_value()
            || !em.get_component<fog::Transform>(entity).has_value()) continue;
        if(const auto* field = em.get_component_raw<fog::FogControlField>(entity); field && field->enabled) continue;
        const auto key = texture_key(texture);
        if(std::ranges::find(textures, key) == textures.end()) textures.push_back(key);
    }
    std::ranges::sort(textures, [](const auto& a, const auto& b) {
        return std::tie(a.path, a.extent) < std::tie(b.path, b.extent);
    });
    if(!local_fog_atlas || textures != local_fog_textures) rebuild_local_fog_atlas(textures);

    std::vector<std::pair<std::uint64_t, GpuFogVolume>> entries;
    std::vector<std::pair<std::uint64_t, GpuFogControlField>> fields;
    std::vector<std::pair<std::uint64_t, fog::FogControlField>> settings;
    for(auto&& [entity, transform, medium] : em.view<fog::Transform, fog::FogVolume>()) {
        if(!medium.enabled || medium.density <= 0.0f) continue;
        const auto model = transform.build_model_matrix();
        const float determinant = glm::determinant(model);
        if(!std::isfinite(determinant) || std::abs(determinant) < 1e-8f) continue;
        GpuFogVolume gpu;
        gpu.world_to_local = glm::inverse(model);
        gpu.scattering = glm::vec4(glm::clamp(medium.color, glm::vec3(0.0f), glm::vec3(1.0f)),
            std::clamp(medium.albedo, 0.0f, 1.0f));
        gpu.params = glm::vec4(std::max(medium.density, 0.0f), std::clamp(medium.edge_fade, 0.0f, 1.0f),
            std::clamp(medium.shape, 0, 1), 0.0f);
        gpu.bounds = glm::vec4(transform.position, glm::length(transform.scale) * 0.5f);
        const auto* field = em.get_component_raw<fog::FogControlField>(entity);
        if(field && field->enabled) {
            fields.emplace_back(entity.id, control_field_data(*field));
            settings.emplace_back(entity.id, *field);
            settings.back().second.cycle = 0.0; // Animation is handled by temporal response, not invalidation.
        } else if(const auto* texture = em.get_component_raw<fog::FogDensityTexture>(entity); texture && !texture->path.empty()) {
            const auto key = texture_key(*texture);
            const auto slot = std::ranges::find(local_fog_textures, key) - local_fog_textures.begin();
            const glm::vec3 atlas_extent(fog_texture_tile, fog_texture_tile, fog_texture_tile * max_local_fog_textures);
            gpu.tex_tiling = glm::vec4(texture->tiling, texture->repeat ? 1.0f : 0.0f);
            gpu.tex_offset = glm::vec4(texture->offset, 0.0f);
            gpu.params.w = std::clamp(texture->strength, 0.0f, 1.0f);
            gpu.atlas_offset = glm::vec4(glm::vec3(1.5f, 1.5f, float(slot * fog_texture_tile) + 1.5f) / atlas_extent, 1.0f);
            gpu.atlas_scale = glm::vec4(glm::vec3(fog_texture_size - 1) / atlas_extent, 0.0f);
        }
        entries.emplace_back(entity.id, gpu);
    }
    if(entries.size() > max_local_fog_volumes) panicf("At most {} active local fog volumes are supported.", max_local_fog_volumes);
    std::ranges::sort(entries, {}, &std::pair<std::uint64_t, GpuFogVolume>::first);
    LocalFogUpload snapshot;
    snapshot.counts.x = static_cast<unsigned>(entries.size());
    FogControlUpload controls {};
    for(unsigned i = 0; i < entries.size(); ++i) {
        snapshot.entries[i] = entries[i].second;
        const auto field = std::ranges::find(fields, entries[i].first,
            &std::pair<std::uint64_t, GpuFogControlField>::first);
        if(field != fields.end()) {
            controls[i] = field->second;
            snapshot.entries[i].atlas_scale.w = float(i + 1);
            snapshot.counts.y |= 1u << i;
        }
        if(std::memcmp(&control_fog_snapshot[i], &controls[i], sizeof(GpuFogControlField)) != 0)
            ++control_fog_revisions[i];
        // Revision zero is reserved for uninitialized per-image GPU caches.
        if(control_fog_revisions[i] == 0) control_fog_revisions[i] = 1;
    }
    control_fog_snapshot = controls;
    std::ranges::sort(settings, {}, &std::pair<std::uint64_t, fog::FogControlField>::first);
    if(settings != control_fog_settings) {
        control_fog_settings = std::move(settings);
        ++history_epoch; // Also catches programmatic edits outside the inspector.
    }
    if(!local_fog_snapshot || std::memcmp(&*local_fog_snapshot, &snapshot, sizeof(snapshot)) != 0) {
        local_fog_snapshot = snapshot;
        ++local_fog_revision;
        ++history_epoch;
    }
}

auto App::spawn_local_fog(int shape) -> void {
    if(em.view<fog::FogVolume>().count() >= max_local_fog_volumes) {
        editor_status = "最多保留 16 个局部雾实体（含禁用项）";
        lg(alib5::Severity::Warn) << "Local fog entity limit reached." << std::endl;
        return;
    }
    fog::FogVolume medium;
    medium.shape = shape == 2 ? 0 : std::clamp(shape, 0, 1);
    fog::FogDensityTexture texture;
    if(shape == 1) texture.path = "builtin:cloud";
    fog::FogVolumeEntity object(em, std::format("local-fog-{}", ++local_fog_serial), medium, texture,
        camera.transform().position + camera.transform().view_direction() * 8.0f, glm::vec3(6.0f, 3.0f, 6.0f));
    if(shape == 2) {
        auto field = object.add<fog::FogControlField>();
        field->points.resize(3);
        field->points[0].position = glm::vec3(0.30f, 0.45f, 0.40f);
        field->points[1].position = glm::vec3(0.55f, 0.55f, 0.50f);
        field->points[2].position = glm::vec3(0.70f, 0.40f, 0.65f);
        field->points[1].phase = 2.0f;
        field->points[2].phase = 4.0f;
    }
}
