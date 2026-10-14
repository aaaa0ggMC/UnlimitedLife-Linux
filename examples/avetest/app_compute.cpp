#include "app.h"

namespace avetest {

namespace {
    void check_compute(bool value, const char* message) {
        if(!value) {
            std::cerr << "ComputePipeline FAIL: " << message << std::endl;
            std::exit(1);
        }
    }

    struct ComputeParams {
        uint32_t count;
        uint32_t bias;
    };
    static_assert(ave::is_std430_compatible_v<ComputeParams>);
}

static void exercise_compute_pipeline(const std::shared_ptr<ave::Instance>& instance) {
    uint32_t device_count = 0;
    check_compute(vkEnumeratePhysicalDevices(instance->get_system_handle(), &device_count, nullptr)
        == VK_SUCCESS && device_count > 0, "enumerate physical devices");
    std::vector<VkPhysicalDevice> physical_devices(device_count);
    check_compute(vkEnumeratePhysicalDevices(instance->get_system_handle(), &device_count,
        physical_devices.data()) == VK_SUCCESS, "read physical devices");

    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    uint32_t queue_family = 0;
    for(const auto candidate : physical_devices) {
        const auto info = ave::PhysicalDeviceInfo::query(candidate).as_gpu();
        if(info.compute_queue_family) {
            physical_device = candidate;
            queue_family = *info.compute_queue_family;
            break;
        }
    }
    check_compute(physical_device != VK_NULL_HANDLE, "find compute queue");
    auto device = ave::Device::create({
        .instance = instance,
        .physical_device = physical_device,
        .queues = {{ queue_family, { 1.0f } }}
    });
    check_compute(bool(device), "create compute device");

    // Renderer 不需要 swapchain、LegacyRender 或 DynamicRender 也可以创建计算管线。
    ave::Renderer renderer;
    renderer.device = device;
    constexpr uint32_t multiplier = 7;
    const VkSpecializationMapEntry entry { 0, 0, sizeof(multiplier) };
    const VkSpecializationInfo specialization { 1, &entry, sizeof(multiplier), &multiplier };
    const auto configure = [&](ave::CreateComputePipelineInfo& ci) {
        ci.shader_stages.front().specialization_info = &specialization;
        ci.descriptor_bindings = { ave::DescriptorBinding::ssbo(0, VK_SHADER_STAGE_COMPUTE_BIT) };
        ci.constant_attributes = {{
            .name = "params",
            .size = sizeof(ComputeParams),
            .stage_flags = VK_SHADER_STAGE_COMPUTE_BIT
        }};
    };
    constexpr std::string_view shader_path = "avetest/shaders/compute-test.spv";
    auto pipeline = renderer.create_compute_pipeline(shader_path, configure);
    check_compute(pipeline && *pipeline, "create pipeline from file");
    check_compute(pipeline->is_compute() && !pipeline->is_graphics() &&
        !pipeline->is_dynamic() && !pipeline->is_legacy(), "pipeline type");
    check_compute(pipeline->get_bind_point() == VK_PIPELINE_BIND_POINT_COMPUTE, "bind point");
    check_compute(pipeline->get_push_constant_size() == sizeof(ComputeParams) &&
        pipeline->get_push_constant_stage_flags() == VK_SHADER_STAGE_COMPUTE_BIT,
        "generated push constant range");
    check_compute(pipeline->get_owned_descriptor_set_layouts().size() == 1 &&
        pipeline->get_descriptor_set_layout(1) == VK_NULL_HANDLE, "owned layout and bounds");

    std::string shader;
    check_compute(alib6::io::read_all(shader_path, shader) == shader.size() && !shader.empty(),
        "read bytecode");
    const ave::ShaderBytecode bytecode {
        reinterpret_cast<const alib6::u8*>(shader.data()), shader.size()
    };
    auto bytecode_pipeline = renderer.create_compute_pipeline(bytecode, configure);
    check_compute(bool(bytecode_pipeline), "create pipeline from bytecode");

    // 原生 module 重载保留调用方所有权，并覆盖 descriptor_sets 创建路径。
    std::vector<uint32_t> aligned_code(shader.size() / sizeof(uint32_t));
    std::memcpy(aligned_code.data(), shader.data(), shader.size());
    VkShaderModuleCreateInfo module_info {};
    module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_info.codeSize = shader.size();
    module_info.pCode = aligned_code.data();
    VkShaderModule shader_module = VK_NULL_HANDLE;
    check_compute(vkCreateShaderModule(device->get_system_handle(), &module_info,
        instance->get_vk_allocator(), &shader_module) == VK_SUCCESS, "create external shader module");
    ave::CreateComputePipelineInfo native_info;
    native_info.device = device;
    native_info.shader_stages = {{ .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader_module }};
    configure(native_info);
    native_info.descriptor_sets = { ave::DescriptorSetLayoutInfo{ native_info.descriptor_bindings } };
    native_info.descriptor_bindings.clear();
    auto native_pipeline = ave::ComputePipeline::create(native_info);
    auto native_copy = ave::ComputePipeline::create(native_info);
    check_compute(native_pipeline && native_copy, "create pipelines sharing external module");
    vkDestroyShaderModule(device->get_system_handle(), shader_module, instance->get_vk_allocator());
    native_copy.reset();

    // 借用外部 layout，使用显式 push range；销毁借用者不能销毁原管线的 layout。
    auto borrowed_pipeline = ave::ComputePipeline::create(device, bytecode,
    [&](ave::CreateComputePipelineInfo& ci) {
        ci.descriptor_set_layouts = { pipeline->get_descriptor_set_layout() };
        ci.push_constant_ranges = {{ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputeParams) }};
    });
    check_compute(borrowed_pipeline && borrowed_pipeline->get_owned_descriptor_set_layouts().empty(),
        "borrowed descriptor layout");
    borrowed_pipeline->destroy();
    borrowed_pipeline->destroy();
    check_compute(!*borrowed_pipeline, "idempotent destroy");

