module;
#include <alib6/debug.h>
#include <vulkan/vulkan.h>

module ave.profile;

import ave.ecode;

using namespace ave;
using enum RenderBuildStageId;
using enum RenderBuildStageStatus;

namespace {
    struct DeviceExtensionResolution {
        std::vector<std::string> satisfied;
        std::vector<std::string> missing;
    };

    std::vector<std::string> collect_required_device_extensions(
        const ProfileWith& with,
        bool has_surface
    ) {
        auto result = with.required_device_extensions;
        const bool has_swapchain = std::ranges::any_of(
            result,
            [](const std::string& extension) {
                return extension == VK_KHR_SWAPCHAIN_EXTENSION_NAME;
            }
        );
        if(has_surface && with.add_khr_swapchain && !has_swapchain) {
            result.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        }
        return result;
    }

    DeviceExtensionResolution resolve_device_extensions(
        const PhysicalDeviceInfo& device,
        std::span<const std::string> requested
    ) {
        DeviceExtensionResolution result;
        result.satisfied.reserve(requested.size());
        result.missing.reserve(requested.size());
        for(const auto& extension : requested) {
            (device.supports_extension(extension)
                ? result.satisfied
                : result.missing
            ).push_back(extension);
        }
        return result;
    }
}

Renderer RenderProfile::build(alib6::ErrorWrapper ew){
    auto * result = std::exchange(build_report, nullptr);
    if(result) result->reset();
    struct FinishResult {
        RenderBuildReport * result;
        ~FinishResult(){
            if(result) result->finish();
        }
    } finish_result { result };

    Renderer renderer;
    renderer.context = &ctx;

    /// Instance 始终是第一阶段；仅提供 Device 时可沿依赖取得它绑定的 Instance。
    if(!with_data.instance && !with_data.device){
        // 用户可以自己销毁这个 instance,如果不需要的话
        if(!__vk_instance(renderer, ew, result)){
            return renderer;
        }
    }else{
        renderer.instance = with_data.instance
            ? with_data.instance
            : with_data.device->get_instance();
        const bool valid =
            renderer.instance &&
            renderer.instance->get_system_handle() != VK_NULL_HANDLE;
        if(result){
            result->skip({
                instance_extensions,
                instance_layers
            }, with_data.instance
                ? "Instance was externally provided"
                : "Instance was obtained from external Device");

            auto& stage = (*result)[create_instance];
            if(valid) stage.succeed();
            else stage.fail("Provided Instance is not valid");
            stage["source"] = with_data.instance
                ? "provided"
                : "device_dependency";
            stage["instance_handle"] = std::format("{}", static_cast<const void*>(renderer.instance.get()));
        }
        if(!valid){
            ew.report(
                ave_vk_create_instance,
                "The provided Vulkan Instance is not valid."
            );
            return renderer;
        }
    }

    /// debug messenger
    if(with_data.debug_messenger) {
        const bool instance_matches =
            with_data.debug_messenger->get_instance() == renderer.instance;
        if(result){
            auto & stage = (*result)[debug_messenger];
            if(instance_matches){
                stage.succeed();
            }else{
                stage.fail("Provided debug messenger belongs to a different Vulkan instance");
            }
            stage["source"] = "provided";
            stage["instance_matches"] = instance_matches;
            stage["messenger_handle"] = std::format("{}", static_cast<const void*>(with_data.debug_messenger.get()));
        }
        panic_debug(
            !instance_matches,
            "The provided debug messenger belongs to a different Vulkan instance."
        );
        // 不匹配别伤害好吧
        if(instance_matches) renderer.debug_messenger = with_data.debug_messenger;
    }else if(with_data.configure_debug_messenger) {
        auto ci = *with_data.configure_debug_messenger;
        ci.ew = ew;
        renderer.debug_messenger = DebugMessenger::create(renderer.instance, ci);
        if(result){
            auto & stage = (*result)[debug_messenger];
            if(renderer.debug_messenger){
                stage.succeed();
                stage["messenger_handle"] = std::format("{}", static_cast<const void*>(renderer.debug_messenger.get()));
            }else{
                stage.fail("Failed to create debug messenger");
            }
            stage["source"] = "created";
            stage["has_callback"] = (ci.on_message != nullptr);
        }
        if(!renderer.debug_messenger) renderer.instance.reset();
    }else if(result){
        auto & stage = result->skip(debug_messenger, "Debug messenger not configured");
        stage["source"] = "disabled";
    }

    /// surface: 支持使用已提供的 Surface bypass 创建
    if(with_data.surface) {
        renderer.surface = with_data.surface;
        if(result) {
            auto & stage = (*result)[create_surface];
            stage["source"] = "provided";
            stage["surface_handle"] = std::format(
                "{}",
                static_cast<const void*>(renderer.surface->get_system_handle())
            );
            stage.succeed();
        }
    }else if(window){
        if(!__vk_create_glfw_surface(renderer, ew, result)){
            return renderer;
        }
    }

    if(with_data.device){
        const bool valid_device =
            with_data.device->get_system_handle() != VK_NULL_HANDLE &&
            with_data.device->get_physical_device() != VK_NULL_HANDLE &&
            with_data.device->get_instance() == renderer.instance;
        if(!valid_device){
            if(result){
                result->skip(
                    select_physical_device,
                    "Physical device selection was overridden by external Device"
                );
                result->fail(
                    create_device,
                    "External Device is invalid or belongs to a different Instance"
                );
            }
            ew.report(
                ave_vk_create_device,
                "The provided Vulkan Device is invalid or belongs to a different Instance."
            );
            return renderer;
        }
        renderer.physical_device = PhysicalDeviceInfo::query(
            with_data.device->get_physical_device(),
            renderer.surface
                ? renderer.surface->get_system_handle()
                : VK_NULL_HANDLE
        );
        if(result){
            auto& stage = result->skip(
                select_physical_device,
                "Physical device was provided by external Device"
            );
            const auto gpu = renderer.physical_device->as_gpu();
            stage["name"] = gpu.name;
            stage["vendor_id"] = gpu.vendor_id;
            stage["device_id"] = gpu.device_id;
        }
    }else{
        /// 选择 Physical Device
        if(!__vk_select_physical_device(renderer, ew, result)){
            return renderer;
        }
    }

    /// 创建或接收 Logical Device
    if(!__vk_device(renderer, ew, result)){
        return renderer;
    }

    /// SurfaceRenderer 的最后一个基础阶段：创建或接收 Swapchain。
    if(!__vk_swapchain(renderer, ew, result)){
        return renderer;
    }

    if(!__vk_images(renderer, ew, result)){
        return renderer;
    }

    if(!__vk_sync_objects(renderer, ew, result)){
        return renderer;
    }

    const bool use_dynamic = !with_data.legacy_render &&
        renderer.device && renderer.device->supports_dynamic_rendering();
    if(use_dynamic) {
        if(!__vk_dynamic_render(renderer, ew, result)){
            return renderer;
        }
    }else{
        if(!__vk_legacy_render(renderer, ew, result)){
            return renderer;
        }
    }

    if(!__vk_command_pool(renderer, ew, result)){
        return renderer;
    }

    if(!__vk_command_buffers(renderer, ew, result)){
        return renderer;
    }

    return renderer;
}

