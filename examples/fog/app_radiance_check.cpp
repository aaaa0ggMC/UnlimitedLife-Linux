#include "app.h"

namespace {
void require_gpu(bool condition, std::string_view message) {
    if(!condition) throw std::runtime_error(std::string(message));
}

std::size_t exercise_radiance(const std::shared_ptr<ave::Instance>& instance) {
    const auto devices = instance->enumerate_physical_devices();
    std::optional<ave::PhysicalDeviceInfo> chosen;
    for(auto&& info : devices) {
        if(info.as_gpu().compute_queue_family) { chosen = std::move(info); break; }
    }
    require_gpu(chosen.has_value(), "No Vulkan compute queue");
    const unsigned family = *chosen->as_gpu().compute_queue_family;
    auto device = ave::Device::create({ .instance = instance, .physical_device = chosen->device,
        .queues = {{ family, {1.0f} }} });
    require_gpu(bool(device), "Cannot create compute device");
    ave::Renderer renderer;
    renderer.device = device;
    auto pipeline = renderer.create_compute_pipeline("fog/shaders/radiance-check.spv", [](auto& ci) {
        ci.descriptor_bindings = ave::parse_descriptor_bindings(
            R"([["ubo",0],["ssbo",3],["sampler",4],["ssbo",9],["sampler",10],["ssbo",11],["ssbo",13],["sampler",14]])",
            ave::ShaderStage::compute);
    });
    require_gpu(bool(pipeline), "Cannot create radiance-check pipeline");
    auto allocator = ave::VMAAllocator::create_shared({ .device = device });
    constexpr unsigned control_size = fog_control_grid_size;
    constexpr std::size_t control_cells = std::size_t(control_size) * control_size * control_size;
    // Only slot zero is exercised; all existing radiance media remain procedural.
    ave::VMABuffer control_density(ave::CreateVMABufferInfo{
        .allocator = allocator, .size = control_cells * sizeof(float), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .host_access = ave::BufferHostAccess::RandomAccess,
    });
    ave::VMABuffer control_fields(ave::CreateVMABufferInfo{
        .allocator = allocator, .size = sizeof(GpuFogControlField), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .host_access = ave::BufferHostAccess::RandomAccess,
    });
    auto control_pipeline = renderer.create_compute_pipeline("fog/shaders/control-density.spv", [](auto& ci) {
        ci.descriptor_bindings = ave::parse_descriptor_bindings(
            R"([["ssbo",11],["ssbo",12]])", ave::ShaderStage::compute);
        ci.push_constant_ranges = {{ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(glm::uvec4) }};
    });
    require_gpu(bool(control_pipeline), "Cannot create production control-density pipeline");
    ave::VMABuffer camera(ave::CreateVMABufferInfo{
        .allocator = allocator, .size = sizeof(CameraFogUbo), .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .host_access = ave::BufferHostAccess::RandomAccess,
    });
    ave::VMABuffer output(ave::CreateVMABufferInfo{
        .allocator = allocator, .size = 1088 * sizeof(glm::vec4), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .host_access = ave::BufferHostAccess::RandomAccess,
    });
    ave::VMABuffer integrated(ave::CreateVMABufferInfo{
        .allocator = allocator, .size = 66 * sizeof(glm::vec4), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .host_access = ave::BufferHostAccess::RandomAccess,
    });
    ave::VMABuffer filtered(ave::CreateVMABufferInfo{
        .allocator = allocator, .size = 64 * sizeof(glm::vec4), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .host_access = ave::BufferHostAccess::RandomAccess,
    });
    auto integrate_pipeline = renderer.create_compute_pipeline("fog/shaders/froxel-integrate.spv", [](auto& ci) {
        ci.descriptor_bindings = ave::parse_descriptor_bindings(
            R"([["ubo",0],["ssbo",3],["ssbo",8],["sampler",4],["ssbo",9],["sampler",10],["ssbo",11],["sampler",14]])",
            ave::ShaderStage::compute);
    });
    require_gpu(bool(integrate_pipeline), "Cannot create production froxel integration pipeline");
    ave::VMABuffer media(ave::CreateVMABufferInfo{
        .allocator = allocator, .size = sizeof(LocalFogUpload), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .host_access = ave::BufferHostAccess::RandomAccess,
    });
    auto shadow = ave::Image::create({ .allocator = allocator, .format = VK_FORMAT_R8_UNORM,
        .extent = {1, 1, 1}, .usage = VK_IMAGE_USAGE_SAMPLED_BIT });
    auto point_shadow = ave::Image::create({ .allocator = allocator, .format = VK_FORMAT_R8_UNORM,
        .extent = {1, 1, 1}, .array_layers = 6, .usage = VK_IMAGE_USAGE_SAMPLED_BIT });
    auto point_shadow_view = point_shadow ? ave::ImageView::create({
        .image = point_shadow,
        .view = { .view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
                  .subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 } },
    }) : nullptr;
    auto atlas = ave::Image::create({ .allocator = allocator, .image_type = VK_IMAGE_TYPE_3D,
        .format = VK_FORMAT_R8_UNORM, .extent = {1, 1, 1}, .usage = VK_IMAGE_USAGE_SAMPLED_BIT });
    auto sampler = ave::Sampler::create({ .device = device });
    require_gpu(shadow && atlas && point_shadow_view && sampler, "Cannot create radiance-check dummy images");
    auto descriptors = ave::DescriptorPool::create({
        .device = device,
        .pool_sizes = { ave::DescriptorPoolSize::uniform_buffer(2), ave::DescriptorPoolSize::storage_buffer(10),
            ave::DescriptorPoolSize::combined_image_sampler(8) },
        .max_sets = 3
    });
    require_gpu(bool(descriptors), "Cannot create radiance descriptor pool");
    const auto set = descriptors->allocate_and_write(pipeline->get_descriptor_set_layout(), {
        ave::DescriptorWrite::ubo(0, camera), ave::DescriptorWrite::ssbo(13, output),
        ave::DescriptorWrite::ssbo(11, control_density),
        ave::DescriptorWrite::ssbo(3, integrated),
        ave::DescriptorWrite::ssbo(9, media), ave::DescriptorWrite::image(4, *sampler, *shadow),
        ave::DescriptorWrite::image(10, *sampler, *atlas),
        ave::DescriptorWrite::image(14, *sampler, *point_shadow_view)
    });
    require_gpu(set != VK_NULL_HANDLE, "Cannot allocate radiance descriptors");
    const auto integrate_set = descriptors->allocate_and_write(integrate_pipeline->get_descriptor_set_layout(), {
        ave::DescriptorWrite::ubo(0, camera), ave::DescriptorWrite::ssbo(3, integrated),
        ave::DescriptorWrite::ssbo(8, filtered), ave::DescriptorWrite::ssbo(9, media),
        ave::DescriptorWrite::ssbo(11, control_density),
        ave::DescriptorWrite::image(4, *sampler, *shadow),
        ave::DescriptorWrite::image(10, *sampler, *atlas),
        ave::DescriptorWrite::image(14, *sampler, *point_shadow_view)
    });
    require_gpu(integrate_set != VK_NULL_HANDLE, "Cannot allocate froxel integration descriptors");
    const auto control_set = descriptors->allocate_and_write(control_pipeline->get_descriptor_set_layout(), {
        ave::DescriptorWrite::ssbo(11, control_density), ave::DescriptorWrite::ssbo(12, control_fields)
    });
    require_gpu(control_set != VK_NULL_HANDLE, "Cannot allocate control-density descriptors");
    auto pool = ave::CommandPool::create({ .device = device, .queue_family = family });
    require_gpu(bool(pool), "Cannot create compute command pool");
    auto commands = ave::CommandBuffers::create({ .pool = pool, .count = 1 });
    require_gpu(bool(commands), "Cannot allocate compute commands");
    const auto command = commands->get_buffers().front();
    // 生产 shader 声明了 point shadow 采样器；检查用 1x1 array 必须先进入可采样布局。
    require_gpu(commands->begin(0), "Cannot begin radiance-check image transitions");
    ave::image_barrier(command, {
        .image = point_shadow->get_system_handle(),
        .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
        .src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        .dst_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .dst_access = VK_ACCESS_SHADER_READ_BIT,
        .layer_count = 6,
    });
    require_gpu(commands->end(0), "Cannot end radiance-check image transitions");
    require_gpu(commands->submit_and_wait(0, device->get_queue(family)),
        "Cannot submit radiance-check image transitions");
    // Run generation once per regression fixture, never inside the radiance parameter sweep.
    const auto generate_control = [&](const GpuFogControlField& field) {
        require_gpu(control_fields.upload(field), "Cannot upload control-density fixture");
        require_gpu(commands->reset(0), "Cannot reset control-density commands");
        require_gpu(commands->begin(0), "Cannot begin control-density commands");
        control_pipeline->dispatch(command, control_set, glm::uvec4(0u),
            (control_size + 3) / 4, (control_size + 3) / 4, (control_size + 3) / 4);
        ave::memory_barrier(command, {
            .src_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            .src_access = VK_ACCESS_SHADER_WRITE_BIT,
            .dst_stage = VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            .dst_access = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
        });
        require_gpu(commands->end(0), "Cannot end control-density commands");
        require_gpu(commands->submit_and_wait(0, device->get_queue(family)),
            "Cannot submit control-density dispatch");
        auto mapped = control_density.map();
        require_gpu(mapped && mapped.invalidate(), "Cannot invalidate control-density readback");
        mapped.cancel_auto_upload();
        const auto* values = static_cast<const float*>(mapped.raw_data());
        std::vector<float> result(values, values + control_cells);
        for(float value : result) require_gpu(std::isfinite(value) && value >= 0.0f && value <= 1.0f,
            "Control density is nonfinite or outside [0,1]");
        return result;
    };
    GpuFogControlField field {};
    const auto empty = generate_control(field);
    require_gpu(std::ranges::all_of(empty, [](float value) { return value == 0.0f; }),
        "Empty control field is not zero");
    field.info = glm::uvec4(1, 17, 1, 0);
    field.points[0].position_radius = glm::vec4(0.5f, 0.5f, 0.5f, 0.35f);
    field.points[0].density_softness = glm::vec4(2.0f, 0.5f, 0.0f, 0.0f);
    const auto plain = generate_control(field);
    const float core_density = 1.0f - std::exp(-2.0f);
    for(unsigned z = 0; z < control_size; ++z) for(unsigned y = 0; y < control_size; ++y)
        for(unsigned x = 0; x < control_size; ++x) {
            const glm::vec3 p = glm::vec3(x, y, z) / float(control_size - 1);
            const float radius = glm::length(p - glm::vec3(0.5f));
            const float value = plain[(std::size_t(z) * control_size + y) * control_size + x];
            if(radius <= 0.175f) require_gpu(std::abs(value - core_density) < 0.000002f,
                "Single control point core disagrees with 1-exp(-density)");
            if(radius >= 0.35f) require_gpu(value == 0.0f, "Control density escaped point radius");
        }
    field.noise = glm::vec4(3.5f, 0.8f, 0.0f, 0.0f);
    field.noise_offset = glm::vec4(0.13f, 0.27f, 0.41f, 0.0f);
    const auto single_octave = generate_control(field);
    field.info.z = 4;
    const auto noisy = generate_control(field);
    require_gpu(noisy != plain && noisy != single_octave, "Multilayer noise did not change the control field");
    require_gpu(noisy == generate_control(field), "Control noise is not repeatable");
    field.info.y = 91;
    const auto reseeded = generate_control(field);
    require_gpu(reseeded != noisy, "Control noise seed did not change the field");
    require_gpu(reseeded == generate_control(field), "Reseeded control noise is not repeatable");
    // Generate empty again after populated fields to detect stale density on regeneration.
    require_gpu(empty == generate_control(GpuFogControlField{}), "Empty regeneration retained stale control density");
    std::size_t checked = 0;
    bool images_initialized = false;
    for(float g : { -0.85f, -0.45f, 0.0f, 0.45f, 0.85f })
        for(float density : { 0.0f, 1e-12f, 0.035f, 0.475f, 3.0f }) for(float exposure : { -3.0f, 0.0f, 3.0f })
            for(bool cutoff : { false, true }) for(bool locals : { false, true }) for(bool slant : {false, true})
                for(bool encoded : {false, true}) {
        CameraFogUbo ubo {};
        ubo.volume_params.x = g;
        ubo.fog_extra.w = density;
        ubo.fog_params = glm::vec4(8.0f, 300.0f, 0.0f, 0.0f);
        ubo.view = glm::mat4(1.0f);
        ubo.proj = glm::mat4(1.0f);
        ubo.inverse_view_proj = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, -1));
        ubo.inverse_view_proj[3].x = slant ? 1.0f : 0.0f;
        ubo.froxel_grid = glm::vec4(1, 1, 64, 1);
        ubo.fog_anim.w = cutoff ? 1.0f : 0.0f;
        ubo.local_fog_info.x = locals ? 1.0f : 0.0f;
        ubo.fog_color = glm::vec4(0.7f, 0.8f, 0.9f, 1.0f);
        ubo.light_direction.w = 0.24f;
        ubo.volume_params.y = 0.9f;
        ubo.display_params = glm::vec4(exposure, 1.0f, encoded ? 1.0f : 0.0f, 0.0f);
        LocalFogUpload local;
        if(locals) {
            local.counts.x = 1;
            // A local slab beyond the old end=55 remains part of the world-space medium.
            auto& slab = local.entries[0];
            slab.world_to_local = glm::inverse(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -72))
                * glm::scale(glm::mat4(1.0f), glm::vec3(10, 10, 4)));
            slab.params = glm::vec4(0.2f, 0, 0, 0);
            slab.scattering = glm::vec4(0.7f, 0.8f, 0.9f, 0.9f);
            slab.bounds = glm::vec4(0, 0, -72, glm::length(glm::vec3(10, 10, 4)) * 0.5f);
        }
        require_gpu(media.upload(local), "Cannot upload radiance local media");
        std::array<glm::vec4, 64> coefficients;
        coefficients.fill(glm::vec4(glm::vec3(ubo.fog_color) * (density * 0.9f * 0.24f), density));
        require_gpu(filtered.upload(coefficients), "Cannot upload homogeneous integration coefficients");
        require_gpu(camera.upload(ubo), "Cannot upload radiance UBO");
        require_gpu(commands->reset(0), "Cannot reset compute commands");
        require_gpu(commands->begin(0), "Cannot begin compute commands");
        if(!images_initialized) {
            std::array<ave::ImageBarrier, 2> transitions {};
            for(unsigned index = 0; index < transitions.size(); ++index) {
                auto& transition = transitions[index];
                transition.image = (index == 0 ? shadow : atlas)->get_system_handle();
                transition.old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
                transition.new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                transition.dst_access = VK_ACCESS_SHADER_READ_BIT;
                transition.src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                transition.dst_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            }
            ave::image_barriers(command, transitions);
        }
        integrate_pipeline->bind_descriptor_set(command, integrate_set);
        integrate_pipeline->dispatch(command, 1, 1);
        ave::memory_barrier(command, {
            .src_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            .src_access = VK_ACCESS_SHADER_WRITE_BIT,
            .dst_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            .dst_access = VK_ACCESS_SHADER_READ_BIT,
        });
        pipeline->bind_descriptor_set(command, set);
        pipeline->dispatch(command, 1);
        ave::memory_barrier(command, {
            .src_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            .src_access = VK_ACCESS_SHADER_WRITE_BIT,
            .dst_stage = VK_PIPELINE_STAGE_HOST_BIT,
            .dst_access = VK_ACCESS_HOST_READ_BIT,
        });
        require_gpu(commands->end(0), "Cannot end compute commands");
        require_gpu(commands->submit_and_wait(0, device->get_queue(family)), "Cannot submit radiance dispatch");
        images_initialized = true;
        auto mapped = output.map();
        require_gpu(mapped && mapped.invalidate(), "Cannot invalidate GPU readback");
        mapped.cancel_auto_upload();
        const auto* values = static_cast<const glm::vec4*>(mapped.raw_data());
        for(unsigned i = 0; i < 64; ++i) {
            const auto composite = values[i];
            const auto source_data = values[64 + i];
            const auto source = values[128 + i];
            const auto fog = values[192 + i];
            const auto near = values[256 + i];
            const auto path = values[320 + i];
            const auto distant_lamp = values[384 + i];
            const auto surface = values[448 + i];
            const auto sky = values[512 + i];
            const float visibility[] = {1e-6f, 0.0001f, 0.001f, 0.01f, 0.1f, 0.5f, 0.9f, 1.0f};
            const float visible = visibility[i / 8];
            const auto linear_display = [encoded](float value) {
                if(!encoded) return value;
                return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
            };
            for(int c = 0; c < 3; ++c) {
                const float marker = linear_display(values[576 + i][c]);
                const float background = linear_display(values[640 + i][c]);
                const float material = linear_display(values[832 + i][c]);
                const float reflected = linear_display(values[896 + i][c]);
                require_gpu(std::abs(marker - background) <= visible + 0.000004f,
                    "Emissive marker screen contrast exceeds fog visibility (HDR is saturating through fog)");
                require_gpu(std::abs(material - reflected) <= visible + 0.000004f,
                    "Ordinary material emission bypassed fog visibility");
                if(visible <= 0.0001f) require_gpu(std::abs(values[576 + i][c] - values[640 + i][c]) < 1.0f / 255.0f,
                    "Heavily obscured emissive marker still differs by an 8-bit display level");
                const float glow = linear_display(values[704 + i][c]);
                require_gpu(std::abs(marker - std::min(background + visible * glow, 1.0f)) < 0.000004f,
                    "Emission is not attenuated in linear display space");
                require_gpu(std::abs(values[704 + i][c] - values[768 + i][c]) < 0.000002f,
                    "No-fog emission display changed");
                require_gpu(std::abs(values[960 + i][c] - values[1024 + i][c]) < 0.000004f,
                    "No-fog reflective/emissive material display changed");
            }
            const float distances[] = {1, 8, 16, 55, 80, 120, 240, 400};
            const float view_distance = distances[i / 8];
            const float tau = density * std::max(view_distance - (cutoff ? 8.0f : 0.0f), 0.0f)
                + (locals ? 0.2f * std::clamp(view_distance - 70.0f, 0.0f, 4.0f) : 0.0f);
            const float expected_t = std::exp(-tau);
            if(!locals) {
                const bool in_frustum = view_distance * (slant ? 0.70710678f : 1.0f) <= ubo.fog_params.y;
                if(in_frustum) require_gpu(std::abs(surface.a - expected_t) < std::max(1e-7f, expected_t * 0.0003f),
                    "Production froxel sampling froze attenuation for ordinary surfaces");
                require_gpu(density == 0.0f ? std::abs(sky.a - 1.0f) < 1e-6f : sky.a < 1e-6f,
                    "Sky stopped at a finite distance instead of converging optically");
                for(int c = 0; c < 3; ++c) {
                    if(in_frustum) require_gpu(std::abs(surface[c] - ubo.fog_color[c] * 0.9f * 0.24f * (1.0f - expected_t)) < 0.00005f,
                        "Production froxel scattering disagrees with homogeneous transport");
                    const float expected_sky = density == 0.0f ? 0.0f : ubo.fog_color[c] * 0.9f * 0.24f;
                    require_gpu(std::abs(sky[c] - expected_sky) < 0.00005f, "Sky asymptote is incorrect");
                }
            }
            require_gpu(std::abs(path.a - expected_t) < std::max(1e-7f, expected_t * 0.0003f),
                "Full camera-to-emitter transmittance disagrees with analytic global/local optical depth");
            for(int c = 0; c < 3; ++c) {
                const float expected_s = ubo.fog_color[c] * 0.9f * 0.24f * (1.0f - expected_t);
                require_gpu(std::abs(path[c] - expected_s) < 0.00005f,
                    "Full camera-to-emitter scattering disagrees with analytic transport");
                require_gpu(std::isfinite(distant_lamp[c]), "Distant lamp display is not finite");
                if(density == 0.0f && !locals)
                    require_gpu(std::abs(distant_lamp[c] - values[128 + i][c]) < 0.00001f,
                        "Vacuum introduced artificial view-distance dimming");
                if(i >= 8) require_gpu(distant_lamp[c] <= values[384 + i - 8][c] + 0.00001f,
                    "Distant lamp brightened while optical depth increased");
                if(density == 0.035f && exposure == 0.0f && i / 8 == 7)
                    require_gpu(distant_lamp[c] < values[384 + 24][c] - 0.1f,
                        "Lamp remained visually saturated beyond the froxel far boundary");
            }
            const float expected_start = cutoff ? 8.0f : 0.0f;
            require_gpu(near.x == expected_start && near.y == (locals ? 0.0f : expected_start),
                "Global/local near-cutoff interval mismatch");
            const float near_distance = 0.5f + float(i % 16);
            const float expected_near_t = std::exp(-density * std::max(near_distance - expected_start, 0.0f));
            require_gpu(std::abs(near.z - expected_near_t) < 0.00001f,
                "Near-distance transmittance disagrees with the configured cutoff");
            require_gpu(near.w == (cutoff && locals && near_distance < expected_start ? 0.0f : 1.0f),
                "Global injection cutoff leaked into local-medium range");
            require_gpu(std::isfinite(composite.w) && composite.w > 0.0f && composite.w <= 1.00001f,
                "GPU finite-emitter probability is out of bounds");
            require_gpu(std::isfinite(source_data.x) && source_data.x >= 0.0f && source_data.x <= source_data.y + 0.001f,
                "GPU scattering exceeds source radiance");
            const double mu = -1.0 + 2.0 * double(i % 8) / 7.0;
            const double phase = (1.0 - double(g) * g) / (4.0 * 3.141592653589793
                * std::pow(1.0 + double(g) * g - 2.0 * g * mu, 1.5));
            require_gpu(std::abs(source_data.z - phase) <= std::max(1e-6, phase * 0.0001),
                "GPU HG normalization disagrees with analytic phase");
            if(i / 8 == 0) {
                const double expected = 24.0 * 1e-6 / (0.11 * 0.11) * phase;
                require_gpu(std::abs(source_data.x / expected - 1.0) < 0.0002,
                    "GPU finite emitter does not approach point-source transport");
            }
            for(int c = 0; c < 3; ++c) {
                require_gpu(std::isfinite(composite[c]) && composite[c] >= 0.0f && composite[c] <= 1.00001f,
                    "GPU mapped composite is out of display range");
                require_gpu(source[c] > 0.95f && source[c] <= 1.00001f, "Emitter is not bright without fog");
                require_gpu(fog[c] >= 0.0f && fog[c] <= source[c] + 0.0001f, "Mapped fog exceeds the emitter");
                if(density == 0.0f) {
                    require_gpu(std::abs(composite[c] - source[c]) < 0.0001f, "Zero fog changed source radiance");
                    require_gpu(std::abs(fog[c]) < 0.00001f, "Zero fog generated scattering");
                }
            }
            ++checked;
        }
    }
    return checked;
}
} // namespace