    alib6::Error error;
    ave::CreateComputePipelineInfo invalid;
    invalid.ew = error;
    check_compute(!ave::ComputePipeline::create(invalid) && error.has_error(), "reject missing device");
    check_compute(!ave::ComputePipeline::create(device, ave::ShaderBytecode{}, nullptr, error),
        "reject empty bytecode");
    check_compute(!ave::ComputePipeline::create(device, bytecode.first(3), nullptr, error),
        "reject unaligned bytecode size");
    check_compute(!ave::ComputePipeline::create(device, "avetest/no-such-compute.spv", nullptr, error),
        "reject missing file");
    const auto reject = [&](const ave::ConfigureComputePipeline& change) {
        return !ave::ComputePipeline::create(device, bytecode,
        [&](ave::CreateComputePipelineInfo& ci) {
            configure(ci);
            change(ci);
        }, error);
    };
    check_compute(reject([](auto& ci) { ci.shader_stages.clear(); }), "reject missing stage");
    check_compute(reject([](auto& ci) { ci.shader_stages.push_back(ci.shader_stages.front()); }),
        "reject duplicate stages");
    check_compute(reject([](auto& ci) { ci.shader_stages.front().stage = VK_SHADER_STAGE_VERTEX_BIT; }),
        "reject graphics stage");
    check_compute(reject([](auto& ci) { ci.shader_stages.front().module = VK_NULL_HANDLE; }),
        "reject null module");
    check_compute(reject([](auto& ci) { ci.shader_stages.front().entry_point.clear(); }),
        "reject empty entry point");
    check_compute(reject([](auto& ci) { ci.base_pipeline_index = 0; }), "reject single-pipeline base index");
    check_compute(reject([](auto& ci) { ci.push_constant_ranges = {{ VK_SHADER_STAGE_COMPUTE_BIT, 1, 4 }}; }),
        "reject unaligned push constant range");
    check_compute(reject([](auto& ci) { ci.push_constant_ranges = {{ VK_SHADER_STAGE_VERTEX_BIT, 0, 4 }}; }),
        "reject wrong push constant stage");
    check_compute(reject([](auto& ci) {
        ci.push_constant_ranges = {{ VK_SHADER_STAGE_COMPUTE_BIT, 0, 4 },
                                  { VK_SHADER_STAGE_COMPUTE_BIT, 4, 4 }};
    }), "reject repeated push constant stage");
    check_compute(reject([](auto& ci) {
        ci.constant_attributes.front().offset = std::numeric_limits<uint32_t>::max() - 3;
    }), "reject constant attribute overflow");
    check_compute(reject([](auto& ci) {
        ci.push_constant_ranges = {{ VK_SHADER_STAGE_COMPUTE_BIT, 0,
            ci.device->get_physical_device_limits().maxPushConstantsSize + 4 }};
    }), "reject oversized push constants");

