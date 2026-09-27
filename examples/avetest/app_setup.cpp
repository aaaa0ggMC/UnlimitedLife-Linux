#include "app.h"

namespace avetest {

App::App()
    : logger(),
      lg(logger, "avetest"),
      vklg(logger, "Vulkan") {
    logger.append_mod<alib6::lot::Console>("console");
}

void App::setup() {
    window = std::make_unique<ave::Window>(ave::CreateWindowInfo{
        .ctx = context,
        .title = "Hello from AVE! (Vertex Buffer Triangle)",
        .width = 800,
        .height = 600,
    });

    window->on<ave::AfterWindowFramebufferResizeEvent>(
    [this](ave::AfterWindowFramebufferResizeEvent& ev) {
        if(ev.width > 0 && ev.height > 0 && renderer.has_value()) {
            if(ave::recreate_swapchain_from_window(*renderer, *window)) {
                lg << "Recreated swapchain." << std::endl;
            }
        }
    });

    ave::ProfileWith with;
    with.configure_instance = [](ave::WithGlobalInput&, ave::CreateInstanceInfo& ci) {
        ci.application_name = "avetest";
        ci.api_version = ave::ave_vk_1_4;
    };
    with.configure_debug_messenger.emplace();
    with.try_dynamic_rendering = true;
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

    with.configure_descriptor_pool = [](
        ave::WithDescriptorPoolInput& input,
        ave::CreateDescriptorPoolInfo& ci
    ) {
        // 先沿用默认配置（每帧一个 uniform buffer），再追加纹理用的 sampler 描述符
        ave::default_configure_descriptor_pool(input, ci);
        ci.pool_sizes.push_back(
            ave::DescriptorPoolSize::combined_image_sampler(1)
        );
    };

    ave::RenderBuildReport report;
    renderer = ave::RenderProfile::from_window(context, *window)
        .with(with)
        .with_result(report)
        .build();

    // lg << alib6::to_adata(report) << std::endl;
    setup_vertex_data();

    auto vertex_input = ave::vertex_layout<Vertex>().build();
    auto push_layout = ave::constant_layout<PushConstant>().build();

    lg << "Vertex Input Layout Table:\n" << vertex_input.to_table() << std::endl;
    lg << "Push Constant Layout Table:\n" << push_layout.to_table() << std::endl;

    // 使用聚合 GraphicsPipelineConfig 配置管线（错误由 ErrorWrapper 自动处理）
    pipeline = renderer->create_graphics_pipeline({
        .vert = "avetest/shaders/vertex-vert.spv",
        .frag = "avetest/shaders/simple-frag.spv",
        .bindings = vertex_input.bindings,
        .attributes = vertex_input.attributes,
        .cull_mode = VK_CULL_MODE_NONE,
        .depth_test = true,
        .depth_write = true,
        .depth_compare_op = VK_COMPARE_OP_LESS,
        .descriptor_bindings = {
            ave::DescriptorBinding::ubo(0, VK_SHADER_STAGE_VERTEX_BIT),
            ave::DescriptorBinding::combined_image_sampler(
                1, VK_SHADER_STAGE_FRAGMENT_BIT
            )
        },
        .constant_attributes = push_layout.attributes
    });

    setup_ubo();
    setup_texture();
}

void App::setup_vertex_data() {
    allocator = ave::VMAAllocator::create_shared({ .device = renderer->device });
    // 顶点与索引共用同一个 DeviceOnly 缓冲区（各自通过 slice 访问）
    buffer.emplace(ave::CreateVMABufferInfo{
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
        .size = sizeof(Vertex) * cube_vertices.size()
              + sizeof(std::uint16_t) * cube_indices.size(),
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .host_access = ave::BufferHostAccess::SequentialWrite,
    });

    // 一行代码无感通过 staging 上传到 DeviceOnly 缓冲区切片
    const ave::AllocateBufferInfo slice_info {
        .map_info = {
            .staging = staging_buffer.get(),
            .upload_context = renderer->get_upload_context()
        }
    };
    vertices_data = buffer->alloc(cube_vertices, slice_info);
    indices_data = buffer->alloc(cube_indices, slice_info);
    index_count = static_cast<uint32_t>(cube_indices.size());

    lg << "Uploaded " << cube_vertices.size() << " vertices and "
       << cube_indices.size() << " indices to DeviceOnly GPU buffer via staging buffer." << std::endl;
}

void App::setup_texture() {
    // 一条调用完成：stb 解码 → VMA 建图 → 内部临时 staging 上传（含 layout 转换）。
    // - format/extent/usage 零值 → 按文件自动推断；
    // - .map_info.staging 留空 → 使用 ci.allocator 创建内部 staging；
    // - 未来有 Sampler 后即可直接通过 descriptor 采样本纹理。
    texture = ave::Image::create_from_file(
        { .allocator = allocator },
        alib6::io::load_entry("avetest/textures/template.png"),
        { .map_info = { .upload_context = renderer->get_upload_context() } }
    );

    if(!texture) return;
    // 采样器：线性过滤 + Repeat 寻址；非 mipmap 纹理 max_lod = 0
    sampler = ave::Sampler::create({ .device = renderer->device });
    if(!sampler) return;

    // 纹理（sampler + view）写入 descriptor set 的 binding 1
    renderer->descriptor_pool->write_image(
        descriptor_set,
        1,
        sampler->get_system_handle(),
        texture->get_image_view()
    );

    lg << "Loaded texture " << texture->get_extent().width << "x"
       << texture->get_extent().height << " (format "
       << static_cast<int>(texture->get_format()) << ", auto default view)." << std::endl;
}

void App::setup_ubo() {
    ubo_buffer.emplace(ave::CreateVMABufferInfo{
        .allocator = allocator,
        .size = sizeof(CameraUbo),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .host_access = ave::BufferHostAccess::RandomAccess,
        .persistent_mapping = true,
    });

    // DescriptorPool 由 Profile 在构建 Renderer 时创建并持有（RAII，销毁时自行等待设备空闲）。
    descriptor_set = renderer->descriptor_pool->allocate_descriptor_set(
        pipeline->get_descriptor_set_layout(0)
    );
    renderer->descriptor_pool->write_buffer(
        descriptor_set,
        0,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        ubo_buffer->get_system_handle(),
        0,
        sizeof(CameraUbo)
    );

    lg << "Initialized Camera UBO buffer and allocated DescriptorSet from Pipeline layout." << std::endl;
}

} // namespace avetest