bool RenderProfile::__vk_command_pool(
    Renderer& r,
    alib6::ErrorWrapper ew,
    RenderBuildReport* result
){
    if(with_data.command_pool) {
        const bool valid =
            static_cast<bool>(*with_data.command_pool) &&
            with_data.command_pool->get_device() == r.device &&
            with_data.command_pool->get_queue_family() ==
                r.swapchain->get_graphics_queue_family();
        if(result) {
            auto& stage = (*result)[create_command_pool];
            stage["source"] = "provided";
            stage["queue_family"] = with_data.command_pool->get_queue_family();
            if(valid) stage.succeed();
            else stage.fail(
                "Provided CommandPool is invalid or belongs to a different Device/queue family");
        }
        if(!valid) {
            ew.report(ave_vk_create_command_pool,
                "The provided CommandPool is invalid or belongs to a different Device/queue family.");
            return false;
        }
        r.command_pool = with_data.command_pool;
        return true;
    }

    WithCommandPoolInput input(r.device, r.swapchain);
    CreateCommandPoolInfo ci;
    ci.ew = ew;
    if(with_data.configure_command_pool) {
        with_data.configure_command_pool(input, ci);
    }else{
        default_configure_command_pool(input, ci);
    }
    const auto queue_family = ci.queue_family;
    r.command_pool = CommandPool::create(std::move(ci));

    if(result) {
        auto& stage = (*result)[create_command_pool];
        stage["source"] = "created";
        stage["queue_family"] = queue_family;
        if(r.command_pool) stage.succeed();
        else stage.fail("Failed to create Vulkan CommandPool");
    }
    return static_cast<bool>(r.command_pool);
}

bool RenderProfile::__vk_command_buffers(
    Renderer& r,
    alib6::ErrorWrapper ew,
    RenderBuildReport* result
){
    if(with_data.command_buffers) {
        const bool valid =
            static_cast<bool>(*with_data.command_buffers) &&
            with_data.command_buffers->get_pool() == r.command_pool &&
            with_data.command_buffers->size() >= r.sync_objects->size();
        if(result) {
            auto& stage = (*result)[allocate_command_buffers];
            stage["source"] = "provided";
            stage["count"] = with_data.command_buffers->size();
            if(valid) stage.succeed();
            else stage.fail(
                "Provided CommandBuffers is invalid, uses a different pool, or has insufficient frame buffers");
        }
        if(!valid) {
            ew.report(ave_vk_allocate_command_buffers,
                "The provided CommandBuffers is invalid, uses a different pool, or has insufficient frame buffers.");
            return false;
        }
        r.command_buffers = with_data.command_buffers;
        return true;
    }

    WithCommandBuffersInput input(r.command_pool, r.sync_objects);
    CreateCommandBuffersInfo ci;
    ci.ew = ew;
    if(with_data.configure_command_buffers) {
        with_data.configure_command_buffers(input, ci);
    }else{
        default_configure_command_buffers(input, ci);
    }
    const auto requested_count = ci.count;
    r.command_buffers = CommandBuffers::create(std::move(ci));

    if(result) {
        auto& stage = (*result)[allocate_command_buffers];
        stage["source"] = "allocated";
        stage["requested_count"] = requested_count;
        if(r.command_buffers) {
            stage.succeed();
            stage["count"] = r.command_buffers->size();
        }else{
            stage.fail("Failed to allocate Vulkan CommandBuffers");
        }
    }
    return static_cast<bool>(r.command_buffers);
}

