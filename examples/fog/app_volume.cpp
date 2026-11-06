#include "app.h"

namespace {
constexpr std::array<unsigned, 1> shadow_sizes { 2048 };
constexpr unsigned query_count = 6;

struct alignas(16) ShadowPush {
    glm::mat4 model { 1.0f };
    glm::mat4 light_view_proj { 1.0f };
};
static_assert(sizeof(ShadowPush) == 128);

void compute_dependency(VkCommandBuffer commands, VkPipelineStageFlags destination) {
    ave::memory_barrier(commands, {
        .src_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .src_access = VK_ACCESS_SHADER_WRITE_BIT,
        .dst_stage = destination,
        .dst_access = VK_ACCESS_SHADER_READ_BIT,
    });
}
} // namespace

auto App::setup_shadows() -> void {
    shadow_pipeline.reset();
    shadow_instanced_pipeline.reset();
    for(auto& render : shadow_renders) render.reset();
    for(auto& images : shadow_images) images.clear();
    point_shadow_sampler.reset();
    for(auto& slot : point_shadow_renders) for(auto& render : slot) render.reset();
    point_shadow_images.clear();
    point_shadow_array_views.clear();
    point_shadow_layer_views.clear();
    point_shadow_cache.clear();
    const auto count = renderer->swapchain->get_image_count();
    shadow_cache.assign(count, {});
    VkFormat depth_format = VK_FORMAT_UNDEFINED;
    const auto required = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    for(const auto format : { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM }) {
        if(renderer->device->supports_format(format, required)) { depth_format = format; break; }
    }
    if(depth_format == VK_FORMAT_UNDEFINED) panicf("No sampled depth format for fog shadows.");
    shadow_sampler = ave::Sampler::create({
        .device = renderer->device,
        .mag_filter = VK_FILTER_NEAREST,
        .min_filter = VK_FILTER_NEAREST,
        .mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .border_color = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
    });
    if(!shadow_sampler) panicf("Cannot create shadow sampler.");
    for(unsigned light = 0; light < shadow_sizes.size(); ++light) {
        ave::CreateLegacyRenderInfo ci;
        ci.swapchain = renderer->swapchain;
        ci.attachments = {{
            .format = depth_format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        }};
        ci.subpasses = {{ .depth_stencil_attachment = VkAttachmentReference{
            0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL } }};
        ci.dependencies = {
            { VK_SUBPASS_EXTERNAL, 0,
              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
              VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 0 },
            { 0, VK_SUBPASS_EXTERNAL,
              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, 0 }
        };
        ci.default_clear_values = { VkClearValue{ .depthStencil = { 1.0f, 0 } } };
        ci.framebuffer_extent = { shadow_sizes[light], shadow_sizes[light] };
        for(unsigned i = 0; i < count; ++i) {
            auto image = ave::Image::create({
                .allocator = allocator,
                .format = depth_format,
                .extent = { shadow_sizes[light], shadow_sizes[light], 1 },
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            });
            if(!image) panicf("Cannot allocate shadow image.");
            ci.image_dependencies.push_back(image);
            ci.framebuffer_attachments.push_back({ image->get_image_view() });
            shadow_images[light].push_back(std::move(image));
        }
        shadow_renders[light] = ave::LegacyRender::create(std::move(ci));
        if(!shadow_renders[light]) panicf("Cannot create shadow render pass.");
    }
    // 点光 cube shadow：每张交换链图像一份 2D array（6*max_shadow_lights 层）。
    point_shadow_sampler = ave::Sampler::create({
        .device = renderer->device,
        .mag_filter = VK_FILTER_NEAREST,
        .min_filter = VK_FILTER_NEAREST,
        .mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .border_color = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
    });
    if(!point_shadow_sampler) panicf("Cannot create point shadow sampler.");
    const unsigned shadow_layers = point_shadow_faces * max_shadow_lights;
    point_shadow_images.assign(count, {});
    point_shadow_array_views.assign(count, {});
    point_shadow_layer_views.assign(count, {});
    for(unsigned i = 0; i < count; ++i) {
        auto image = ave::Image::create({
            .allocator = allocator,
            .format = depth_format,
            .extent = { point_shadow_resolution, point_shadow_resolution, 1 },
            .array_layers = shadow_layers,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        });
        if(!image) panicf("Cannot allocate point shadow cube array.");
        point_shadow_images[i] = image;
        point_shadow_array_views[i] = ave::ImageView::create({
            .image = image,
            .view = {
                .view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
                .subresource_range = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, shadow_layers },
            },
        });
        if(!point_shadow_array_views[i]) panicf("Cannot create point shadow array view.");
        point_shadow_layer_views[i].reserve(shadow_layers);
        for(unsigned layer = 0; layer < shadow_layers; ++layer) {
            auto view = ave::ImageView::create({
                .image = image,
                .view = {
                    .view_type = VK_IMAGE_VIEW_TYPE_2D,
                    .subresource_range = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, layer, 1 },
                },
            });
            if(!view) panicf("Cannot create point shadow layer view.");
            point_shadow_layer_views[i].push_back(std::move(view));
        }
    }
    for(unsigned slot = 0; slot < max_shadow_lights; ++slot) {
        for(unsigned face = 0; face < point_shadow_faces; ++face) {
            ave::CreateLegacyRenderInfo ci;
            ci.swapchain = renderer->swapchain;
            ci.attachments = {{
                .format = depth_format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            }};
            ci.subpasses = {{ .depth_stencil_attachment = VkAttachmentReference{
                0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL } }};
            ci.dependencies = {
                { VK_SUBPASS_EXTERNAL, 0,
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                  VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 0 },
                { 0, VK_SUBPASS_EXTERNAL,
                  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, 0 }
            };
            ci.default_clear_values = { VkClearValue{ .depthStencil = { 1.0f, 0 } } };
            ci.framebuffer_extent = { point_shadow_resolution, point_shadow_resolution };
            for(unsigned i = 0; i < count; ++i) {
                ci.framebuffer_attachments.push_back({
                    point_shadow_layer_views[i][slot * point_shadow_faces + face]->get_image_view()
                });
            }
            point_shadow_renders[slot][face] = ave::LegacyRender::create(std::move(ci));
            if(!point_shadow_renders[slot][face]) panicf("Cannot create point shadow render pass.");
        }
    }
    point_shadow_cache.assign(count, {});
    point_shadow_cleared.assign(count, {});

    const auto vertex = ave::vertex_layout<ave::Shape::Vertex>().build();
    const auto configure = [&](ave::CreateLegacyPipelineInfo& ci) {
        for(const auto& binding : vertex.bindings)
            ci.vertex_bindings.push_back(static_cast<VkVertexInputBindingDescription>(binding));
        ci.vertex_attributes = { static_cast<VkVertexInputAttributeDescription>(vertex.attributes.front()) };
        ci.rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
        // AVE 的默认 frontFace 是 CW；本工程的 Vulkan Y 翻转投影下外表面是 CCW。
        ci.rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        ci.rasterization.depthBiasEnable = VK_TRUE;
        ci.rasterization.depthBiasConstantFactor = 1.25f;
        ci.rasterization.depthBiasSlopeFactor = 1.75f;
        ci.descriptor_bindings = ave::parse_descriptor_bindings(
            R"([["ubo",0],["ssbo",2]])", ave::ShaderStage::vertex);
        ci.push_constant_ranges = {{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPush) }};
    };
    shadow_pipeline = shadow_renders[0]->create_graphics_pipeline(
        ave::GraphicsShaderPaths{ "fog/shaders/shadow-vert.spv", "fog/shaders/shadow-frag.spv" }, configure);
    shadow_instanced_pipeline = shadow_renders[0]->create_graphics_pipeline(
        ave::GraphicsShaderPaths{ "fog/shaders/shadow-instanced-vert.spv", "fog/shaders/shadow-frag.spv" }, configure);
    if(!shadow_pipeline || !shadow_instanced_pipeline) panicf("Cannot create shadow pipelines.");

    gpu_queries = std::make_unique<FogGpuQueries>();
    gpu_queries->pool = ave::TimestampQueryPool::create({
        .device = renderer->device,
        .queue_family = renderer->swapchain->get_graphics_queue_family(),
        .count = count * query_count,
    });
    if(!gpu_queries->pool) gpu_status = "Cannot create GPU timestamp pool";
    else if(gpu_queries->pool->get_valid_bits() == 0) {
        gpu_queries->pool.reset();
        gpu_status = "GPU timestamps unsupported by selected queue";
    }
    gpu_queries->submitted.assign(count, false);
}

