#include "app.h"

auto App::setup() -> void {
    setup_create_window();
    setup_renderer();
    setup_geometry();
    setup_pipeline();
    setup_ubo();
    setup_textures();
    setup_scene();
}

auto App::setup_create_window() -> void {
    // 初始化窗口
    window.emplace(ave::CreateWindowInfo {
        .ctx = context,
        .title = "Global Linear Fog by aaaa0ggmc",
        .width = cfg.window.width,
        .height = cfg.window.height
    });
    // 输入状态机挂到窗口：poll_events 时 GLFW 回调自动驱动
    window->bind_input(input);
    // 处理Swapchain重建
    window->on<ave::AfterWindowFramebufferResizeEvent>(
        [this](const auto &){
            if(ave::recreate_swapchain_from_window(context, *renderer, *window)) {
                lg << "Recreated swapchain." << std::endl;
            }
        }
    );
}

auto App::setup_renderer() -> void {
    ave::ProfileWith with;
    with.configure_instance = [](ave::WithGlobalInput&, ave::CreateInstanceInfo& ci) {
        ci.application_name = "fog";
        ci.api_version = ave::ave_vk_1_4;
    };
    with.try_dynamic_rendering = true;
    with.configure_debug_messenger.emplace();
    with.configure_debug_messenger->on_message = [this](
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT,
        const VkDebugUtilsMessengerCallbackDataEXT& data
    ) {
        vklg(ave::to_log_level(severity))
            << (data.pMessage ? data.pMessage : "<no message>")
            << std::endl;
        return false;
    };

    // 两个完整描述符集：set0 = 相机/雾 UBO + ice.png（立方体），
    // set1 = 同一 UBO + wall.jpg 砖墙（地面）；每 draw 前按需切换。
    with.configure_descriptor_pool = [](
        ave::WithDescriptorPoolInput& input,
        ave::CreateDescriptorPoolInfo& ci
    ) {
        ci.device = input.get_device();
        ci.max_sets = std::max<alib6::u32>(input.get_frame_count(), 2);
        ci.pool_sizes = {
            ave::DescriptorPoolSize::uniform_buffer(ci.max_sets),
            ave::DescriptorPoolSize::combined_image_sampler(2),
        };
    };

    ave::RenderBuildReport report;
    renderer = ave::RenderProfile::from_window(context, *window)
        .with(with)
        .with_result(report)
        .build();
}

