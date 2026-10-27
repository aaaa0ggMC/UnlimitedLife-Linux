#pragma once

#include <vulkan/vulkan.h>
#include <algorithm>
#include <limits>
#include <optional>

namespace ave::detail {
    inline bool fixed_surface_extent(const VkSurfaceCapabilitiesKHR& cap) {
        return cap.currentExtent.width != std::numeric_limits<uint32_t>::max();
    }

    inline VkExtent2D window_extent(const VkSurfaceCapabilitiesKHR& cap,
                                   std::optional<VkExtent2D> requested, bool android) {
        // Android may expose native-display-oriented buffer dimensions. The
        // SurfaceView size is authoritative for non-prerotated scene/UI/input.
        if(android && requested) return *requested;
        if(fixed_surface_extent(cap)) return cap.currentExtent;
        return requested.value_or(cap.minImageExtent);
    }

    inline VkExtent2D swapchain_extent(const VkSurfaceCapabilitiesKHR& cap,
                                      VkExtent2D window, bool android) {
        if(!android && fixed_surface_extent(cap)) return cap.currentExtent;
        return {
            std::clamp(window.width, cap.minImageExtent.width, cap.maxImageExtent.width),
            std::clamp(window.height, cap.minImageExtent.height, cap.maxImageExtent.height)
        };
    }

    inline bool valid_swapchain_extent(const VkSurfaceCapabilitiesKHR& cap,
                                       VkExtent2D extent, bool android) {
        if(extent.width == 0 || extent.height == 0) return false;
        if(!android && fixed_surface_extent(cap)) {
            return extent.width == cap.currentExtent.width &&
                   extent.height == cap.currentExtent.height;
        }
        // Android WSI explicitly allows extents different from currentExtent;
        // the compositor scales them. Desktop fixed extents remain strict.
        return extent.width >= cap.minImageExtent.width &&
               extent.width <= cap.maxImageExtent.width &&
               extent.height >= cap.minImageExtent.height &&
               extent.height <= cap.maxImageExtent.height;
    }

    inline VkSurfaceTransformFlagBitsKHR swapchain_transform(
        const VkSurfaceCapabilitiesKHR& cap, bool android) {
        // currentTransform promises that the app ALREADY pre-rotated all pixels.
        // The default Android renderer does not rotate scene/UI/scissors/input:
        // request identity and let the compositor handle display orientation.
        // initialize() rejects unsupported identity; never silently draw sideways.
        return android ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : cap.currentTransform;
    }
}
