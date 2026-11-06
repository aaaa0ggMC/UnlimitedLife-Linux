/**
 * @file app_setup.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 初始化流程
 * @version 5.0
 * @date 2026-10-14
 * 
 * @copyright Copyright (c) 2026
 * @parblock audit history (AI 别写这里)
 * 1. OK 主要是修正代码冗余以及清晰性 
 * 2. OK 基本没变 
 * @endparblock
 */
#include "app.h"
#include "ui_bridge.h"
#include <cstdlib>

App::App(const AppConfig & cfg)
:cfg(cfg)
,lg(logger, "Fog")
,vklg(logger, "Vulkan") {
    logger.append_mod<alib5::lot::Console>("console");

    const auto env_float = [](const char* name, float fallback) {
        if(const char* v = std::getenv(name); v && *v) return std::strtof(v, nullptr);
        return fallback;
    };
    if(const char* v = std::getenv("FOG_MASCOT"); v && *v && std::string_view(v) != "0") {
        mascot.enabled = true;
        mascot.output = std::getenv("FOG_MASCOT_OUT") ? std::getenv("FOG_MASCOT_OUT") : "mascot.tga";
        mascot.orbit_deg = env_float("FOG_MASCOT_ORBIT", mascot.orbit_deg);
        mascot.elevation_deg = env_float("FOG_MASCOT_ELEV", mascot.elevation_deg);
        mascot.distance = env_float("FOG_MASCOT_DIST", mascot.distance);
        mascot.fov_deg = env_float("FOG_MASCOT_FOV", mascot.fov_deg);
        mascot.frames = static_cast<int>(env_float("FOG_MASCOT_FRAMES", float(mascot.frames)));
        mascot.halo = env_float("FOG_MASCOT_HALO", mascot.halo ? 1.0f : 0.0f) > 0.5f;
        ui_visible = false;
    }
}

auto App::setup() -> void {
    setup_create_window();
    setup_renderer();
    setup_geometry();
    setup_pipeline();
    setup_textures();
    setup_world();
    sync_local_fog();
    setup_instances();
    setup_ubo();
    setup_imgui();
}

auto App::setup_create_window() -> void {
    // 初始化窗口与注入事件
    window.emplace(ave::CreateWindowInfo {
        .ctx = context,
        // Dont change this forever, and never ~
        .title = "Fog Simulation",
        .width = cfg.window.width,
        .height = cfg.window.height
    });
    window->bind_input(input);
    // 处理Swapchain重建
    window->on<ave::AfterWindowFramebufferResizeEvent>(
        [this](const auto &){
            if(ave::recreate_swapchain_from_window(context, *renderer, *window)) {
                setup_ubo();
                if(renderer->swapchain) {
                    fog::ui::backend_on_swapchain_recreated(renderer->swapchain->get_image_count());
                }
                lg << "Recreated swapchain." << std::endl;
            }
        }
    );
}

auto App::setup_renderer() -> void {
    ave::ProfileWith with;
    with.configure_instance = [](ave::WithGlobalInput&, ave::CreateInstanceInfo& ci) {
        ci.application_name = "fog";
#if defined(__ANDROID__)
        ci.api_version = ave::ave_vk_1_1;
#else
        ci.api_version = ave::ave_vk_1_4;
#endif
    };
    // 一些toggle, validation_layer / render_mode
#if defined(__ANDROID__)
    with.validation_layer = false;
#else
    if(const char* v = std::getenv("FOG_VALIDATION"); v && std::string_view(v) == "0") {
        with.validation_layer = false;
    }
#endif
    {
        const char* mode = std::getenv("FOG_RENDER_MODE");
        with.try_dynamic_rendering = !(mode && std::string_view(mode) == "legacy");
    }
    // 截帧需要把最终交换链图像复制到 host；桌面/Android 都支持 TRANSFER_SRC。
    with.configure_swapchain = [](ave::WithSwapchain& sw, ave::CreateSwapchainInfo& ci) {
        ave::default_configure_swapchain(sw, ci);
        ci.image_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    };
    with.configure_debug_messenger.emplace().on_message(
        [this](alib5::LogLevel level, std::string_view, std::string_view message) {
            vklg(level) << message << std::endl;
        }
    );

    ave::RenderBuildReport report;
    renderer = ave::RenderProfile::from_window(context, *window)
        .with(with)
        .with_result(report)
        .build();

    {
        const auto [fb_w, fb_h] = window->get_framebuffer_size();
        lg << "Framebuffer: " << fb_w << "x" << fb_h << std::endl;
    }
    lg << "Render backend: " << (renderer->dynamic_render ? "dynamic rendering" : "legacy render pass")
       << ", present mode: " << static_cast<int>(renderer->swapchain->get_present_mode())
       << " (1=MAILBOX, 2=FIFO, 0=IMMEDIATE)." << std::endl;
}