int check_radiance_gpu() {
    try {
        ave::Context context;
        auto instance = std::make_shared<ave::Instance>();
        ave::CreateInstanceInfo ci { .ctx = context, .application_name = "fog radiance check", .api_version = ave::ave_vk_1_0 };
        const bool validation = ave::instance_layer_available("VK_LAYER_KHRONOS_validation");
        if(validation) { ci.enable_layer("VK_LAYER_KHRONOS_validation"); ci.enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); }
        require_gpu(instance->create(ci), "Cannot create headless Vulkan instance");
        std::atomic_uint32_t errors { 0 };
        std::shared_ptr<ave::DebugMessenger> messenger;
        if(validation) {
            messenger = ave::DebugMessenger::create(instance, {
                .severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .on_message_advanced = [&](auto severity, auto, const auto& data) {
                    if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ++errors;
                    std::cerr << (data.pMessage ? data.pMessage : "Vulkan validation message") << std::endl;
                    return false;
                }
            });
            require_gpu(bool(messenger), "Cannot create validation messenger");
        }
        const auto checked = exercise_radiance(instance);
        require_gpu(errors == 0, "Vulkan validation reported errors");
        std::cout << "Radiance GPU PASS: " << checked << " cases, production control-density empty/core/bounds/multilayer/seed/repeatability, emissive display contrast/sRGB, production froxel integration/sampling, slanted rays, optical sky convergence, near-cutoff modes, zero-fog invariance; validation "
            << (validation ? "enabled, zero errors" : "unavailable") << std::endl;
        return 0;
    } catch(const std::exception& error) {
        std::cerr << "Radiance GPU FAIL: " << error.what() << std::endl;
        return 1;
    }
}
