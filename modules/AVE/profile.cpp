module;
#include <alib6/debug.h>
#include <vulkan/vulkan.h>

module ave.profile;

using namespace ave;
using enum RenderBuildStageId;
using enum RenderBuildStageStatus;

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
    /// 创建好instance
    if(!with_data.instance){
        // 用户可以自己销毁这个 instance,如果不需要的话
        if(!__vk_instance(renderer, ew, result)){
            return renderer;
        }
    }else{
        renderer.instance = with_data.instance;
        if(result){
            result->skip({
                instance_extensions,
                instance_layers
            }, "Instance was externally provided");

            auto & stage = result->succeed(create_instance);
            stage["source"] = "provided";
            stage["instance_handle"] = std::format("{}", static_cast<const void*>(renderer.instance.get()));
        }
    }

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
        renderer.debug_messenger = with_data.debug_messenger;
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

    return renderer;
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

    // 1. 扩展协商 (instance_extensions)
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

    // 2. 中间层协商 (instance_layers)
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

    // 3. 真正创建 Instance (create_instance)
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