bool RenderProfile::__vk_sync_objects(
    Renderer& r,
    alib6::ErrorWrapper ew,
    RenderBuildReport* result
){
    if(with_data.sync_objects) {
        const bool valid =
            static_cast<bool>(*with_data.sync_objects) &&
            with_data.sync_objects->get_device() == r.device &&
            with_data.sync_objects->size() >= r.swapchain->get_image_count();
        if(result) {
            auto& stage = (*result)[create_sync_objects];
            stage["source"] = "provided";
            stage["count"] = with_data.sync_objects->size();
            if(valid) stage.succeed();
            else stage.fail(
                "Provided SyncObjects is invalid, belongs to a different Device, or has insufficient frame slots");
        }
        if(!valid) {
            ew.report(ave_vk_create_sync_objects,
                "The provided SyncObjects is invalid, belongs to a different Device, or has insufficient frame slots.");
            return false;
        }
        r.sync_objects = with_data.sync_objects;
        return true;
    }

    WithSyncObjectsInput input(r.device, r.swapchain);
    CreateSyncObjectsInfo ci;
    ci.device = r.device;
    ci.ew = ew;
    ci.count = with_data.select_sync_objects_count
        ? with_data.select_sync_objects_count(input)
        : default_select_sync_objects_count(input);
    if(with_data.configure_sync_objects) {
        with_data.configure_sync_objects(input, ci);
    }
    const auto configured_count = ci.count;
    r.sync_objects = SyncObjects::create(std::move(ci));

    if(result) {
        auto& stage = (*result)[create_sync_objects];
        stage["source"] = "created";
        stage["requested_count"] = configured_count;
        if(r.sync_objects) {
            stage.succeed();
            stage["count"] = r.sync_objects->size();
        }else{
            stage.fail("Failed to create Vulkan synchronization objects");
        }
    }
    return static_cast<bool>(r.sync_objects);
}

bool RenderProfile::__vk_legacy_render(
    Renderer& r,
    alib6::ErrorWrapper ew,
    RenderBuildReport* result
){
    if(with_data.legacy_render) {
        const bool valid =
            static_cast<bool>(*with_data.legacy_render) &&
            with_data.legacy_render->get_swapchain() == r.swapchain;
        if(result) {
            auto& render_pass_stage = (*result)[create_render_pass];
            auto& framebuffer_stage = (*result)[create_framebuffers];
            render_pass_stage["source"] = "provided";
            framebuffer_stage["source"] = "provided";
            framebuffer_stage["count"] =
                with_data.legacy_render->get_framebuffers().size();
            if(valid) {
                render_pass_stage.succeed();
                framebuffer_stage.succeed();
            }else{
                render_pass_stage.fail(
                    "Provided LegacyRender is invalid or belongs to a different Swapchain");
                framebuffer_stage.skip("Provided LegacyRender validation failed");
            }
        }
        if(!valid) {
            ew.report(ave_vk_create_render_pass,
                "The provided LegacyRender is invalid or belongs to a different Swapchain.");
            return false;
        }
        r.legacy_render = with_data.legacy_render;
        return true;
    }

    WithLegacyRenderInput input(r.device, r.swapchain, r.images);
    CreateLegacyRenderInfo ci;
    ci.swapchain = r.swapchain;
    ci.ew = ew;
    if(with_data.configure_legacy_render) {
        with_data.configure_legacy_render(input, ci);
    }else{
        default_configure_legacy_render(input, ci);
    }

    LegacyRenderCreateStatus status;
    r.legacy_render = LegacyRender::create(std::move(ci), &status);
    if(result) {
        auto& render_pass_stage = (*result)[create_render_pass];
        auto& framebuffer_stage = (*result)[create_framebuffers];
        render_pass_stage["source"] = "created";
        framebuffer_stage["source"] = "created";
        framebuffer_stage["created_count"] = status.framebuffers_created;
        if(status.render_pass_created) render_pass_stage.succeed();
        else render_pass_stage.fail("Failed to create Vulkan RenderPass");
        if(r.legacy_render) {
            framebuffer_stage.succeed();
            framebuffer_stage["count"] =
                r.legacy_render->get_framebuffers().size();
        }else if(status.render_pass_created) {
            framebuffer_stage.fail("Failed to create all Vulkan Framebuffers");
        }else{
            framebuffer_stage.skip("RenderPass creation failed");
        }
    }
    return static_cast<bool>(r.legacy_render);
}

bool RenderProfile::__vk_dynamic_render(
    Renderer& r,
    alib6::ErrorWrapper ew,
    RenderBuildReport* result
){
    if(result) {
        result->skip(create_render_pass, "Dynamic rendering is active");
        result->skip(create_framebuffers, "Dynamic rendering is active");
    }

    if(with_data.dynamic_render) {
        const bool valid =
            static_cast<bool>(*with_data.dynamic_render) &&
            with_data.dynamic_render->get_swapchain() == r.swapchain;
        if(!valid) {
            ew.report(ave_vk_create_device,
                "The provided DynamicRender is invalid or belongs to a different Swapchain.");
            return false;
        }
        r.dynamic_render = with_data.dynamic_render;
        r.default_clear_values = r.dynamic_render->get_default_clear_values();
        return true;
    }

    WithDynamicRenderInput input(r.device, r.swapchain, r.images);
    CreateDynamicRenderInfo ci;
    ci.swapchain = r.swapchain;
    ci.ew = ew;
    if(with_data.configure_dynamic_render) {
        with_data.configure_dynamic_render(input, ci);
    } else {
        default_configure_dynamic_render(input, ci);
    }

    r.dynamic_render = DynamicRender::create(std::move(ci));
    if(r.dynamic_render) {
        r.default_clear_values = r.dynamic_render->get_default_clear_values();
    }
    return static_cast<bool>(r.dynamic_render);
}