auto App::setup_geometry() -> void {
    allocator = ave::VMAAllocator::create_shared({ .device = renderer->device });

    ave::Shape cube_shape = ave::Prefab::cube(1.0f);
    ave::Shape loaded_shape;
    if(!cfg.shape.path.empty()){
        try{
            if(auto loaded = ave::ShapeLoader::from_entry(alib5::io::load_entry(cfg.shape.path));
               loaded && !loaded->empty()) {
                loaded_shape = std::move(*loaded);
                shape_index_count = static_cast<std::uint32_t>(loaded_shape.index_count());
                lg << "Loaded Shape: " << cfg.shape.path << ", " << loaded_shape.vertex_count()
                   << " vertices, " << shape_index_count / 3 << " triangles." << std::endl;
            } else {
                lg(alib5::Severity::Warn) << "Shape has no usable geometry: " << cfg.shape.path
                         << "; keeping prefab scene." << std::endl;
            }
        }catch(const std::exception& e){
            lg(alib5::Severity::Warn) << "Shape load failed: " << e.what() << "; keeping prefab scene." << std::endl;
        }
    }
    // 设备端几何缓冲 dragon 约 17.5 MiB，staging 8 MiB
    // 超出 staging 容量的切片会由 AVE 自动分多轮上传
    constexpr std::size_t geometry_buffer_size = 64u * 1024u * 1024u;
    constexpr std::size_t staging_buffer_size = 8u * 1024u * 1024u;

    // 方块与加载的 Shape 共用 DeviceOnly 缓冲区，各自通过 slice 访问。
    geometry_buffer.emplace(ave::CreateVMABufferInfo{
        .allocator = allocator,
        .size = geometry_buffer_size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
               | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
               | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .host_access = ave::BufferHostAccess::DeviceOnly,
    });

    // 创建临时的 CPU 可见中介缓冲 (Staging Buffer)
    staging_buffer = std::make_unique<ave::VMABuffer>(ave::CreateVMABufferInfo{
        .allocator = allocator,
        .size = staging_buffer_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .host_access = ave::BufferHostAccess::SequentialWrite,
    });

    // 通过 staging 上传到 GPU 上
    const ave::AllocateBufferInfo slice_info {
        .map_info = {
            .staging = staging_buffer.get(),
            .upload_context = renderer->get_upload_context()
        }
    };
    cube_vertices = geometry_buffer->alloc(cube_shape.vertices, slice_info);
    cube_indices = geometry_buffer->alloc(cube_shape.indices, slice_info);
    if(!loaded_shape.empty()) {
        shape_vertices = geometry_buffer->alloc(loaded_shape.vertices, slice_info);
        shape_indices = geometry_buffer->alloc(loaded_shape.indices, slice_info);
    }

    lg << "Uploaded " << cube_shape.vertex_count() << " cube vertices + "
       << cube_shape.index_count() << " indices + loaded Shape." << std::endl;
}