auto App::setup_geometry() -> void {
    allocator = ave::VMAAllocator::create_shared({ .device = renderer->device });

    ave::Shape cube_shape = ave::Prefab::cube(1.0f);
    ave::Shape ground_shape = ave::Prefab::plane(60.0f, 60.0f, 15.0f);

    // 立方体与地面几何共用同一个 DeviceOnly 缓冲区（各自通过 slice 访问）
    geometry_buffer.emplace(ave::CreateVMABufferInfo{
        .allocator = allocator,
        .size = 1024 * 1024,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
               | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
               | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .host_access = ave::BufferHostAccess::DeviceOnly,
    });

    // 创建临时的 CPU 可见中介缓冲 (Staging Buffer)
    staging_buffer = std::make_unique<ave::VMABuffer>(ave::CreateVMABufferInfo{
        .allocator = allocator,
        .size = sizeof(ave::Shape::Vertex) * (cube_shape.vertex_count() + ground_shape.vertex_count())
              + sizeof(std::uint32_t) * (cube_shape.index_count() + ground_shape.index_count()),
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
    ground_vertices = geometry_buffer->alloc(ground_shape.vertices, slice_info);
    ground_indices = geometry_buffer->alloc(ground_shape.indices, slice_info);

    lg << "Uploaded " << cube_shape.vertex_count() << " cube vertices + "
       << cube_shape.index_count() << " indices, "
       << ground_shape.vertex_count() << " ground vertices + "
       << ground_shape.index_count() << " indices." << std::endl;
}

auto App::setup_pipeline() -> void {
    auto vertex_input = ave::vertex_layout<ave::Shape::Vertex>().build();
    auto push_layout = ave::constant_layout<PushConstant>().build();

    lg << "Vertex Input Layout Table:\n" << vertex_input.to_table() << std::endl;
    lg << "Push Constant Layout Table:\n" << push_layout.to_table() << std::endl;

    // 不透明场景：雾在 fragment shader 内 mix，关闭颜色混合。
    pipeline = renderer->create_graphics_pipeline({
        .vert = "fog/shaders/fog-vert.spv",
        .frag = "fog/shaders/fog-frag.spv",
        .bindings = vertex_input.bindings,
        .attributes = vertex_input.attributes,
        .cull_mode = VK_CULL_MODE_NONE,
        .depth_test = true,
        .depth_write = true,
        .depth_compare_op = VK_COMPARE_OP_LESS,
        .blend_enable = false,
        .descriptor_bindings = {
            ave::DescriptorBinding::ubo(
                0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            ),
            ave::DescriptorBinding::combined_image_sampler(
                1, VK_SHADER_STAGE_FRAGMENT_BIT
            )
        },
        .constant_attributes = push_layout.attributes
    });
}

auto App::setup_ubo() -> void {
    ubo_buffer.emplace(ave::CreateVMABufferInfo{
        .allocator = allocator,
        .size = sizeof(CameraFogUbo),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .host_access = ave::BufferHostAccess::RandomAccess,
        .persistent_mapping = true,
    });

    // DescriptorPool 由 Profile 在构建 Renderer 时创建并持有
    descriptor_cubes = renderer->descriptor_pool->allocate_descriptor_set(
        pipeline->get_descriptor_set_layout(0)
    );
    descriptor_ground = renderer->descriptor_pool->allocate_descriptor_set(
        pipeline->get_descriptor_set_layout(0)
    );

    for (VkDescriptorSet set : { descriptor_cubes, descriptor_ground }) {
        renderer->descriptor_pool->write_buffer(
            set,
            0,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            ubo_buffer->get_system_handle(),
            0,
            sizeof(CameraFogUbo)
        );
    }

    lg << "Initialized Camera+Fog UBO buffer and 2 DescriptorSets from Pipeline layout." << std::endl;
}

auto App::setup_textures() -> void {
    sampler = ave::Sampler::create({ .device = renderer->device });

    // 注意：load_entry(path) 默认 force_existence=true，文件不存在时会经默认的
    // 空 ErrorWrapper 上报——AVE 开启了 ALIB6_ERROR_USE_PANIC，即直接 panic。
    // 纹理是必需资源：缺文件就该响亮失败；若真的想软检查（如可选配置），
    // 必须显式 load_entry(path, false) 后判 entry.invalid()（见 main.cpp）。
    // 半路的降级分支没有意义：缺纹理继续渲染只会把未绑定描述符喂给管线。
    cube_texture = ave::Image::create_from_file(
        { .allocator = allocator },
        alib6::io::load_entry("test_data/imgs/ice.png"),
        { .map_info = { .upload_context = renderer->get_upload_context() } }
    );
    ground_texture = ave::Image::create_from_file(
        { .allocator = allocator },
        alib6::io::load_entry("test_data/imgs/wall.jpg"),
        { .map_info = { .upload_context = renderer->get_upload_context() } }
    );

    renderer->descriptor_pool->write_image(
        descriptor_cubes, 1, sampler->get_system_handle(), cube_texture->get_image_view()
    );
    renderer->descriptor_pool->write_image(
        descriptor_ground, 1, sampler->get_system_handle(), ground_texture->get_image_view()
    );

    lg << "Loaded ice.png (cubes) " << cube_texture->get_extent().width << "x"
       << cube_texture->get_extent().height << std::endl;
    lg << "Loaded wall.jpg (ground) " << ground_texture->get_extent().width << "x"
       << ground_texture->get_extent().height << std::endl;
}

auto App::setup_scene() -> void {
    objects.clear();

    // 一排沿 -Z 渐远的立方体，直观展示线性雾的距离衰减
    for(int i = 0; i < 12; ++i) {
        Object obj;
        obj.model = glm::translate(
            glm::mat4(1.0f),
            glm::vec3(
                (i % 3 - 1) * 1.6f,
                0.1f + 0.35f * std::sin(static_cast<float>(i) * 0.9f),
                -2.6f * static_cast<float>(i)
            )
        );
        obj.model = glm::rotate(
            obj.model,
            glm::radians(12.0f * static_cast<float>(i)),
            glm::vec3(0.25f, 1.0f, 0.15f)
        );
        obj.ground = false;
        objects.push_back(obj);
    }

    // 地面（ice.png，置于 y = -1）
    Object ground;
    ground.model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
    ground.ground = true;
    objects.push_back(ground);

    lg << "Scene ready: " << objects.size() << " objects." << std::endl;
}