    constexpr uint32_t count = 257;
    constexpr uint32_t padded_count = 320;
    std::vector<uint32_t> values(padded_count);
    std::iota(values.begin(), values.end(), 0u);
    auto allocator = ave::VMAAllocator::create_shared({ .device = device });
    ave::VMABuffer buffer({
        .allocator = allocator,
        .size = values.size() * sizeof(uint32_t),
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .host_access = ave::BufferHostAccess::RandomAccess
    });
    check_compute(buffer.upload(values), "upload SSBO");
    auto descriptor_pool = ave::DescriptorPool::create({
        .device = device,
        .pool_sizes = { ave::DescriptorPoolSize::storage_buffer(1) },
        .max_sets = 1
    });
    const auto descriptor_set = descriptor_pool->allocate_and_write(
        pipeline->get_descriptor_set_layout(),
        { ave::DescriptorWrite::ssbo(0, buffer) }
    );
    auto pool = ave::CommandPool::create({ .device = device, .queue_family = queue_family });
    auto commands = ave::CommandBuffers::create({ .pool = pool, .count = 1 });
    const auto command_buffer = commands->get_buffers().front();
    VkCommandBufferBeginInfo begin_info {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check_compute(vkBeginCommandBuffer(command_buffer, &begin_info) == VK_SUCCESS, "begin command buffer");
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline->get_layout(), 0, 1, &descriptor_set, 0, nullptr);
    const ComputeParams params { count, 11 };
    vkCmdPushConstants(command_buffer, pipeline->get_layout(), VK_SHADER_STAGE_COMPUTE_BIT,
        0, sizeof(params), &params);
    const std::array pipelines { pipeline, bytecode_pipeline, native_pipeline };
    for(std::size_t index = 0; index < pipelines.size(); ++index) {
        if(index > 0) {
            VkMemoryBarrier dependency {};
            dependency.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            dependency.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &dependency, 0, nullptr, 0, nullptr);
        }
        pipelines[index]->dispatch(command_buffer, padded_count / 64);
    }

    VkMemoryBarrier barrier {};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    check_compute(vkEndCommandBuffer(command_buffer) == VK_SUCCESS, "end command buffer");
    VkSubmitInfo submit_info {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    const auto queue = device->get_queue(queue_family);
    check_compute(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE) == VK_SUCCESS, "submit dispatch");
    check_compute(vkQueueWaitIdle(queue) == VK_SUCCESS, "wait for dispatch");
    auto mapped = buffer.map();
    check_compute(mapped && mapped.invalidate(), "invalidate readback");
    const auto* result = static_cast<const uint32_t*>(mapped.raw_data());
    for(uint32_t index = 0; index < padded_count; ++index) {
        auto expected = index;
        if(index < count) {
            for(std::size_t pass = 0; pass < pipelines.size(); ++pass) {
                expected = expected * multiplier + params.bias;
            }
        }
        check_compute(result[index] == expected, "SSBO result or tail guard");
    }
}

void test_compute_pipeline() {
    ave::Context context;
    auto instance = std::make_shared<ave::Instance>();
    ave::CreateInstanceInfo instance_info {
        .ctx = context,
        .application_name = "avetest compute",
        .api_version = ave::ave_vk_1_0
    };
    const bool validation = std::getenv("AVE_COMPUTE_VALIDATION") != nullptr;
    if(validation) {
        instance_info.enable_layer("VK_LAYER_KHRONOS_validation");
        instance_info.enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    check_compute(instance->create(instance_info), "create instance");
    std::atomic_uint32_t validation_errors { 0 };
    std::shared_ptr<ave::DebugMessenger> messenger;
    if(validation) {
        messenger = ave::DebugMessenger::create(instance, {
            .severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .on_message_advanced = [&](auto severity, auto, const auto& data) {
                if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ++validation_errors;
                std::cerr << (data.pMessage ? data.pMessage : "<no message>") << std::endl;
                return false;
            }
        });
        check_compute(bool(messenger), "create validation messenger");
    }

    exercise_compute_pipeline(instance);
    check_compute(validation_errors == 0, "Vulkan validation errors (including resource cleanup)");
    std::cout << "ComputePipeline PASS: 257 values, specialization, push constants, tail guard and invalid inputs."
              << std::endl;
}

} // namespace avetest
