/**
 * @file debug_messenger.cppm
 * @brief Vulkan debug utils messenger
 * @version 5.0
 * @date 2026-09-11
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>
#include <cstdio>

export module ave.render:debug_messenger;

import std;
import alib6;
import ave.ecode;
import :instance;

export namespace ave {
    // 返回 true 会要求 Vulkan 中止触发该消息的调用；通常应返回 false。
    using DebugMessageCallback = std::function<bool(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT& data
    )>;

    struct AVE_API CreateDebugMessengerInfo {
        VkDebugUtilsMessageSeverityFlagsEXT severity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        VkDebugUtilsMessageTypeFlagsEXT type =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        DebugMessageCallback on_message;
        mutable alib6::ErrorWrapper ew = {};
    };

    class AVE_API DebugMessenger final {
    private:
        // 保证 messenger 一定先于其依赖的 instance 销毁。
        std::shared_ptr<Instance> instance;
        VkDebugUtilsMessengerEXT messenger { VK_NULL_HANDLE };
        DebugMessageCallback on_message;

        DebugMessenger() = default;

        static const char* severity_name(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity
        ) noexcept {
            if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) return "error";
            if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) return "warning";
            if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) return "info";
            return "verbose";
        }

        static VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_callback(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT type,
            const VkDebugUtilsMessengerCallbackDataEXT* data,
            void* user_data
        ) noexcept {
            auto* self = static_cast<DebugMessenger*>(user_data);
            if(!self || !data) return VK_FALSE;

            try {
                if(self->on_message) {
                    return self->on_message(severity, type, *data)
                        ? VK_TRUE
                        : VK_FALSE;
                }

                std::fprintf(
                    stderr,
                    "[Vulkan][%s][type=0x%x] %s\n",
                    severity_name(severity),
                    static_cast<unsigned int>(type),
                    data->pMessage ? data->pMessage : "<no message>"
                );
            } catch(...) {
                // C++ 异常不能越过 Vulkan 的 C ABI 回调边界。
                std::fputs("[Vulkan] Debug message callback threw an exception.\n", stderr);
            }

            return VK_FALSE;
        }

        bool initialize(
            std::shared_ptr<Instance> target_instance,
            const CreateDebugMessengerInfo& ci
        ) {
            if(!target_instance || target_instance->get_system_handle() == VK_NULL_HANDLE) {
                ci.ew.report(
                    ave_vk_create_debug_messenger,
                    "Cannot create a Vulkan debug messenger without a valid instance."
                );
                return false;
            }

            instance = std::move(target_instance);
            on_message = ci.on_message;

            const auto create_fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(
                    instance->get_system_handle(),
                    "vkCreateDebugUtilsMessengerEXT"
                )
            );
            if(!create_fn) {
                ci.ew.report(
                    ave_vk_create_debug_messenger,
                    "Cannot load vkCreateDebugUtilsMessengerEXT. Ensure '{}' is enabled.",
                    VK_EXT_DEBUG_UTILS_EXTENSION_NAME
                );
                instance.reset();
                on_message = {};
                return false;
            }

            VkDebugUtilsMessengerCreateInfoEXT create_info {};
            create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            create_info.messageSeverity = ci.severity;
            create_info.messageType = ci.type;
            create_info.pfnUserCallback = vulkan_callback;
            create_info.pUserData = this;

            const VkResult code = create_fn(
                instance->get_system_handle(),
                &create_info,
                instance->get_vk_allocator(),
                &messenger
            );
            if(code != VK_SUCCESS) {
                ci.ew.report(
                    ave_vk_create_debug_messenger,
                    "Failed to create Vulkan debug messenger ({}).",
                    static_cast<int>(code)
                );
                instance.reset();
                on_message = {};
                return false;
            }

            return true;
        }

    public:
        ~DebugMessenger() { destroy(); }

        DebugMessenger(const DebugMessenger&) = delete;
        DebugMessenger& operator=(const DebugMessenger&) = delete;
        DebugMessenger(DebugMessenger&&) = delete;
        DebugMessenger& operator=(DebugMessenger&&) = delete;

        [[nodiscard]] static std::shared_ptr<DebugMessenger> create(
            std::shared_ptr<Instance> instance,
            const CreateDebugMessengerInfo& ci = {}
        ) {
            auto result = std::shared_ptr<DebugMessenger>(new DebugMessenger());
            if(!result->initialize(std::move(instance), ci)) return {};
            return result;
        }

        void destroy() noexcept {
            if(messenger != VK_NULL_HANDLE && instance) {
                const auto destroy_fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(
                        instance->get_system_handle(),
                        "vkDestroyDebugUtilsMessengerEXT"
                    )
                );
                if(destroy_fn) {
                    destroy_fn(
                        instance->get_system_handle(),
                        messenger,
                        instance->get_vk_allocator()
                    );
                }
            }

            messenger = VK_NULL_HANDLE;
            on_message = {};
            instance.reset();
        }

        [[nodiscard]] VkDebugUtilsMessengerEXT get_system_handle() const noexcept {
            return messenger;
        }

        [[nodiscard]] const std::shared_ptr<Instance>& get_instance() const noexcept {
            return instance;
        }
    };
}
