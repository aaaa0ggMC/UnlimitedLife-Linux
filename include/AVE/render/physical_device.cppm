/**
 * @file physical_device.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Vulkan 物理设备原始信息及常用 GPU 信息转换
 * @version 5.0
 * @date 2026-09-11
 *
 * @copyright Copyright (c) 2026
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.render:physical_device;

import std;
import alib6;
import ave.render.base;

export namespace ave {

    enum class GPUType {
        Other,
        Integrated,
        Discrete,
        Virtual,
        CPU
    };

    /// 由原始 Vulkan 信息归纳出的常用设备信息。
    struct AVE_API GPUInfo {
        std::string name;
        alib6::u32 vendor_id { 0 };
        alib6::u32 device_id { 0 };
        alib6::u32 driver_version { 0 };
        ApiVersion api_version {};
        GPUType type { GPUType::Other };
        bool discrete { false };

        /// 内存大小，单位为字节。
        alib6::u64 total_memory { 0 };
        alib6::u64 device_local_memory { 0 };
        alib6::u64 host_visible_memory { 0 };

        bool support_graphics { false };
        bool support_compute { false };
        bool support_transfer { false };
        bool surface_support_present { false };

        std::optional<alib6::u32> graphics_queue_family;
        std::optional<alib6::u32> compute_queue_family;
        std::optional<alib6::u32> transfer_queue_family;
        std::optional<alib6::u32> present_queue_family;

        bool support_swapchain { false };
        bool swapchain_adequate { false };
        bool geometry_shader { false };
        bool tessellation_shader { false };
        bool sampler_anisotropy { false };
        bool multi_draw_indirect { false };
        bool fill_mode_non_solid { false };
        bool wide_lines { false };
        bool shader_int64 { false };
        bool shader_float64 { false };

        alib6::u32 max_image_dimension_2d { 0 };
        alib6::u32 max_push_constants_size { 0 };
        alib6::u32 max_bound_descriptor_sets { 0 };
        alib6::u32 max_color_attachments { 0 };
        alib6::u32 max_compute_work_group_invocations { 0 };
        float max_sampler_anisotropy { 0.0f };
        float timestamp_period { 0.0f };
    };

    /// 单个 Vulkan 物理设备及其在当前查询上下文中的信息。
    struct AVE_API PhysicalDeviceInfo {
        struct DeviceXSurfaceInfo {
            bool support_present { false };
        };

        VkPhysicalDevice device { VK_NULL_HANDLE };
        VkPhysicalDeviceProperties properties {};
        VkPhysicalDeviceFeatures features {};
        VkPhysicalDeviceMemoryProperties memory_properties {};
        std::vector<VkQueueFamilyProperties> queue_family_properties;
        std::vector<VkExtensionProperties> extension_properties;

        /// 未提供 Surface 时为 nullopt；否则与 queue_family_properties 等长。
        std::optional<std::vector<DeviceXSurfaceInfo>>
            queue_family_x_surface_info;
        std::optional<VkSurfaceCapabilitiesKHR> surface_capabilities;
        std::vector<VkSurfaceFormatKHR> surface_formats;
        std::vector<VkPresentModeKHR> present_modes;

        /// 查询一个物理设备的完整原始信息。Surface 为空时跳过所有呈现相关查询。
        [[nodiscard]] static PhysicalDeviceInfo query(
            VkPhysicalDevice target,
            VkSurfaceKHR surface = VK_NULL_HANDLE
        ) {
            PhysicalDeviceInfo info;
            info.device = target;
            if(target == VK_NULL_HANDLE) return info;

            vkGetPhysicalDeviceProperties(target, &info.properties);
            vkGetPhysicalDeviceFeatures(target, &info.features);
            vkGetPhysicalDeviceMemoryProperties(target, &info.memory_properties);

            alib6::u32 extension_count = 0;
            VkResult extension_result = VK_SUCCESS;
            do {
                extension_result = vkEnumerateDeviceExtensionProperties(
                    target, nullptr, &extension_count, nullptr
                );
                if(extension_result != VK_SUCCESS) break;
                info.extension_properties.resize(extension_count);
                extension_result = vkEnumerateDeviceExtensionProperties(
                    target,
                    nullptr,
                    &extension_count,
                    info.extension_properties.data()
                );
            } while(extension_result == VK_INCOMPLETE);
            if(extension_result == VK_SUCCESS) {
                info.extension_properties.resize(extension_count);
            }else{
                info.extension_properties.clear();
            }

            alib6::u32 queue_family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(
                target, &queue_family_count, nullptr
            );
            info.queue_family_properties.resize(queue_family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(
                target,
                &queue_family_count,
                info.queue_family_properties.data()
            );
            info.queue_family_properties.resize(queue_family_count);

            if(surface == VK_NULL_HANDLE) return info;

            info.queue_family_x_surface_info.emplace(queue_family_count);
            for(alib6::u32 i = 0; i < queue_family_count; ++i) {
                VkBool32 supported = VK_FALSE;
                if(vkGetPhysicalDeviceSurfaceSupportKHR(
                    target, i, surface, &supported
                ) == VK_SUCCESS) {
                    (*info.queue_family_x_surface_info)[i].support_present =
                        supported == VK_TRUE;
                }
            }

            VkSurfaceCapabilitiesKHR capabilities {};
            if(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                target, surface, &capabilities
            ) == VK_SUCCESS) {
                info.surface_capabilities = capabilities;
            }

            alib6::u32 format_count = 0;
            VkResult format_result = VK_SUCCESS;
            do {
                format_result = vkGetPhysicalDeviceSurfaceFormatsKHR(
                    target, surface, &format_count, nullptr
                );
                if(format_result != VK_SUCCESS) break;
                info.surface_formats.resize(format_count);
                format_result = vkGetPhysicalDeviceSurfaceFormatsKHR(
                    target,
                    surface,
                    &format_count,
                    info.surface_formats.data()
                );
            } while(format_result == VK_INCOMPLETE);
            if(format_result == VK_SUCCESS) {
                info.surface_formats.resize(format_count);
            }else{
                info.surface_formats.clear();
            }

            alib6::u32 present_mode_count = 0;
            VkResult present_mode_result = VK_SUCCESS;
            do {
                present_mode_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
                    target, surface, &present_mode_count, nullptr
                );
                if(present_mode_result != VK_SUCCESS) break;
                info.present_modes.resize(present_mode_count);
                present_mode_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
                    target,
                    surface,
                    &present_mode_count,
                    info.present_modes.data()
                );
            } while(present_mode_result == VK_INCOMPLETE);
            if(present_mode_result == VK_SUCCESS) {
                info.present_modes.resize(present_mode_count);
            }else{
                info.present_modes.clear();
            }

            return info;
        }

        [[nodiscard]] bool has_surface() const noexcept {
            return queue_family_x_surface_info.has_value();
        }

        [[nodiscard]] bool supports_extension(std::string_view name) const noexcept {
            return std::ranges::any_of(
                extension_properties,
                [name](const VkExtensionProperties& extension) {
                    return std::string_view(extension.extensionName) == name;
                }
            );
        }

        [[nodiscard]] GPUInfo as_gpu() const {
            GPUInfo result;

            result.name = properties.deviceName;
            result.vendor_id = properties.vendorID;
            result.device_id = properties.deviceID;
            result.driver_version = properties.driverVersion;
            result.api_version = properties.apiVersion;

            switch(properties.deviceType) {
                case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                    result.type = GPUType::Integrated;
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                    result.type = GPUType::Discrete;
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                    result.type = GPUType::Virtual;
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_CPU:
                    result.type = GPUType::CPU;
                    break;
                default:
                    result.type = GPUType::Other;
                    break;
            }
            result.discrete = result.type == GPUType::Discrete;

            std::array<bool, VK_MAX_MEMORY_HEAPS> host_visible_heaps {};
            for(alib6::u32 i = 0; i < memory_properties.memoryTypeCount; ++i) {
                const auto& memory_type = memory_properties.memoryTypes[i];
                if(memory_type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
                    host_visible_heaps[memory_type.heapIndex] = true;
                }
            }

            for(alib6::u32 i = 0; i < memory_properties.memoryHeapCount; ++i) {
                const auto& heap = memory_properties.memoryHeaps[i];
                result.total_memory += heap.size;
                if(heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                    result.device_local_memory += heap.size;
                }
                if(host_visible_heaps[i]) {
                    result.host_visible_memory += heap.size;
                }
            }

            for(alib6::u32 i = 0; i < queue_family_properties.size(); ++i) {
                const auto& queue_family = queue_family_properties[i];
                if(queue_family.queueCount == 0) continue;

                if(queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    result.support_graphics = true;
                    if(!result.graphics_queue_family) {
                        result.graphics_queue_family = i;
                    }
                }

                if(queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                    result.support_compute = true;
                    if(!result.compute_queue_family) {
                        result.compute_queue_family = i;
                    }
                }

                if(queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                    result.support_transfer = true;
                    if(!result.transfer_queue_family) {
                        result.transfer_queue_family = i;
                    }
                }

                if(queue_family_x_surface_info &&
                   i < queue_family_x_surface_info->size() &&
                   (*queue_family_x_surface_info)[i].support_present) {
                    result.surface_support_present = true;
                    if(!result.present_queue_family) {
                        result.present_queue_family = i;
                    }
                }
            }

            // 与 LearnVulkan2 一致：存在同时支持 graphics/present 的队列族时优先共用。
            if(queue_family_x_surface_info) {
                for(alib6::u32 i = 0; i < queue_family_properties.size(); ++i) {
                    if(queue_family_properties[i].queueCount > 0 &&
                       (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                       i < queue_family_x_surface_info->size() &&
                       (*queue_family_x_surface_info)[i].support_present) {
                        result.graphics_queue_family = i;
                        result.present_queue_family = i;
                        break;
                    }
                }
            }

            result.support_swapchain = std::ranges::any_of(
                extension_properties,
                [](const VkExtensionProperties& extension) {
                    return std::string_view(extension.extensionName) ==
                           VK_KHR_SWAPCHAIN_EXTENSION_NAME;
                }
            );
            result.swapchain_adequate =
                result.support_swapchain &&
                result.surface_support_present &&
                !surface_formats.empty() &&
                !present_modes.empty();

            result.geometry_shader = features.geometryShader == VK_TRUE;
            result.tessellation_shader = features.tessellationShader == VK_TRUE;
            result.sampler_anisotropy = features.samplerAnisotropy == VK_TRUE;
            result.multi_draw_indirect = features.multiDrawIndirect == VK_TRUE;
            result.fill_mode_non_solid = features.fillModeNonSolid == VK_TRUE;
            result.wide_lines = features.wideLines == VK_TRUE;
            result.shader_int64 = features.shaderInt64 == VK_TRUE;
            result.shader_float64 = features.shaderFloat64 == VK_TRUE;

            result.max_image_dimension_2d = properties.limits.maxImageDimension2D;
            result.max_push_constants_size = properties.limits.maxPushConstantsSize;
            result.max_bound_descriptor_sets = properties.limits.maxBoundDescriptorSets;
            result.max_color_attachments = properties.limits.maxColorAttachments;
            result.max_compute_work_group_invocations =
                properties.limits.maxComputeWorkGroupInvocations;
            result.max_sampler_anisotropy = properties.limits.maxSamplerAnisotropy;
            result.timestamp_period = properties.limits.timestampPeriod;

            return result;
        }
    };
}
