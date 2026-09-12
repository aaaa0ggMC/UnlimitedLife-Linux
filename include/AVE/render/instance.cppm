/**
 * @file instance.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Vulkan实例
 * @version 5.0
 * @date 2026-09-10
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>
#include <alib6/debug.h>

export module ave.render:instance;

import std;
import alib6;
import ave.context;
import ave.ecode;
import ave.render.base;

namespace ave::detail{
    auto get_c_strings(std::span<const std::string> view) -> std::vector<const char*> {
        return view
            | std::views::transform([](const std::string& str) {
                return str.c_str();
            })
            | std::ranges::to<std::vector>();
    }

    // 去重复的加入
    inline bool enable_target(
        std::vector<std::string>& target,
        std::string_view name
    ) {
        if(std::ranges::contains(target, name)) return false;
        target.emplace_back(name);

        return true;
    }

    inline alib6::u32 enable_target(
        std::vector<std::string>& target,
        std::span<const std::string_view> names
    ) {
        alib6::u32 enabled = 0;

        for(const auto name : names){
            enabled += enable_target(target, name);
        }

        return enabled;
    }

    inline alib6::u32 disable_target(
        std::vector<std::string>& target,
        std::string_view name
    ) {
        return static_cast<alib6::u32>(
            std::erase_if(
                target,
                [name](const std::string& value) {
                    return value == name;
                }
            )
        );
    }

    inline alib6::u32 disable_target(
        std::vector<std::string>& target,
        std::span<const std::string_view> names
    ) {
        alib6::u32 disabled = 0;

        for(const auto name : names){
            disabled += disable_target(target, name);
        }

        return disabled;
    }
}

export namespace ave{

    struct AVE_API CreateInstanceInfo{
        Context & ctx;
        
        // 基础内容
        std::string application_name = "AVE";
        Version application_version = {0,1,0};
        std::string engine_name = "No Engine";
        Version engine_version = {1, 0 ,0};
        ApiVersion api_version = ave_vk_1_0;
        // 扩展
        std::vector<std::string> extensions;
        // 层级
        std::vector<std::string> layers;
        mutable alib6::ErrorWrapper ew = {};

        inline bool enable_extension(std::string_view sv){ return detail::enable_target(extensions, sv); }
        inline bool enable_layer(std::string_view sv){ return detail::enable_target(layers, sv); }
        inline alib6::u32 enable_extensions(std::span<const std::string_view> svs){ return detail::enable_target(extensions, svs); }
        inline alib6::u32 enable_layers(std::span<const std::string_view> svs){ return detail::enable_target(layers, svs); }
        inline alib6::u32 disable_extension(std::string_view sv){ return detail::disable_target(extensions, sv); }
        inline alib6::u32 disable_layer(std::string_view sv){ return detail::disable_target(layers, sv); }
        inline alib6::u32 disable_extensions(std::span<const std::string_view> svs){ return detail::disable_target(extensions, svs); }
        inline alib6::u32 disable_layers(std::span<const std::string_view> svs){ return detail::disable_target(layers, svs); }
    };

    struct AVE_API Instance {
    private:
        VkInstance instance { nullptr };
        Context * ctx { nullptr };
        ApiVersion api_version { ave_vk_1_0 };

    public:
        Instance() = default;
        Instance(const CreateInstanceInfo & ci){ create(ci); }
        ~Instance(){ destroy(); }

        // move
        Instance(Instance && i) noexcept
        :instance(i.instance),ctx(i.ctx),api_version(i.api_version){
            i.instance = VK_NULL_HANDLE;
            i.ctx = nullptr;
            i.api_version = ave_vk_1_0;
        }
        Instance& operator=(Instance && i) noexcept {
            if(&i == this) return *this;
            panic_debug(instance != VK_NULL_HANDLE, "Cannot move instance to an object that has an instance already.");
            if(instance != VK_NULL_HANDLE) [[unlikely]] {
                destroy();
            }
            
            instance = i.instance;
            ctx = i.ctx;
            api_version = i.api_version;
            i.instance = VK_NULL_HANDLE;
            i.ctx = nullptr;
            i.api_version = ave_vk_1_0;
            return *this;
        }
        Instance& operator=(const Instance &) = delete;
        Instance(const Instance &) = delete;

        bool create(const CreateInstanceInfo & ci){
            if(instance){
                ci.ew.report(
                    ave_already_created,
                    "Error: instance has been created already."
                );
                return false;
            }
            
            ctx = &(ci.ctx);

            VkApplicationInfo appInfo{};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = ci.application_name.c_str();
            appInfo.applicationVersion = ci.application_version.to();
            appInfo.pEngineName = ci.engine_name.c_str();
            appInfo.engineVersion = ci.engine_version.to();
            appInfo.apiVersion = ci.api_version.to();
            
            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &appInfo;
            
            // 扩展支持
            auto extensions = detail::get_c_strings(ci.extensions);
            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();

            // 中间层支持
            auto layers = detail::get_c_strings(ci.layers);
            createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
            createInfo.ppEnabledLayerNames = layers.data();

            if(auto code = vkCreateInstance(&createInfo, ctx->get_vk_allocator(), &instance); code != VK_SUCCESS){
                ci.ew.report(
                    ave_vk_create_instance,
                    "Fatal Error: failed to create vkInstance({}).",
                    (int)code
                );
                return false;
            }
            api_version = ci.api_version;
            return true;
        }

        inline void destroy() noexcept {
            if(!instance) return;
            vkDestroyInstance(instance, ctx->get_vk_allocator());
            instance = VK_NULL_HANDLE;
            ctx = nullptr;
            api_version = ave_vk_1_0;
        }

        VkInstance get_system_handle() const noexcept { return instance; }
        auto get_vk_allocator() const noexcept {
            return ctx ? ctx->get_vk_allocator() : nullptr;
        }
        [[nodiscard]] ApiVersion get_api_version() const noexcept {
            return api_version;
        }
    };
}