auto App::record_shadows(VkCommandBuffer commands, unsigned image) -> void {
    const auto set = descriptor_shadow.at(image);
    for(unsigned light = 0; light < shadow_sizes.size(); ++light) {
        const auto& cached = shadow_cache.at(image)[light];
        const glm::mat4& matrix = sun_shadow_matrix;
        const bool enabled = volume.shadows;
        // Each swapchain image owns its maps. Only a completed submission may validate them.
        if(cached.reusable(matrix, geometry_revision, enabled)) continue;
        // 禁用的图也要清空/进入可采样布局：描述符始终指向已初始化的图像。
        shadow_renders[light]->begin(commands, image);
        if(enabled) {
            shadow_pipeline->bind(commands);
            shadow_pipeline->bind_descriptor_set(commands, set);
            em.view<fog::Transform, fog::Renderable, fog::SurfaceMaterial, fog::DirectDraw>().for_each(
                [&](auto& transform, auto& renderable, auto& material, auto&) {
                    if(material.emission > 0.0f || !renderable.cast_shadow) return;
                    const ShadowPush push { transform.build_model_matrix(), matrix };
                    shadow_pipeline->push_constant(commands, push, VK_SHADER_STAGE_VERTEX_BIT);
                    const auto buffer = renderable.vertices.get_system_handle();
                    const auto offset = renderable.vertices.get_offset();
                    vkCmdBindVertexBuffers(commands, 0, 1, &buffer, &offset);
                    vkCmdBindIndexBuffer(commands, renderable.indices.get_system_handle(),
                        renderable.indices.get_offset(), VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(commands, renderable.index_count, 1, 0, 0, 0);
                });
            if(instance_count > 0) {
                shadow_instanced_pipeline->bind(commands);
                shadow_instanced_pipeline->bind_descriptor_set(commands, set);
                const ShadowPush push { glm::mat4(1.0f), matrix };
                shadow_instanced_pipeline->push_constant(commands, push, VK_SHADER_STAGE_VERTEX_BIT);
                const auto buffer = cube_vertices.get_system_handle();
                const auto offset = cube_vertices.get_offset();
                vkCmdBindVertexBuffers(commands, 0, 1, &buffer, &offset);
                vkCmdBindIndexBuffer(commands, cube_indices.get_system_handle(), cube_indices.get_offset(), VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(commands, 36, instance_count, 0, 0, 0);
            }
        }
        shadow_renders[light]->end(commands);
    }
}

auto App::record_point_shadows(VkCommandBuffer commands, unsigned image) -> void {
    if(point_shadow_images.empty()) return;
    const auto set = descriptor_shadow.at(image);
    // Six 90° faces matching lighting.glsl's CUBE_DIR/CUBE_UP tables.
    static const glm::vec3 face_direction[point_shadow_faces] = {
        { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }
    };
    static const glm::vec3 face_up[point_shadow_faces] = {
        { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 }, { 0, -1, 0 }
    };
    for(unsigned slot = 0; slot < max_shadow_lights; ++slot) {
        const unsigned index = shadowed_lights[slot];
        auto& cleared = point_shadow_cleared.at(image).at(slot);
        const bool has_light = index < local_lights.size() && local_lights[index].enabled;
        if(!has_light) {
            // 无灯的 slot 也必须清一次并转入 READ_ONLY；否则描述符仍指向 UNDEFINED 图像。
            if(cleared) continue;
            for(unsigned face = 0; face < point_shadow_faces; ++face) {
                auto& render = point_shadow_renders[slot][face];
                if(!render) continue;
                render->begin(commands, image);
                render->end(commands);
            }
            cleared = true;
            continue;
        }
        const auto& light = local_lights[index];
        auto& cache = point_shadow_cache.at(image).at(slot);
        if(cache.reusable(index, light.position, light.range, geometry_revision)) continue;
        const float far_plane = std::max(light.range, 0.2f);
        auto projection = glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, 0.05f, far_plane);
        projection[1][1] *= -1.0f;
        for(unsigned face = 0; face < point_shadow_faces; ++face) {
            auto& render = point_shadow_renders[slot][face];
            if(!render) continue;
            const glm::mat4 view = glm::lookAtRH(light.position,
                light.position + face_direction[face], face_up[face]);
            const glm::mat4 matrix = projection * view;
            render->begin(commands, image);
            shadow_pipeline->bind(commands);
            shadow_pipeline->bind_descriptor_set(commands, set);
            em.view<fog::Transform, fog::Renderable, fog::SurfaceMaterial, fog::DirectDraw>().for_each(
                [&](auto& transform, auto& renderable, auto& material, auto&) {
                    if(material.emission > 0.0f || !renderable.cast_shadow) return;
                    const ShadowPush push { transform.build_model_matrix(), matrix };
                    shadow_pipeline->push_constant(commands, push, VK_SHADER_STAGE_VERTEX_BIT);
                    const auto buffer = renderable.vertices.get_system_handle();
                    const auto offset = renderable.vertices.get_offset();
                    vkCmdBindVertexBuffers(commands, 0, 1, &buffer, &offset);
                    vkCmdBindIndexBuffer(commands, renderable.indices.get_system_handle(),
                        renderable.indices.get_offset(), VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(commands, renderable.index_count, 1, 0, 0, 0);
                });
            if(instance_count > 0) {
                shadow_instanced_pipeline->bind(commands);
                shadow_instanced_pipeline->bind_descriptor_set(commands, set);
                const ShadowPush push { glm::mat4(1.0f), matrix };
                shadow_instanced_pipeline->push_constant(commands, push, VK_SHADER_STAGE_VERTEX_BIT);
                const auto buffer = cube_vertices.get_system_handle();
                const auto offset = cube_vertices.get_offset();
                vkCmdBindVertexBuffers(commands, 0, 1, &buffer, &offset);
                vkCmdBindIndexBuffer(commands, cube_indices.get_system_handle(), cube_indices.get_offset(), VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(commands, 36, instance_count, 0, 0, 0);
            }
            render->end(commands);
        }
        cache = PointShadowCache{ light.position, light.range, geometry_revision, true, index };
        cleared = true;
    }
}

auto App::record_volume(VkCommandBuffer commands, unsigned image, unsigned output) -> void {
    const auto stamp = [&](unsigned slot) {
        if(gpu_queries->pool) gpu_queries->pool->write(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            image * query_count + slot);
    };
    if(gpu_queries->pool) {
        gpu_queries->pool->reset(commands, image * query_count, query_count);
        gpu_queries->pool->write(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, image * query_count);
    }
    record_shadows(commands, image);
    // 位置切分 + 点光 cube shadow：先算每 cell 的光源掩码，再渲染阴影。
    if(light_cull_pipeline && light_mask_buffers.size() && local_lights_enabled) {
        light_cull_pipeline->bind_descriptor_set(commands, descriptor_light_cull.at(image));
        light_cull_pipeline->dispatch(commands, (froxel_extent.x + 3) / 4,
            (froxel_extent.y + 3) / 4, (froxel_depth + 3) / 4);
        compute_dependency(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }
    record_point_shadows(commands, image);
    compute_dependency(commands, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    stamp(1);
#ifdef __ANDROID__
    if(android_no_volume) {
        // Keep all timestamp slots defined without executing any volume compute pass.
        stamp(2);
        stamp(3);
        stamp(4);
        return;
    }
#endif
    bool generated = false;
    for(unsigned slot = 0; slot < max_local_fog_volumes; ++slot) {
        if(!(local_fog_snapshot->counts.y & (1u << slot))
            || generated_control_fog.at(image)[slot] == control_fog_revisions[slot]) continue;
        control_fog_pipeline->dispatch(commands, descriptor_control_fog.at(image),
            glm::uvec4(slot, 0u, 0u, 0u),
            fog_control_grid_size / 4, fog_control_grid_size / 4, fog_control_grid_size / 4);
        generated = true;
    }
    // Per-image fences protect reuse; publish new density to all sampling stages.
    if(generated) compute_dependency(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    froxel_pipeline->bind_descriptor_set(commands, descriptor_froxel.at(image));
    froxel_pipeline->dispatch(commands, (froxel_extent.x + 3) / 4, (froxel_extent.y + 3) / 4, (froxel_depth + 3) / 4);
    compute_dependency(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    stamp(2);
    temporal_pipeline->bind_descriptor_set(commands, descriptor_temporal.at(image)[output]);
    temporal_pipeline->dispatch(commands, (froxel_extent.x + 3) / 4, (froxel_extent.y + 3) / 4, (froxel_depth + 3) / 4);
    compute_dependency(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    stamp(3);
    integrate_pipeline->bind_descriptor_set(commands, descriptor_integrate.at(image)[output]);
    integrate_pipeline->dispatch(commands, (froxel_extent.x + 7) / 8, (froxel_extent.y + 7) / 8);
    compute_dependency(commands, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    stamp(4);
}

auto App::read_gpu_timings(unsigned image) -> void {
    if(!gpu_queries->pool || !gpu_queries->submitted.at(image)) return;
    std::array<std::uint64_t, query_count> ticks {};
    // 不阻塞 CPU 等待 profiling 结果：未就绪直接跳过本帧。
    if(!gpu_queries->pool->read(ticks, image * query_count)) return;
    const auto bits = gpu_queries->pool->get_valid_bits();
    const auto mask = bits == 64 ? ~std::uint64_t(0) : (std::uint64_t(1) << bits) - 1;
    const double scale = renderer->device->get_physical_device_limits().timestampPeriod * 1e-6;
    const auto ms = [&](unsigned a, unsigned b) { return double((ticks[b] - ticks[a]) & mask) * scale; };
    gpu_status = std::format("GPU ms | shadow {:.2f} | density+inject {:.2f} | temporal {:.2f} | integrate {:.2f} | scene+UI {:.2f} | total {:.2f}",
        ms(0, 1), ms(1, 2), ms(2, 3), ms(3, 4), ms(4, 5), ms(0, 5));
}

auto App::apply_weather() -> void {
    volume.weather = std::clamp(volume.weather, 0, 3);
    if(applied_weather == volume.weather) return;
    applied_weather = volume.weather;
    ++history_epoch;
    if(volume.weather == 0) return; // Custom keeps the current values.
    auto& params = fog.params();
    params.start = 0.5f;
    cfg.fog.anim_period = 90.0f;
    cfg.fog.height_falloff = 0.25f;
    cfg.fog.ground_y = -0.5f;
    if(volume.weather == 1) {
        cfg.fog.density = 0.10f; cfg.fog.mist_density = 0.035f;
        cfg.fog.noise_amount = 0.15f; cfg.fog.noise_scale = 0.06f;
        cfg.fog.anim_amplitude = 0.12f;
        params.color = glm::vec3(0.75f, 0.79f, 0.82f);
    } else if(volume.weather == 2) {
        cfg.fog.density = 0.035f; cfg.fog.mist_density = 0.16f;
        cfg.fog.noise_amount = 1.0f; cfg.fog.noise_scale = 0.14f;
        cfg.fog.anim_amplitude = 0.45f;
        params.color = glm::vec3(0.58f, 0.70f, 0.78f);
    } else {
        cfg.fog.density = 0.008f; cfg.fog.mist_density = 0.035f;
        cfg.fog.noise_amount = 0.55f; cfg.fog.noise_scale = 0.08f;
        cfg.fog.anim_amplitude = 0.20f;
        volume.layer_center = 18.0f; volume.layer_thickness = 5.0f;
        params.color = glm::vec3(0.68f, 0.77f, 0.88f);
    }
}