bool RenderProfile::__vk_images(
    Renderer& r,
    alib6::ErrorWrapper ew,
    RenderBuildReport* result
){
    const auto swapchain_handles = r.swapchain->enumerate_images(ew);
    if(!swapchain_handles) {
        if(result) result->fail(
            create_images, "Failed to enumerate Swapchain Images");
        return false;
    }

    if(with_data.images) {
        const bool dependencies_valid = std::ranges::all_of(
            *with_data.images,
            [&](const auto& image) {
                return image && *image && image->get_device() == r.device;
            }
        );
        const bool swapchain_images_valid = std::ranges::all_of(
            *swapchain_handles,
            [&](VkImage handle) {
                return std::ranges::any_of(
                    *with_data.images,
                    [&](const auto& image) {
                        return image && image->get_swapchain() == r.swapchain &&
                            image->get_system_handle() == handle &&
                            image->get_image_view() != VK_NULL_HANDLE;
                    }
                );
            }
        );
        const bool valid = dependencies_valid && swapchain_images_valid;
        if(result) {
            auto& stage = (*result)[create_images];
            stage["source"] = "provided";
            stage["count"] = with_data.images->size();
            stage["swapchain_image_count"] = swapchain_handles->size();
            if(valid) stage.succeed();
            else stage.fail(
                "A provided Image is invalid or belongs to a different Device"
            );
        }
        if(!valid) {
            ew.report(ave_vk_create_image,
                "Provided Images are invalid, belong to another Device, or do not wrap every Swapchain Image.");
            return false;
        }
        r.images = *with_data.images;
        return true;
    }

    WithImagesInput input(r.device, r.swapchain);
    CreateImagesInfo ci;
    ci.device = r.device;
    ci.ew = ew;
    if(with_data.configure_images) {
        with_data.configure_images(input, ci);
    }else{
        default_configure_images(input, ci);
    }

    const auto requested_count = ci.images.size();
    std::vector<std::shared_ptr<Image>> created;
    created.reserve(swapchain_handles->size() + requested_count);
    for(std::size_t i = 0; i < swapchain_handles->size(); ++i) {
        auto image = Image::create_swapchain_image({
            .swapchain = r.swapchain,
            .image = (*swapchain_handles)[i],
            .image_view_next = ci.swapchain_image_view_next,
            .image_view_flags = ci.swapchain_image_view_flags,
            .image_view_type = ci.swapchain_image_view_type,
            .image_view_components = ci.swapchain_image_view_components,
            .image_view_subresource_range =
                ci.swapchain_image_view_subresource_range,
            .ew = ci.ew
        });
        if(!image) {
            if(result) {
                auto& stage = (*result)[create_images];
                stage["source"] = "created";
                stage["swapchain_image_count"] = swapchain_handles->size();
                stage["failed_swapchain_image_index"] = i;
                stage.fail("Failed to wrap a Swapchain Image");
            }
            return false;
        }
        created.push_back(std::move(image));
    }
    for(std::size_t i = 0; i < ci.images.size(); ++i) {
        auto request = std::move(ci.images[i]);
        if(!request.device) request.device = ci.device;
        request.ew = ci.ew;
        auto image = Image::create(std::move(request));
        if(!image || image->get_device() != r.device) {
            if(result) {
                auto& stage = (*result)[create_images];
                stage["source"] = "created";
                stage["requested_count"] = requested_count;
                stage["swapchain_image_count"] = swapchain_handles->size();
                stage["created_auxiliary_count"] =
                    created.size() - swapchain_handles->size();
                stage["failed_auxiliary_index"] = i;
                stage.fail("Failed to create a configured Vulkan Image");
            }
            return false;
        }
        created.push_back(std::move(image));
    }
    r.images = std::move(created);

    if(result) {
        auto& stage = (*result)[create_images];
        stage["source"] = "created";
        stage["requested_auxiliary_count"] = requested_count;
        stage["swapchain_image_count"] = swapchain_handles->size();
        stage["created_count"] = r.images.size();
        stage.succeed();
    }
    return true;
}

bool RenderProfile::__vk_swapchain(
    Renderer& r,
    alib6::ErrorWrapper ew,
    RenderBuildReport* result
){
    if(with_data.swapchain) {
        const bool valid =
            with_data.swapchain->get_system_handle() != VK_NULL_HANDLE &&
            with_data.swapchain->get_device() == r.device &&
            with_data.swapchain->get_surface() == r.surface;
        if(result) {
            auto& stage = (*result)[create_swapchain];
            stage["source"] = "provided";
            if(valid) {
                stage.succeed();
                const auto extent = with_data.swapchain->get_extent();
                stage["width"] = extent.width;
                stage["height"] = extent.height;
                stage["image_count"] = with_data.swapchain->get_image_count();
            }else{
                stage.fail(
                    "Provided Swapchain is invalid or belongs to different Device/Surface dependencies"
                );
            }
        }
        if(!valid) {
            ew.report(
                ave_vk_create_swapchain,
                "The provided Swapchain is invalid or belongs to different Device/Surface dependencies."
            );
            return false;
        }
        r.swapchain = with_data.swapchain;
        return true;
    }

    auto with = query_swapchain_support(r.device, r.surface, ew);
    if(!with) {
        if(result) result->fail(
            create_swapchain,
            "Failed to query Swapchain support"
        );
        return false;
    }

    CreateSwapchainInfo ci;
    ci.device = r.device;
    ci.surface = r.surface;
    ci.ew = ew;

    if(with_data.configure_swapchain) {
        with_data.configure_swapchain(*with, ci);
    }else{
        default_configure_swapchain(*with, ci);
    }

    const auto configured_format = ci.surface_format;
    const auto configured_present_mode = ci.present_mode;
    const auto configured_extent = ci.extent;
    const auto configured_image_count = ci.image_count;
    r.swapchain = Swapchain::create(std::move(ci), *with);

    if(result) {
        auto& stage = (*result)[create_swapchain];
        stage["source"] = "created";
        stage["format"] = static_cast<alib6::i64>(configured_format.format);
        stage["color_space"] = static_cast<alib6::i64>(configured_format.colorSpace);
        stage["present_mode"] = static_cast<alib6::i64>(configured_present_mode);
        stage["width"] = configured_extent.width;
        stage["height"] = configured_extent.height;
        stage["requested_image_count"] = configured_image_count;
        if(r.swapchain) {
            stage.succeed();
            stage["image_count"] = r.swapchain->get_image_count();
        }else{
            stage.fail("Failed to create Vulkan Swapchain or image views");
        }
    }
    return static_cast<bool>(r.swapchain);
}

