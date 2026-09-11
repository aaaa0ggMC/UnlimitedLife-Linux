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

    /// 如果存在window, 创建surface
    if(window){
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

    return renderer;
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
    const bool should_enumerate_exts = (result != nullptr) || need_debug_ext || has_req_ext || has_opt_ext;

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
