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
import ave.context;
import ave.ecode;
import :base;

namespace ave::detail{
    auto get_c_span(std::span<const std::string> view) -> std::vector<const char*> {
        return view
            | std::views::transform([](const std::string& str) {
                return str.c_str();
            })
            | std::ranges::to<std::vector>();
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
    
        mutable alib6::ErrorWrapper ew = {};
    };

    struct AVE_API Instance {
    private:
        VkInstance instance { nullptr };
        Context * ctx { nullptr };

    public:
        Instance() = default;
        Instance(const CreateInstanceInfo & ci){ create(ci); }
        ~Instance(){ destroy(); }

        // move
        Instance(Instance && i)
        :instance(i.instance),ctx(i.ctx){
            i.instance = VK_NULL_HANDLE;
        }
        void operator=(Instance && i){
            if(&i == this)return;
            panic_debug(instance != VK_NULL_HANDLE, "Cannot move instance to an object that has an instance already.");
            if(instance != VK_NULL_HANDLE) [[unlikely]] {
                destroy();
            }
            
            instance = i.instance;
            ctx = i.ctx;
            i.instance = nullptr;
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
            auto extensions = detail::get_c_span(ci.extensions);
            createInfo.enabledExtensionCount = extensions.size();
            createInfo.ppEnabledExtensionNames = extensions.data();

            // 中间层
            createInfo.enabledLayerCount = 0;

            if(auto code = vkCreateInstance(&createInfo, ctx->get_vk_allocator(), &instance); code != VK_SUCCESS){
                ci.ew.report(
                    ave_vk_create_instance,
                    "Fatal Error: failed to create vkInstance({}).",
                    (int)code
                );
                return false;
            }
            return true;
        }

        inline void destroy() noexcept {
            if(!instance) return;
            vkDestroyInstance(instance, ctx->get_vk_allocator());
            instance = nullptr;
        }
    };
}