bool RenderProfile::__vk_device(
    Renderer& r,
    alib6::ErrorWrapper ew,
    RenderBuildReport* result
){
    if(with_data.device){
        r.device = with_data.device;
        if(result){
            auto& stage = result->succeed(create_device);
            stage["source"] = "provided";
            stage["queue_family_count"] =
                r.device->get_queue_requests().size();
            stage["extensions"] = alib6::to_adata(
                r.device->get_enabled_extensions()
            );
            stage["dynamic_rendering"] = r.device->supports_dynamic_rendering()
                ? "provided" : "disabled";
        }
        return true;
    }

    if(!r.physical_device ||
       r.physical_device->device == VK_NULL_HANDLE) {
        if(result) result->fail(
            create_device,
            "No physical device was selected"
        );
        ew.report(
            ave_vk_create_device,
            "Cannot create a logical device before selecting a physical device."
        );
        return false;
    }

    const GPUInfo gpu = r.physical_device->as_gpu();
    if(!gpu.graphics_queue_family) {
        if(result) result->fail(
            create_device,
            "Selected physical device has no graphics queue"
        );
        ew.report(
            ave_vk_create_device,
            "Selected physical device has no graphics queue family."
        );
        return false;
    }

    CreateDeviceInfo ci;
    ci.instance = r.instance;
    ci.physical_device = r.physical_device->device;
    ci.ew = ew;
    ci.request_queue(*gpu.graphics_queue_family);
    if(gpu.present_queue_family &&
       *gpu.present_queue_family != *gpu.graphics_queue_family) {
        ci.request_queue(*gpu.present_queue_family);
    }
    const auto required_extensions = collect_required_device_extensions(
        with_data,
        static_cast<bool>(r.surface)
    );
    const auto required_resolution = resolve_device_extensions(
        *r.physical_device,
        required_extensions
    );
    const auto optional_resolution = resolve_device_extensions(
        *r.physical_device,
        with_data.optional_device_extensions
    );

    if(with_data.on_device_extensions_resolved) {
        with_data.on_device_extensions_resolved(
            true,
            required_resolution.satisfied,
            required_resolution.missing
        );
        with_data.on_device_extensions_resolved(
            false,
            optional_resolution.satisfied,
            optional_resolution.missing
        );
    }

    if(!required_resolution.missing.empty()) {
        if(result) {
            auto& stage = result->fail(
                create_device,
                "Selected physical device is missing required device extensions"
            );
            stage["required_extensions"] = alib6::to_adata(required_extensions);
            stage["missing_extensions"] = alib6::to_adata(
                required_resolution.missing
            );
        }
        ew.report(
            ave_vk_create_device,
            "Selected physical device is missing required device extensions."
        );
        return false;
    }

    for(const auto& extension : required_resolution.satisfied) {
        ci.enable_extension(extension);
    }
    for(const auto& extension : optional_resolution.satisfied) {
        ci.enable_extension(extension);
    }

    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_feature {};
    dynamic_rendering_feature.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamic_rendering_feature.dynamicRendering = VK_TRUE;

    std::string dynamic_rendering_mode = "disabled";
    if(with_data.try_dynamic_rendering) {
        const auto app_version = r.instance
            ? r.instance->get_api_version() : ave_vk_1_0;
        const auto gpu_version = r.physical_device->properties.apiVersion;
        const bool app_is_1_3_plus = (app_version.major > 1 ||
            (app_version.major == 1 && app_version.minor >= 3));
        const bool gpu_is_1_3_plus = (gpu_version >= VK_API_VERSION_1_3);

        VkPhysicalDeviceDynamicRenderingFeatures query_feature {};
        query_feature.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
        VkPhysicalDeviceFeatures2 features2 {};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &query_feature;

        auto pfnGetFeatures2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
            vkGetInstanceProcAddr(
                r.instance->get_system_handle(), "vkGetPhysicalDeviceFeatures2"
            )
        );
        if(!pfnGetFeatures2) {
            pfnGetFeatures2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
                vkGetInstanceProcAddr(
                    r.instance->get_system_handle(),
                    "vkGetPhysicalDeviceFeatures2KHR"
                )
            );
        }
        if(!pfnGetFeatures2) {
            pfnGetFeatures2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
                vkGetInstanceProcAddr(
                    nullptr, "vkGetPhysicalDeviceFeatures2"
                )
            );
        }
        if(!pfnGetFeatures2) {
            pfnGetFeatures2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
                vkGetInstanceProcAddr(
                    nullptr, "vkGetPhysicalDeviceFeatures2KHR"
                )
            );
        }
        if(pfnGetFeatures2) {
            pfnGetFeatures2(r.physical_device->device, &features2);
        }

        if(app_is_1_3_plus && gpu_is_1_3_plus &&
           query_feature.dynamicRendering == VK_TRUE) {
            dynamic_rendering_mode = "core_1_3";
            dynamic_rendering_feature.pNext = const_cast<void*>(ci.next);
            ci.next = &dynamic_rendering_feature;
            ci.enable_dynamic_rendering = true;
        }else if(r.physical_device->supports_extension(
                     VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) &&
                 query_feature.dynamicRendering == VK_TRUE) {
            dynamic_rendering_mode = "extension_khr";
            ci.enable_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
            const bool app_is_1_2_plus = (app_version.major > 1 ||
                (app_version.major == 1 && app_version.minor >= 2));
            if(!app_is_1_2_plus || gpu_version < VK_API_VERSION_1_2) {
                if(r.physical_device->supports_extension(
                       VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME)) {
                    ci.enable_extension(
                        VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME
                    );
                }
                if(r.physical_device->supports_extension(
                       VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME)) {
                    ci.enable_extension(
                        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME
                    );
                }
            }
            dynamic_rendering_feature.pNext = const_cast<void*>(ci.next);
            ci.next = &dynamic_rendering_feature;
            ci.enable_dynamic_rendering = true;
        }
    }

    WithSelectedPhysicalDevice selected(*r.physical_device);
    if(with_data.configure_device) {
        with_data.configure_device(selected, ci);
    }

    const auto enabled_extensions = ci.extensions;
    const auto queue_families = ci.queues
        | std::views::transform(&DeviceQueueRequest::family_index)
        | std::ranges::to<std::vector>();
    r.device = Device::create(std::move(ci));

    if(result){
        auto& stage = (*result)[create_device];
        stage["source"] = "created";
        stage["extensions"] = alib6::to_adata(enabled_extensions);
        stage["queue_families"] = alib6::to_adata(queue_families);
        if(r.device){
            stage.succeed();
            stage["dynamic_rendering"] = r.device->supports_dynamic_rendering()
                ? dynamic_rendering_mode : "disabled";
        }else{
            stage.fail("Failed to create Vulkan logical device");
        }
    }
    return static_cast<bool>(r.device);
}