auto App::setup_pipeline() -> void {
    // Dispatch uses the frame's graphics command pool, which must support compute too.
    const auto family = renderer->swapchain->get_graphics_queue_family();
    if(!renderer->device->queue_family_supports(family, VK_QUEUE_COMPUTE_BIT)) {
        panicf("Froxel fog requires a graphics queue with compute support.");
    }
    auto vertex_input = ave::vertex_layout<ave::Shape::Vertex>().build();
    auto push_layout = ave::constant_layout<PushConstant>().build();

    lg << "Vertex Input Layout Table:\n" << vertex_input.to_table() << std::endl;
    lg << "Push Constant Layout Table:\n" << push_layout.to_table() << std::endl;

    control_fog_pipeline = renderer->create_compute_pipeline("fog/shaders/control-density.spv",
        [](ave::CreateComputePipelineInfo& ci) {
            ci.descriptor_bindings = ave::parse_descriptor_bindings(
                R"([["ssbo",11],["ssbo",12]])", ave::ShaderStage::compute);
            ci.push_constant_ranges = {{ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(glm::uvec4) }};
        });
    if(!control_fog_pipeline) panicf("Cannot create control fog density pipeline.");
    froxel_pipeline = renderer->create_compute_pipeline("fog/shaders/froxel.spv",
        [](ave::CreateComputePipelineInfo& ci) {
            ci.descriptor_bindings = ave::parse_descriptor_bindings(
                R"([["ubo",0],["sampler",4],["ssbo",6],["ssbo",9],["sampler",10],["ssbo",11],["ssbo",12],["sampler",14]])",
                ave::ShaderStage::compute);
        });
    if(!froxel_pipeline) panicf("Cannot create froxel compute pipeline.");
    temporal_pipeline = renderer->create_compute_pipeline("fog/shaders/froxel-temporal.spv",
        [](ave::CreateComputePipelineInfo& ci) {
            ci.descriptor_bindings = ave::parse_descriptor_bindings(
                R"([["ubo",0],["ssbo",6],["ssbo",7],["ssbo",8]])", ave::ShaderStage::compute);
        });
    integrate_pipeline = renderer->create_compute_pipeline("fog/shaders/froxel-integrate.spv",
        [](ave::CreateComputePipelineInfo& ci) {
            ci.descriptor_bindings = ave::parse_descriptor_bindings(
                R"([["ubo",0],["ssbo",3],["ssbo",8],["sampler",4],["ssbo",9],["sampler",10],["ssbo",11],["sampler",14]])",
                ave::ShaderStage::compute);
        });
    if(!temporal_pipeline || !integrate_pipeline) panicf("Cannot create volume filtering pipelines.");
    light_cull_pipeline = renderer->create_compute_pipeline("fog/shaders/light-cull.spv",
        [](ave::CreateComputePipelineInfo& ci) {
            ci.descriptor_bindings = ave::parse_descriptor_bindings(
                R"([["ubo",0],["ssbo",12]])", ave::ShaderStage::compute);
        });
    if(!light_cull_pipeline) panicf("Cannot create light culling pipeline.");

    // 不透明场景：采样 compute 积分后的体积光，关闭颜色混合。
    ave::GraphicsPipelineConfig scene_config {
        .vert = "fog/shaders/fog-vert.spv",
        .frag = "fog/shaders/fog-frag.spv",
        .bindings = vertex_input.bindings,
        .attributes = vertex_input.attributes,
        .cull_mode = VK_CULL_MODE_BACK_BIT,
        .front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depth_test = true,
        .depth_write = true,
        .depth_compare_op = VK_COMPARE_OP_LESS,
        .blend_enable = false,
        .descriptor_bindings = ave::parse_descriptor_bindings(R"(
            {"stage":"fragment","bindings":[
                ["ubo",0,"vertex|fragment"],["sampler",1],["ssbo",2,"vertex"],["ssbo",3],
                ["sampler",4],["ssbo",9],["sampler",10],["ssbo",11],["sampler",14],["sampler",15],
                ["sampler",16],["sampler",17],["sampler",18],["sampler",19],["sampler",20],["sampler",21],["sampler",22]
            ]}
        )"),
        .constant_attributes = push_layout.attributes
    };
    pipeline = renderer->create_graphics_pipeline(scene_config);
    scene_config.vert = "fog/shaders/fog-instanced-vert.spv";
    scene_config.constant_attributes.clear();
    instanced_pipeline = renderer->create_graphics_pipeline(scene_config);

    // 水面：同一套 shading，但不写深度并做简单 alpha 混合；在不透明几何和天空之后绘制。
    auto water_config = scene_config;
    water_config.depth_write = false;
    water_config.blend_enable = true;
    water_pipeline = renderer->create_graphics_pipeline(water_config);

    auto sky_attributes = vertex_input.attributes;
    // 天空只需要 position，避免未消费 normal/UV 的校验警告。
    sky_attributes.resize(1); 
    sky_pipeline = renderer->create_graphics_pipeline({
        .vert = "fog/shaders/sky-vert.spv",
        .frag = "fog/shaders/sky-frag.spv",
        .bindings = vertex_input.bindings,
        .attributes = sky_attributes,
        // 天空盒从内部观察 cube，背面剔除要反着来。
        .cull_mode = VK_CULL_MODE_FRONT_BIT,
        .front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depth_test = true,
        .depth_write = false,
        .depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL,
        .blend_enable = false,
        .descriptor_bindings = ave::parse_descriptor_bindings(
            R"({"stage":"fragment","bindings":[["ubo",0,"vertex|fragment"],["sampler",1],["ssbo",3]]})"),
        .constant_attributes = ave::constant_layout<SkyPushConstant>().build().attributes
    });
}

