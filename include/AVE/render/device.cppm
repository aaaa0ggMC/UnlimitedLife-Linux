/**
 * @file device.cppm
 * @brief Vulkan logical device RAII wrapper
 * @version 5.0
 * @date 2026-09-11
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.render:device;

import std;
import alib6;
import ave.ecode;
import :instance;

export namespace ave {

    /// 一个队列族的创建请求；priorities 的下标就是该族中的 queue index。
    struct AVE_API DeviceQueueRequest {
        alib6::u32 family_index { 0 };
        std::vector<float> priorities { 1.0f };
    };

    struct AVE_API CreateDeviceInfo {
        std::shared_ptr<Instance> instance;
        VkPhysicalDevice physical_device { VK_NULL_HANDLE };
        const void* next { nullptr };
        bool enable_dynamic_rendering { false };
        std::vector<DeviceQueueRequest> queues;
        std::vector<std::string> extensions;
        VkPhysicalDeviceFeatures features {};
        mutable alib6::ErrorWrapper ew {};

        /// 请求一个队列族。重复请求同一族时追加一个 queue。
        void request_queue(alib6::u32 family_index, float priority = 1.0f) {
            const auto it = std::ranges::find(
                queues, family_index, &DeviceQueueRequest::family_index
            );
            if(it == queues.end()) {
                queues.push_back({ family_index, { priority } });
            }else{
                it->priorities.push_back(priority);
            }
        }

        bool enable_extension(std::string_view name) {
            if(std::ranges::contains(extensions, name)) return false;
            extensions.emplace_back(name);
            return true;
        }
    };

    class AVE_API Device final {
    private:
        std::shared_ptr<Instance> instance;
        VkPhysicalDevice physical_device { VK_NULL_HANDLE };
        VkDevice device { VK_NULL_HANDLE };
        std::vector<DeviceQueueRequest> queues;
        std::vector<std::string> extensions;
        bool dynamic_rendering { false };
        PFN_vkCmdBeginRendering pfn_cmd_begin_rendering { nullptr };
        PFN_vkCmdEndRendering pfn_cmd_end_rendering { nullptr };

        Device() = default;

        bool initialize(CreateDeviceInfo ci) {
            if(!ci.instance ||
               ci.instance->get_system_handle() == VK_NULL_HANDLE ||
               ci.physical_device == VK_NULL_HANDLE) {
                ci.ew.report(
                    ave_vk_create_device,
                    "Cannot create a Vulkan device without a valid instance and physical device."
                );
                return false;
            }
            if(ci.queues.empty()) {
                ci.ew.report(
                    ave_vk_create_device,
                    "Cannot create a Vulkan device without at least one queue request."
                );
                return false;
            }

            alib6::u32 family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(
                ci.physical_device, &family_count, nullptr
            );
            std::vector<VkQueueFamilyProperties> family_properties(family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(
                ci.physical_device,
                &family_count,
                family_properties.data()
            );

            for(const auto& request : ci.queues) {
                const bool valid =
                    request.family_index < family_count &&
                    !request.priorities.empty() &&
                    request.priorities.size() <=
                        family_properties[request.family_index].queueCount &&
                    std::ranges::all_of(request.priorities, [](float priority) {
                        return priority >= 0.0f && priority <= 1.0f;
                    });
                if(!valid) {
                    ci.ew.report(
                        ave_vk_create_device,
                        "Invalid queue request for family {}.",
                        request.family_index
                    );
                    return false;
                }
            }

            std::vector<VkDeviceQueueCreateInfo> queue_infos;
            queue_infos.reserve(ci.queues.size());
            for(const auto& request : ci.queues) {
                VkDeviceQueueCreateInfo queue_info {};
                queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queue_info.queueFamilyIndex = request.family_index;
                queue_info.queueCount = static_cast<alib6::u32>(
                    request.priorities.size()
                );
                queue_info.pQueuePriorities = request.priorities.data();
                queue_infos.push_back(queue_info);
            }

            const auto extension_names = ci.extensions
                | std::views::transform([](const std::string& name) {
                    return name.c_str();
                })
                | std::ranges::to<std::vector>();

            VkDeviceCreateInfo create_info {};
            create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            create_info.pNext = ci.next;
            create_info.queueCreateInfoCount = static_cast<alib6::u32>(
                queue_infos.size()
            );
            create_info.pQueueCreateInfos = queue_infos.data();
            create_info.enabledExtensionCount = static_cast<alib6::u32>(
                extension_names.size()
            );
            create_info.ppEnabledExtensionNames = extension_names.data();
            create_info.pEnabledFeatures = &ci.features;

            const VkResult code = vkCreateDevice(
                ci.physical_device,
                &create_info,
                ci.instance->get_vk_allocator(),
                &device
            );
            if(code != VK_SUCCESS) {
                ci.ew.report(
                    ave_vk_create_device,
                    "Failed to create Vulkan logical device ({}).",
                    static_cast<int>(code)
                );
                return false;
            }

            if(ci.enable_dynamic_rendering) {
                pfn_cmd_begin_rendering = reinterpret_cast<PFN_vkCmdBeginRendering>(
                    vkGetDeviceProcAddr(device, "vkCmdBeginRendering")
                );
                if(!pfn_cmd_begin_rendering) {
                    pfn_cmd_begin_rendering = reinterpret_cast<PFN_vkCmdBeginRendering>(
                        vkGetDeviceProcAddr(device, "vkCmdBeginRenderingKHR")
                    );
                }
                pfn_cmd_end_rendering = reinterpret_cast<PFN_vkCmdEndRendering>(
                    vkGetDeviceProcAddr(device, "vkCmdEndRendering")
                );
                if(!pfn_cmd_end_rendering) {
                    pfn_cmd_end_rendering = reinterpret_cast<PFN_vkCmdEndRendering>(
                        vkGetDeviceProcAddr(device, "vkCmdEndRenderingKHR")
                    );
                }
                dynamic_rendering = (pfn_cmd_begin_rendering != nullptr &&
                                     pfn_cmd_end_rendering != nullptr);
            } else {
                dynamic_rendering = false;
                pfn_cmd_begin_rendering = nullptr;
                pfn_cmd_end_rendering = nullptr;
            }

            instance = std::move(ci.instance);
            physical_device = ci.physical_device;
            queues = std::move(ci.queues);
            extensions = std::move(ci.extensions);
            return true;
        }

    public:
        ~Device() { destroy(); }

        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;
        Device(Device&&) = delete;
        Device& operator=(Device&&) = delete;

        [[nodiscard]] static std::shared_ptr<Device> create(CreateDeviceInfo ci) {
            auto result = std::shared_ptr<Device>(new Device());
            if(!result->initialize(std::move(ci))) return {};
            return result;
        }

        void destroy() noexcept {
            if(device != VK_NULL_HANDLE) {
                vkDeviceWaitIdle(device);
                vkDestroyDevice(device, instance->get_vk_allocator());
            }
            device = VK_NULL_HANDLE;
            physical_device = VK_NULL_HANDLE;
            queues.clear();
            extensions.clear();
            dynamic_rendering = false;
            pfn_cmd_begin_rendering = nullptr;
            pfn_cmd_end_rendering = nullptr;
            instance.reset();
        }

        [[nodiscard]] VkDevice get_system_handle() const noexcept {
            return device;
        }

        [[nodiscard]] VkPhysicalDevice get_physical_device() const noexcept {
            return physical_device;
        }

        [[nodiscard]] const std::shared_ptr<Instance>& get_instance() const noexcept {
            return instance;
        }

        [[nodiscard]] const std::vector<DeviceQueueRequest>&
        get_queue_requests() const noexcept {
            return queues;
        }

        [[nodiscard]] const std::vector<std::string>&
        get_enabled_extensions() const noexcept {
            return extensions;
        }

        [[nodiscard]] bool extension_enabled(std::string_view name) const noexcept {
            return std::ranges::contains(extensions, name);
        }

        [[nodiscard]] bool has_queue(
            alib6::u32 family_index,
            alib6::u32 queue_index = 0
        ) const noexcept {
            const auto it = std::ranges::find(
                queues, family_index, &DeviceQueueRequest::family_index
            );
            return it != queues.end() && queue_index < it->priorities.size();
        }

        [[nodiscard]] VkQueue get_queue(
            alib6::u32 family_index,
            alib6::u32 queue_index = 0
        ) const noexcept {
            if(device == VK_NULL_HANDLE || !has_queue(family_index, queue_index)) {
                return VK_NULL_HANDLE;
            }
            VkQueue result = VK_NULL_HANDLE;
            vkGetDeviceQueue(device, family_index, queue_index, &result);
            return result;
        }

        [[nodiscard]] bool supports_dynamic_rendering() const noexcept {
            return dynamic_rendering;
        }

        [[nodiscard]] PFN_vkCmdBeginRendering get_cmd_begin_rendering() const noexcept {
            return pfn_cmd_begin_rendering;
        }

        [[nodiscard]] PFN_vkCmdEndRendering get_cmd_end_rendering() const noexcept {
            return pfn_cmd_end_rendering;
        }

        void cmd_begin_rendering(
            VkCommandBuffer command_buffer,
            const VkRenderingInfo* rendering_info
        ) const noexcept {
            if(pfn_cmd_begin_rendering) {
                pfn_cmd_begin_rendering(command_buffer, rendering_info);
            }
        }

        void cmd_end_rendering(
            VkCommandBuffer command_buffer
        ) const noexcept {
            if(pfn_cmd_end_rendering) {
                pfn_cmd_end_rendering(command_buffer);
            }
        }
    };
}