bool RenderProfile::__vk_select_physical_device(
    Renderer & r,
    alib6::ErrorWrapper ew,
    RenderBuildReport * result
){
    const auto required_extensions = collect_required_device_extensions(
        with_data,
        static_cast<bool>(r.surface)
    );
    WithPhysicalDevices input(
        r.instance,
        r.surface,
        required_extensions
    );
    const auto& devices = input.enumerate_physical_devices();

    std::optional<std::size_t> selected;
    const bool custom_selector = static_cast<bool>(
        with_data.select_physical_device
    );

    if(custom_selector){
        selected = with_data.select_physical_device(input);
    }else if(!devices.empty()){
        // 显式清空 selector 时，按约定直接选择枚举到的第一个设备。
        selected = 0;
    }

    const bool valid_index =
        selected.has_value() &&
        *selected < devices.size() &&
        devices[*selected].device != VK_NULL_HANDLE;
    const bool supports_required_extensions =
        valid_index && input.supports_required_extensions(devices[*selected]);
    const bool valid_selection =
        valid_index && supports_required_extensions;

    if(result){
        auto& stage = (*result)[select_physical_device];
        stage["device_count"] = devices.size();
        stage["selector"] = custom_selector ? "configured" : "first";
        stage["required_device_extensions"] = alib6::to_adata(
            required_extensions
        );

        if(valid_selection){
            const auto gpu = devices[*selected].as_gpu();
            stage.succeed();
            stage["selected_index"] = *selected;
            stage["name"] = gpu.name;
            stage["vendor_id"] = gpu.vendor_id;
            stage["device_id"] = gpu.device_id;
            stage["discrete"] = gpu.discrete;
        }else if(devices.empty()){
            stage.fail("No Vulkan physical devices found");
        }else if(valid_index && !supports_required_extensions){
            const auto resolution = resolve_device_extensions(
                devices[*selected],
                required_extensions
            );
            stage.fail("Selected physical device is missing required extensions");
            stage["missing_device_extensions"] = alib6::to_adata(
                resolution.missing
            );
        }else{
            stage.fail("Physical device selector returned no valid device");
        }
    }

    if(!valid_selection){
        if(devices.empty()){
            ew.report(
                ave_vk_select_physical_device,
                "No Vulkan physical devices found."
            );
        }else if(valid_index && !supports_required_extensions){
            ew.report(
                ave_vk_select_physical_device,
                "Selected physical device is missing required device extensions."
            );
        }else{
            ew.report(
                ave_vk_select_physical_device,
                "Physical device selector returned no valid device."
            );
        }
        return false;
    }

    r.physical_device = devices[*selected];
    return true;
}

bool RenderProfile::__vk_create_glfw_surface(
    Renderer & r,
    alib6::ErrorWrapper ew,
    RenderBuildReport * result
){
    r.surface = Surface::create(
        r.instance,
        *window,
        CreateSurfaceInfo { .ew = ew }
    );

    if(result){
        auto & stage = (*result)[create_surface];
        if(r.surface){
            stage.succeed();
            stage["surface_handle"] = std::format(
                "{}",
                static_cast<const void*>(r.surface->get_system_handle())
            );
        }else{
            stage.fail("Failed to create Vulkan window surface");
        }
    }

    return r.surface != VK_NULL_HANDLE;
}