auto App::setup_ubo() -> void {
    if(renderer->wait_idle() != VK_SUCCESS) panicf("Cannot rebuild scene descriptors: device wait failed.");
    descriptor_cubes.clear();
    descriptor_water.clear();
    descriptor_sky.clear();
    descriptor_froxel.clear();
    descriptor_temporal.clear();
    descriptor_integrate.clear();
    descriptor_shadow.clear();
    descriptor_control_fog.clear();
    descriptor_light_cull.clear();
    light_mask_buffers.clear();
    scene_descriptor_pool.reset();
    ubo_buffers.clear();
    local_fog_buffers.clear();
    control_fog_buffers.clear();
    control_density_buffers.clear();
    uploaded_ubos.clear();
    uploaded_local_fog.clear();
    froxel_buffers.clear();
    injection_buffers.clear();
    history_buffers.clear();
    volume_history.clear();
    const auto [width, height] = window->get_framebuffer_size();
    volume.pixel_size = std::clamp(volume.pixel_size, 4, 32);
    volume.depth_slices = std::clamp(volume.depth_slices, 16, 96);
    const auto tile = volume.pixel_size;
    const auto grid_x = std::clamp((std::max(width, 1) + tile - 1) / tile, 1, 192);
    const auto grid_y = std::clamp((std::max(height, 1) + tile - 1) / tile, 1, 128);
    froxel_extent = glm::uvec2(grid_x, grid_y);
    froxel_depth = static_cast<unsigned>(volume.depth_slices);
    const auto count = renderer->swapchain->get_image_count();
    uploaded_local_fog.assign(count, 0);
    uploaded_control_fog.assign(count, {});
    generated_control_fog.assign(count, {});
    if(fog_control_grid_bytes > renderer->device->get_physical_device_limits().maxStorageBufferRange)
        panicf("Device storage buffer range cannot host control fog density fields.");
    // Quality-only grid changes must not recreate/recompile shadow passes and pipelines.
    if(!shadow_renders[0] || shadow_renders[0]->get_swapchain() != renderer->swapchain) setup_shadows();
    else gpu_queries->submitted.assign(count, false);
    scene_descriptor_pool = ave::DescriptorPool::create({
        .device = renderer->device,
        .pool_sizes = {
            ave::DescriptorPoolSize::uniform_buffer(count * 12),
            ave::DescriptorPoolSize::combined_image_sampler(count * 40),
            ave::DescriptorPoolSize::storage_buffer(count * 32)
        },
        .max_sets = count * 12
    });
    for(ave::u32 i = 0; i < count; ++i) {
        uploaded_ubos.emplace_back();
        ubo_buffers.push_back(std::make_unique<ave::VMABuffer>(ave::CreateVMABufferInfo{
            .allocator = allocator,
            .size = sizeof(CameraFogUbo),
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .host_access = ave::BufferHostAccess::RandomAccess,
            .persistent_mapping = true,
        }));

        local_fog_buffers.push_back(std::make_unique<ave::VMABuffer>(ave::CreateVMABufferInfo{
            .allocator = allocator, .size = sizeof(LocalFogUpload),
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .host_access = ave::BufferHostAccess::RandomAccess, .persistent_mapping = true,
        }));
        control_fog_buffers.push_back(std::make_unique<ave::VMABuffer>(ave::CreateVMABufferInfo{
            .allocator = allocator, .size = sizeof(FogControlUpload),
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .host_access = ave::BufferHostAccess::RandomAccess, .persistent_mapping = true,
        }));
        control_density_buffers.push_back(std::make_unique<ave::VMABuffer>(ave::CreateVMABufferInfo{
            .allocator = allocator, .size = fog_control_grid_bytes,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .host_access = ave::BufferHostAccess::DeviceOnly,
        }));
        light_mask_buffers.push_back(std::make_unique<ave::VMABuffer>(ave::CreateVMABufferInfo{
            .allocator = allocator,
            .size = VkDeviceSize(grid_x) * grid_y * froxel_depth * sizeof(std::uint32_t),
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .host_access = ave::BufferHostAccess::DeviceOnly,
        }));
        descriptor_light_cull.push_back(scene_descriptor_pool->allocate_and_write(
            light_cull_pipeline->get_descriptor_set_layout(), {
                ave::DescriptorWrite::ubo(0, *ubo_buffers.back()),
                ave::DescriptorWrite::ssbo(12, *light_mask_buffers.back()),
            }));
        descriptor_control_fog.push_back(scene_descriptor_pool->allocate_and_write(
            control_fog_pipeline->get_descriptor_set_layout(), {
                ave::DescriptorWrite::ssbo(11, *control_density_buffers.back()),
                ave::DescriptorWrite::ssbo(12, *control_fog_buffers.back()),
            }));
        const auto make_volume = [&](unsigned slices) {
            return std::make_unique<ave::VMABuffer>(ave::CreateVMABufferInfo{
                .allocator = allocator,
                .size = VkDeviceSize(grid_x) * grid_y * slices * sizeof(glm::vec4),
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .host_access = ave::BufferHostAccess::DeviceOnly,
            });
        };
        froxel_buffers.push_back(make_volume(froxel_depth + 2));
        injection_buffers.push_back(make_volume(froxel_depth));
        history_buffers.push_back({ make_volume(froxel_depth), make_volume(froxel_depth) });
        volume_history.emplace_back();
        const auto shadow_write = [&](unsigned binding, unsigned light) {
            return ave::DescriptorWrite::image(binding, shadow_sampler->get_system_handle(),
                shadow_images[light].at(i)->get_image_view(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        };
        descriptor_froxel.push_back(scene_descriptor_pool->allocate_and_write(
            froxel_pipeline->get_descriptor_set_layout(), {
                ave::DescriptorWrite::ubo(0, *ubo_buffers.back()),
                shadow_write(4, 0),
                ave::DescriptorWrite::ssbo(6, *injection_buffers.back()),
                ave::DescriptorWrite::ssbo(9, *local_fog_buffers.back()),
                ave::DescriptorWrite::image(10, *local_fog_sampler, *local_fog_atlas),
                ave::DescriptorWrite::ssbo(11, *control_density_buffers.back()),
                ave::DescriptorWrite::ssbo(12, *light_mask_buffers.back()),
                ave::DescriptorWrite::image(14, *point_shadow_sampler, *point_shadow_array_views.at(i),
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL),
            }));
        std::array<VkDescriptorSet, 2> temporal_sets {};
        std::array<VkDescriptorSet, 2> integrate_sets {};
        for(unsigned output = 0; output < 2; ++output) {
            temporal_sets[output] = scene_descriptor_pool->allocate_and_write(
                temporal_pipeline->get_descriptor_set_layout(), {
                    ave::DescriptorWrite::ubo(0, *ubo_buffers.back()),
                    ave::DescriptorWrite::ssbo(6, *injection_buffers.back()),
                    ave::DescriptorWrite::ssbo(7, *history_buffers.back()[1 - output]),
                    ave::DescriptorWrite::ssbo(8, *history_buffers.back()[output]),
                });
            integrate_sets[output] = scene_descriptor_pool->allocate_and_write(
                integrate_pipeline->get_descriptor_set_layout(), {
                    ave::DescriptorWrite::ubo(0, *ubo_buffers.back()),
                    ave::DescriptorWrite::ssbo(3, *froxel_buffers.back()),
                    ave::DescriptorWrite::ssbo(8, *history_buffers.back()[output]),
                    shadow_write(4, 0),
                    ave::DescriptorWrite::ssbo(9, *local_fog_buffers.back()),
                    ave::DescriptorWrite::image(10, *local_fog_sampler, *local_fog_atlas),
                    ave::DescriptorWrite::ssbo(11, *control_density_buffers.back()),
                    ave::DescriptorWrite::image(14, *point_shadow_sampler, *point_shadow_array_views.at(i),
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL),
                });
        }
        descriptor_temporal.push_back(temporal_sets);
        descriptor_integrate.push_back(integrate_sets);
        descriptor_shadow.push_back(scene_descriptor_pool->allocate_and_write(
            shadow_pipeline->get_descriptor_set_layout(), {
                ave::DescriptorWrite::ubo(0, *ubo_buffers.back()),
                ave::DescriptorWrite::ssbo(2, *instance_buffer),
            }));

        // 场景池单独持有，窗口重建时整体替换。
        descriptor_cubes.push_back(scene_descriptor_pool->allocate_and_write(
            pipeline->get_descriptor_set_layout(0),
            {
                ave::DescriptorWrite::ubo(0, *ubo_buffers.back()),
                ave::DescriptorWrite::image(1, *sampler, *cube_texture),
                ave::DescriptorWrite::ssbo(2, *instance_buffer),
                ave::DescriptorWrite::ssbo(3, *froxel_buffers.back()),
                shadow_write(4, 0),
                ave::DescriptorWrite::ssbo(9, *local_fog_buffers.back()),
                ave::DescriptorWrite::image(10, *local_fog_sampler, *local_fog_atlas),
                ave::DescriptorWrite::ssbo(11, *control_density_buffers.back()),
                ave::DescriptorWrite::image(14, *point_shadow_sampler, *point_shadow_array_views.at(i),
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL),
                ave::DescriptorWrite::image(15, *water_normal_sampler, *water_normal_texture),
                ave::DescriptorWrite::image(16, *water_normal_sampler, *block_normal_textures[0]),
                ave::DescriptorWrite::image(17, *water_normal_sampler, *block_normal_textures[1]),
                ave::DescriptorWrite::image(18, *water_normal_sampler, *block_normal_textures[2]),
                ave::DescriptorWrite::image(19, *water_normal_sampler, *block_normal_textures[3]),
                ave::DescriptorWrite::image(20, *water_normal_sampler, *block_normal_textures[4]),
                ave::DescriptorWrite::image(21, *water_normal_sampler, *block_normal_textures[5]),
                ave::DescriptorWrite::image(22, *water_normal_sampler, *block_normal_textures[6]),
            }
        ));
        descriptor_water.push_back(scene_descriptor_pool->allocate_and_write(
            water_pipeline->get_descriptor_set_layout(0),
            {
                ave::DescriptorWrite::ubo(0, *ubo_buffers.back()),
                ave::DescriptorWrite::image(1, *sampler, *cube_texture),
                ave::DescriptorWrite::ssbo(2, *water_instance_buffer),
                ave::DescriptorWrite::ssbo(3, *froxel_buffers.back()),
                shadow_write(4, 0),
                ave::DescriptorWrite::ssbo(9, *local_fog_buffers.back()),
                ave::DescriptorWrite::image(10, *local_fog_sampler, *local_fog_atlas),
                ave::DescriptorWrite::ssbo(11, *control_density_buffers.back()),
                ave::DescriptorWrite::image(14, *point_shadow_sampler, *point_shadow_array_views.at(i),
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL),
                ave::DescriptorWrite::image(15, *water_normal_sampler, *water_normal_texture),
                ave::DescriptorWrite::image(16, *water_normal_sampler, *block_normal_textures[0]),
                ave::DescriptorWrite::image(17, *water_normal_sampler, *block_normal_textures[1]),
                ave::DescriptorWrite::image(18, *water_normal_sampler, *block_normal_textures[2]),
                ave::DescriptorWrite::image(19, *water_normal_sampler, *block_normal_textures[3]),
                ave::DescriptorWrite::image(20, *water_normal_sampler, *block_normal_textures[4]),
                ave::DescriptorWrite::image(21, *water_normal_sampler, *block_normal_textures[5]),
                ave::DescriptorWrite::image(22, *water_normal_sampler, *block_normal_textures[6]),
            }
        ));
        descriptor_sky.push_back(scene_descriptor_pool->allocate_and_write(
            sky_pipeline->get_descriptor_set_layout(0),
            {
                ave::DescriptorWrite::ubo(0, *ubo_buffers.back()),
                ave::DescriptorWrite::image(1, *sky_sampler, *sky_texture),
                ave::DescriptorWrite::ssbo(3, *froxel_buffers.back()),
            }
        ));
    }
    const auto bytes = VkDeviceSize(grid_x) * grid_y * (froxel_depth * 4 + 2) * sizeof(glm::vec4)
        + fog_control_grid_bytes + sizeof(FogControlUpload);
    volume_status = std::format("Froxel {}x{}x{} + control 64^3x16 | {:.1f} MiB/image, {:.1f} MiB total (SSBO only)",
        grid_x, grid_y, froxel_depth, double(bytes) / 1048576.0, double(bytes * count) / 1048576.0);
    lg << volume_status << std::endl;
}

auto App::setup_textures() -> void {
    sampler = ave::Sampler::create({ .device = renderer->device });
    cube_texture = ave::Image::create_from_file(
        { .allocator = allocator },
        alib5::io::load_entry("test_data/imgs/ice.png"),
        { .map_info = { .upload_context = renderer->get_upload_context() } }
    );

    sky_sampler = ave::Sampler::create({
        .device = renderer->device,
        .address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
    });
    // 无贴图时仍绑定有效图像，程序天空分支不会采样它。
    sky_texture = cube_texture;
    water_normal_sampler = ave::Sampler::create({
        .device = renderer->device,
        .address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT
    });
    // build_shaders.sh 会把源码 textures/ 同步到 assets/fog/textures；同时兼容从仓库根启动。
    alib5::io::FileEntry normal_entry;
    for(const auto* path : { "fog/textures/water_normal.png", "../examples/fog/textures/water_normal.png",
                             "examples/fog/textures/water_normal.png", "textures/water_normal.png" }) {
        normal_entry = alib5::io::load_entry(path, false);
        if(!normal_entry.invalid()) break;
    }
    if(normal_entry.invalid()) panicf("Cannot find textures/water_normal.png; run examples/fog/build_shaders.sh.");
    water_normal_texture = ave::Image::create_from_file(
        { .allocator = allocator },
        normal_entry,
        { .map_info = { .upload_context = renderer->get_upload_context() } }
    );
    if(!water_normal_texture) panicf("Cannot load water normal texture.");

    const auto load_normal = [&](const std::string& file) -> std::shared_ptr<ave::Image> {
        alib5::io::FileEntry entry;
        for(const auto* prefix : { "fog/textures/", "../examples/fog/textures/",
                                   "examples/fog/textures/", "textures/" }) {
            entry = alib5::io::load_entry(std::string(prefix) + file, false);
            if(!entry.invalid()) break;
        }
        if(entry.invalid()) panicf("Cannot find texture '{}'; run examples/fog/build_shaders.sh.", file);
        auto image = ave::Image::create_from_file(
            { .allocator = allocator },
            entry,
            { .map_info = { .upload_context = renderer->get_upload_context() } }
        );
        if(!image) panicf("Cannot load texture '{}'.", file);
        return image;
    };
    const std::array<const char*, 7> block_normal_files {
        "grass_top_normal.png", "grass_side_normal.png", "stone_normal.png",
        "wood_normal.png", "leaves_normal.png", "sand_normal.png", "roof_normal.png"
    };
    for(std::size_t i = 0; i < block_normal_textures.size(); ++i)
        block_normal_textures[i] = load_normal(block_normal_files[i]);
    lg << "Loaded " << block_normal_textures.size() << " block normal maps (256x256)." << std::endl;

    if(!cfg.sky.texture.empty()) {
        sky_texture = ave::Image::create_from_file(
            { .allocator = allocator },
            alib5::io::load_entry(cfg.sky.texture),
            { .map_info = { .upload_context = renderer->get_upload_context() } }
        );
    }

    lg << "Loaded ice.png (cubes) " << cube_texture->get_extent().width << "x"
       << cube_texture->get_extent().height << "; water normal "
       << water_normal_texture->get_extent().width << "x"
       << water_normal_texture->get_extent().height << std::endl;
}