bool RenderProfile::__vk_instance(
    Renderer & r,
    alib6::ErrorWrapper ew,
    RenderBuildReport * result
){
    CreateInstanceInfo ci{
        .ctx = ctx,
        .ew = ew
    };
    WithGlobalInput gi (window);
    // glfw需要一些必需扩展，因此这里加入，用户要修改可以自己删了
    ci.extensions = gi.get_required_extensions();
    if(result){
        auto & stage = (*result)[instance_extensions];
        stage["platform"] = alib6::to_adata(ci.extensions);
        stage["has_window"] = (window != nullptr);
    }

    // 注入通知
    if(with_data.on_extensions_resolved){
        with_data.on_extensions_resolved(true, ci.extensions, {});
    }

    // 配置instance
    const bool has_custom_config = static_cast<bool>(with_data.configure_instance);
    if(has_custom_config){
        try{
            with_data.configure_instance(gi, ci);
        }catch(const std::exception& e){
            if(result){
                result->fail(create_instance, e.what());
            }
            throw;
        }catch(...){
            if(result){
                result->fail(create_instance, "Unknown exception in configure_instance");
            }
            throw;
        }
    }

    const bool need_debug_ext = (!with_data.debug_messenger && with_data.configure_debug_messenger.has_value());
    const bool has_req_ext = !with_data.required_extensions.empty();
    const bool has_opt_ext = !with_data.optional_extensions.empty();
    const bool need_dyn_render_prop2 = with_data.try_dynamic_rendering &&
        (ci.api_version.major < 1 || (ci.api_version.major == 1 && ci.api_version.minor < 1));
    const bool should_enumerate_exts = (result != nullptr) || need_debug_ext || has_req_ext || has_opt_ext || need_dyn_render_prop2;

    if(should_enumerate_exts){
        const auto extensions = gi.enumerate_extension_properties();
        auto has_extension = [&](std::string_view sv) {
            return std::ranges::contains(
                extensions,
                sv,
                &ExtensionProperties::name
            );
        };

        bool ext_had_partial = false;
        bool ext_had_failure = false;

        auto resolve_extensions = [&]<std::ranges::input_range Range>(
            Range&& provided_extensions,
            bool optional,
            std::string_view category
        ) {
            std::vector<std::string> missing_extensions;
            std::vector<std::string> available_extensions;

            for (auto&& provided_extension : provided_extensions) {
                const std::string_view name = provided_extension;
                if (!has_extension(name)) {
                    missing_extensions.emplace_back(name);
                } else {
                    available_extensions.emplace_back(name);
                    ci.enable_extension(name);
                }
            }

            if(with_data.on_extensions_resolved){
                with_data.on_extensions_resolved(
                    !optional,
                    available_extensions,
                    missing_extensions
                );
            }

            if(!missing_extensions.empty()){
                if(optional) ext_had_partial = true;
                else ext_had_failure = true;
            }

            if(result){
                auto & cat = (*result)[instance_extensions][category];
                cat["requested"] = alib6::to_adata(provided_extensions);
                cat["satisfied"] = alib6::to_adata(available_extensions);
                cat["missing"] = alib6::to_adata(missing_extensions);
            }

            panicf_debug(
                (!optional) && !missing_extensions.empty(),
                "Cannot find required extensions '{}'. Supported extensions: {}.",
                missing_extensions,
                extensions
            );
        };

        if(need_debug_ext) {
            resolve_extensions(std::array {
                std::string_view { VK_EXT_DEBUG_UTILS_EXTENSION_NAME }
            }, false, "debug_utils");
        }
        if(has_req_ext) {
            resolve_extensions(
                with_data.required_extensions,
                false,
                "required"
            );
        }
        if(has_opt_ext) {
            resolve_extensions(
                with_data.optional_extensions,
                true,
                "optional"
            );
        }
        if(need_dyn_render_prop2 &&
           has_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
            ci.enable_extension(
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
            );
        }

        if(result){
            auto & stage = (*result)[instance_extensions];
            stage["enabled"] = alib6::to_adata(ci.extensions);
            stage["available_system_count"] = extensions.size();
            stage["system_extensions"] = alib6::to_adata(extensions);
            stage["has_window"] = (window != nullptr);
            if(ext_had_failure){
                stage.fail("Required extensions missing");
            }else if(ext_had_partial){
                stage.partial("Some optional extensions unavailable");
            }else{
                stage.succeed();
            }
        }
    }

    // 中间层协商 (instance_layers)
    const bool need_val_layer = with_data.validation_layer;
    const bool has_req_layers = !with_data.required_layers.empty();
    const bool has_opt_layers = !with_data.optional_layers.empty();
    const bool should_enumerate_layers = (result != nullptr) || need_val_layer || has_req_layers || has_opt_layers;

    if(should_enumerate_layers){
        const auto layers = gi.enumerate_layer_properties();
        auto has_layer = [&](std::string_view sv) {
            return std::ranges::contains(
                layers,
                sv,
                &LayerProperties::name
            );
        };

        bool layer_had_partial = false;
        bool layer_had_failure = false;

        auto resolve_layers = [&]<std::ranges::input_range Range>(
            Range&& provided_layers,
            bool optional,
            std::string_view category
        ) {
            std::vector<std::string> missing_layers;
            std::vector<std::string> available_layers;

            for(auto&& provided_layer : provided_layers){
                const std::string_view name = provided_layer;
                if(!has_layer(name)) {
                    missing_layers.emplace_back(name);
                } else {
                    available_layers.emplace_back(name);
                    ci.enable_layer(name);
                }
            }

            if(with_data.on_layers_resolved){
                with_data.on_layers_resolved(
                    !optional,
                    available_layers,
                    missing_layers
                );
            }

            if(!missing_layers.empty()){
                if(optional) layer_had_partial = true;
                else layer_had_failure = true;
            }

            if(result){
                auto & cat = (*result)[instance_layers][category];
                cat["requested"] = alib6::to_adata(provided_layers);
                cat["satisfied"] = alib6::to_adata(available_layers);
                cat["missing"] = alib6::to_adata(missing_layers);
            }

            panicf_debug(
                (!optional) && !missing_layers.empty(),
                "Cannot find required layers '{}'. Supported layers: {}.",
                missing_layers,
                layers
            );
        };

        if(need_val_layer) {
            resolve_layers(
                std::array{ vulkan_validation_layer_name },
                false,
                "validation"
            );
        }
        if(has_req_layers){
            resolve_layers(
                with_data.required_layers,
                false,
                "required"
            );
        }
        if(has_opt_layers){
            resolve_layers(
                with_data.optional_layers,
                true,
                "optional"
            );
        }

        if(result){
            auto & stage = (*result)[instance_layers];
            stage["enabled"] = alib6::to_adata(ci.layers);
            stage["available_system_count"] = layers.size();
            stage["system_layers"] = alib6::to_adata(layers);
            if(layer_had_failure){
                stage.fail("Required layers missing");
            }else if(layer_had_partial){
                stage.partial("Some optional layers unavailable");
            }else if(!need_val_layer && !has_req_layers && !has_opt_layers){
                stage.skip("No validation or extra layers requested");
            }else{
                stage.succeed();
            }
        }
    }

    // 真正创建 Instance (create_instance)
    r.instance = std::make_shared<Instance>();
    const bool created = r.instance->create(ci);
    if(result){
        auto & stage = (*result)[create_instance];
        if(created){
            stage.succeed();
            stage["source"] = "created";
            stage["custom_configured"] = has_custom_config;
            stage["application_name"] = ci.application_name;
            stage["application_version"] = std::format("{}.{}.{}", ci.application_version.major, ci.application_version.minor, ci.application_version.patch);
            stage["engine_name"] = ci.engine_name;
            stage["engine_version"] = std::format("{}.{}.{}", ci.engine_version.major, ci.engine_version.minor, ci.engine_version.patch);
            stage["api_version"] = std::format("{}.{}.{}", ci.api_version.major, ci.api_version.minor, ci.api_version.patch);
            stage["enabled_extensions"] = alib6::to_adata(ci.extensions);
            stage["enabled_layers"] = alib6::to_adata(ci.layers);
            stage["instance_handle"] = std::format("{}", static_cast<const void*>(r.instance.get()));
        }else{
            stage.fail("Failed to create Vulkan instance");
            stage["source"] = "failed";
        }
    }

    return created;
}

bool RenderProfile::recreate_swapchain_from_window(
    Context & ctx,
    Renderer & r,
    Window & window,
    RenderBuildReport * report,
    ProfileWith user_with,
    alib6::ErrorWrapper ew
){
    if(!r.device || r.device->get_system_handle() == VK_NULL_HANDLE){
        ew.report(ave_vk_create_swapchain, "Cannot recreate swapchain: Renderer has no valid Device.");
        return false;
    }
    if(!r.surface || r.surface->get_system_handle() == VK_NULL_HANDLE){
        ew.report(ave_vk_create_swapchain, "Cannot recreate swapchain: Renderer has no valid Surface.");
        return false;
    }
    const auto [width, height] = window.get_framebuffer_size();
    if(width <= 0 || height <= 0){
        return false;
    }
    if(!r.invalidate_graphics_cache()){
        ew.report(ave_vk_create_swapchain,
            "Cannot recreate swapchain while a GraphicsContext is active.");
        return false;
    }

    const VkResult idle_code = vkDeviceWaitIdle(r.device->get_system_handle());
    if(idle_code != VK_SUCCESS){
        ew.report(ave_vk_create_swapchain,
            "Failed waiting for the Device before swapchain recreation ({}).",
            static_cast<int>(idle_code));
        return false;
    }

    ProfileWith pw = std::move(user_with);
    pw.instance        = r.instance;
    pw.debug_messenger = r.debug_messenger;
    pw.surface         = r.surface;
    pw.device          = r.device;
    // Command buffers and synchronization objects depend on the swapchain image
    // count. Recreate them directly so a capability/image-count change cannot
    // leave Renderer with a stale cache or mismatched synchronizations.
    if(!pw.command_pool) pw.command_pool = r.command_pool;
    pw.swapchain       = nullptr;
    pw.sync_objects    = nullptr;
    pw.command_buffers = nullptr;

    auto original_configure_swapchain = pw.configure_swapchain;
    pw.configure_swapchain = [old_sc = r.swapchain, original_configure_swapchain](
        WithSwapchain& with_sw, CreateSwapchainInfo& ci
    ){
        ci.old_swapchain = old_sc;
        if(original_configure_swapchain){
            original_configure_swapchain(with_sw, ci);
        }else{
            default_configure_swapchain(with_sw, ci);
        }
    };

    auto profile = RenderProfile::from_window(ctx, window);
    profile.with(std::move(pw));
    if(report){
        profile.with_result(*report);
    }

    Renderer new_r = profile.build(ew);
    if(!new_r.swapchain || !new_r.sync_objects || !new_r.command_buffers){
        return false;
    }

    r.context = new_r.context;
    r.swapchain = std::move(new_r.swapchain);
    r.images = std::move(new_r.images);
    if(new_r.dynamic_render) r.dynamic_render = std::move(new_r.dynamic_render);
    if(new_r.legacy_render) r.legacy_render = std::move(new_r.legacy_render);
    r.sync_objects = std::move(new_r.sync_objects);
    r.command_pool = std::move(new_r.command_pool);
    r.command_buffers = std::move(new_r.command_buffers);
    r.default_clear_values = std::move(new_r.default_clear_values);

    return true;
}

bool RenderProfile::recreate_swapchain_from_window(
    Renderer & r,
    Window & window,
    RenderBuildReport * report,
    ProfileWith user_with,
    alib6::ErrorWrapper ew
){
    if(!r.device || r.device->get_system_handle() == VK_NULL_HANDLE){
        ew.report(ave_vk_create_swapchain, "Cannot recreate swapchain: Renderer has no valid Device.");
        return false;
    }
    if(!r.surface || r.surface->get_system_handle() == VK_NULL_HANDLE){
        ew.report(ave_vk_create_swapchain, "Cannot recreate swapchain: Renderer has no valid Surface.");
        return false;
    }
    Context* ctx = r.context;
    if(!ctx && r.instance){
        ctx = r.instance->get_context();
    }
    if(!ctx){
        ew.report(ave_vk_create_swapchain, "Cannot recreate swapchain: Renderer has no valid Context.");
        return false;
    }
    return recreate_swapchain_from_window(*ctx, r, window, report, std::move(user_with), ew);
}
