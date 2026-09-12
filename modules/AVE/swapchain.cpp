module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

module ave.render;

import std;
import alib6;
import ave.ecode;
import :swapchain;

namespace ave {

bool WithSwapchain::supports(VkSurfaceFormatKHR value) const noexcept {
    if(formats.size() == 1 && formats.front().format == VK_FORMAT_UNDEFINED) {
        return formats.front().colorSpace == value.colorSpace;
    }
    return std::ranges::any_of(formats, [value](const auto& candidate) {
        return candidate.format == value.format &&
               candidate.colorSpace == value.colorSpace;
    });
}

bool WithSwapchain::supports(VkPresentModeKHR value) const noexcept {
    return std::ranges::contains(present_modes, value);
}

std::optional<WithSwapchain> query_swapchain_support(
    const std::shared_ptr<Device>& device,
    const std::shared_ptr<Surface>& surface,
    alib6::ErrorWrapper ew
) {
    if(!device || device->get_system_handle() == VK_NULL_HANDLE ||
       device->get_physical_device() == VK_NULL_HANDLE ||
       !surface || surface->get_system_handle() == VK_NULL_HANDLE ||
       device->get_instance() != surface->get_instance()) {
        ew.report(ave_vk_create_swapchain,
            "Cannot query Swapchain support without a compatible Device and Surface.");
        return std::nullopt;
    }

    WithSwapchain result;
    const auto physical_device = device->get_physical_device();
    const auto surface_handle = surface->get_system_handle();
    VkResult code = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physical_device, surface_handle, &result.capabilities);
    if(code != VK_SUCCESS) {
        ew.report(ave_vk_create_swapchain,
            "Failed to query Vulkan Surface capabilities ({}).", static_cast<int>(code));
        return std::nullopt;
    }

    alib6::u32 count = 0;
    do {
        code = vkGetPhysicalDeviceSurfaceFormatsKHR(
            physical_device, surface_handle, &count, nullptr);
        if(code != VK_SUCCESS) break;
        result.formats.resize(count);
        code = vkGetPhysicalDeviceSurfaceFormatsKHR(
            physical_device, surface_handle, &count, result.formats.data());
    } while(code == VK_INCOMPLETE);
    if(code != VK_SUCCESS) {
        ew.report(ave_vk_create_swapchain,
            "Failed to query Vulkan Surface formats ({}).", static_cast<int>(code));
        return std::nullopt;
    }
    result.formats.resize(count);

    count = 0;
    do {
        code = vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device, surface_handle, &count, nullptr);
        if(code != VK_SUCCESS) break;
        result.present_modes.resize(count);
        code = vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device, surface_handle, &count, result.present_modes.data());
    } while(code == VK_INCOMPLETE);
    if(code != VK_SUCCESS) {
        ew.report(ave_vk_create_swapchain,
            "Failed to query Vulkan present modes ({}).", static_cast<int>(code));
        return std::nullopt;
    }
    result.present_modes.resize(count);

    const auto [width, height] = surface->get_window().get_framebuffer_size();
    result.framebuffer_extent = {
        static_cast<alib6::u32>(std::max(width, 0)),
        static_cast<alib6::u32>(std::max(height, 0))
    };

    alib6::u32 family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &family_count, families.data());
    for(alib6::u32 i = 0; i < family_count; ++i) {
        if(!device->has_queue(i) || families[i].queueCount == 0) continue;
        VkBool32 can_present = VK_FALSE;
        code = vkGetPhysicalDeviceSurfaceSupportKHR(
            physical_device, i, surface_handle, &can_present);
        if(code != VK_SUCCESS) continue;
        const bool can_draw = (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        if(can_draw && !result.graphics_queue_family) result.graphics_queue_family = i;
        if(can_present == VK_TRUE && !result.present_queue_family) {
            result.present_queue_family = i;
        }
        if(can_draw && can_present == VK_TRUE) {
            result.graphics_queue_family = i;
            result.present_queue_family = i;
            break;
        }
    }

    if(result.formats.empty() || result.present_modes.empty() ||
       !result.graphics_queue_family || !result.present_queue_family) {
        ew.report(ave_vk_create_swapchain,
            "The selected Device and Surface do not provide an adequate Swapchain.");
        return std::nullopt;
    }
    return result;
}

void default_configure_swapchain(WithSwapchain& with, CreateSwapchainInfo& ci) {
    constexpr auto color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    const auto choose_format = [&](VkFormat format) {
        return std::ranges::find_if(with.formats, [=](const auto& candidate) {
            return candidate.format == format && candidate.colorSpace == color_space;
        });
    };
    if(with.formats.size() == 1 &&
       with.formats.front().format == VK_FORMAT_UNDEFINED) {
        ci.surface_format = { VK_FORMAT_B8G8R8A8_SRGB, with.formats.front().colorSpace };
    }else if(const auto found = choose_format(VK_FORMAT_B8G8R8A8_SRGB);
             found != with.formats.end()) {
        ci.surface_format = *found;
    }else if(const auto found = choose_format(VK_FORMAT_B8G8R8_SRGB);
             found != with.formats.end()) {
        ci.surface_format = *found;
    }else{
        ci.surface_format = with.formats.front();
    }

    ci.present_mode = with.supports(VK_PRESENT_MODE_MAILBOX_KHR)
        ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR;
    if(with.capabilities.currentExtent.width !=
       std::numeric_limits<alib6::u32>::max()) {
        ci.extent = with.capabilities.currentExtent;
    }else{
        ci.extent.width = std::clamp(with.framebuffer_extent.width,
            with.capabilities.minImageExtent.width,
            with.capabilities.maxImageExtent.width);
        ci.extent.height = std::clamp(with.framebuffer_extent.height,
            with.capabilities.minImageExtent.height,
            with.capabilities.maxImageExtent.height);
    }

    ci.image_count = with.capabilities.minImageCount + 1;
    if(with.capabilities.maxImageCount > 0) {
        ci.image_count = std::min(ci.image_count, with.capabilities.maxImageCount);
    }
    ci.image_array_layers = 1;
    ci.image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.queue_family_indices.clear();
    if(with.graphics_queue_family != with.present_queue_family) {
        ci.sharing_mode = VK_SHARING_MODE_CONCURRENT;
        ci.queue_family_indices = {
            *with.graphics_queue_family, *with.present_queue_family
        };
    }else{
        ci.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    }
    ci.pre_transform = with.capabilities.currentTransform;
    constexpr std::array alpha_modes {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
    };
    for(const auto mode : alpha_modes) {
        if((with.capabilities.supportedCompositeAlpha & mode) != 0) {
            ci.composite_alpha = mode;
            break;
        }
    }
    ci.clipped = true;
}

bool Swapchain::initialize(CreateSwapchainInfo ci, const WithSwapchain& with) {
    if(!ci.device || !ci.surface ||
       ci.device->get_system_handle() == VK_NULL_HANDLE ||
       ci.surface->get_system_handle() == VK_NULL_HANDLE ||
       ci.device->get_instance() != ci.surface->get_instance() ||
       !ci.device->extension_enabled(VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
        ci.ew.report(ave_vk_create_swapchain,
            "Cannot create a Swapchain without compatible dependencies and VK_KHR_swapchain.");
        return false;
    }

    if(ci.old_swapchain &&
       (ci.old_swapchain->get_device() != ci.device ||
        ci.old_swapchain->get_surface() != ci.surface)) {
        ci.ew.report(ave_vk_create_swapchain,
            "old_swapchain belongs to different Device/Surface dependencies.");
        return false;
    }

    const auto& cap = with.capabilities;
    const bool fixed_extent = cap.currentExtent.width !=
        std::numeric_limits<alib6::u32>::max();
    const bool valid_extent = fixed_extent
        ? ci.extent.width == cap.currentExtent.width &&
          ci.extent.height == cap.currentExtent.height
        : ci.extent.width >= cap.minImageExtent.width &&
          ci.extent.width <= cap.maxImageExtent.width &&
          ci.extent.height >= cap.minImageExtent.height &&
          ci.extent.height <= cap.maxImageExtent.height;
    const bool valid_count = ci.image_count >= cap.minImageCount &&
        (cap.maxImageCount == 0 || ci.image_count <= cap.maxImageCount);
    const bool valid_usage = ci.image_usage != 0 &&
        (ci.image_usage & ~cap.supportedUsageFlags) == 0;
    const bool valid_sharing = ci.sharing_mode == VK_SHARING_MODE_EXCLUSIVE ||
        (ci.sharing_mode == VK_SHARING_MODE_CONCURRENT &&
         ci.queue_family_indices.size() >= 2);
    if(!with.supports(ci.surface_format) || !with.supports(ci.present_mode) ||
       !valid_extent || !valid_count || !valid_usage ||
       ci.image_array_layers == 0 || ci.image_array_layers > cap.maxImageArrayLayers ||
       (cap.supportedTransforms & ci.pre_transform) == 0 ||
       (cap.supportedCompositeAlpha & ci.composite_alpha) == 0 || !valid_sharing) {
        ci.ew.report(ave_vk_create_swapchain,
            "CreateSwapchainInfo contains values unsupported by the current Surface.");
        return false;
    }

    auto queue_families = ci.queue_family_indices;
    std::ranges::sort(queue_families);
    queue_families.erase(
        std::unique(queue_families.begin(), queue_families.end()),
        queue_families.end());
    if(ci.sharing_mode == VK_SHARING_MODE_CONCURRENT &&
       !std::ranges::all_of(queue_families, [&](alib6::u32 family) {
           return ci.device->has_queue(family);
       })) {
        ci.ew.report(ave_vk_create_swapchain,
            "Concurrent sharing references a queue family not created by Device.");
        return false;
    }

    VkSwapchainCreateInfoKHR create_info {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.flags = ci.flags;
    create_info.surface = ci.surface->get_system_handle();
    create_info.minImageCount = ci.image_count;
    create_info.imageFormat = ci.surface_format.format;
    create_info.imageColorSpace = ci.surface_format.colorSpace;
    create_info.imageExtent = ci.extent;
    create_info.imageArrayLayers = ci.image_array_layers;
    create_info.imageUsage = ci.image_usage;
    create_info.imageSharingMode = ci.sharing_mode;
    create_info.queueFamilyIndexCount = ci.sharing_mode == VK_SHARING_MODE_CONCURRENT
        ? static_cast<alib6::u32>(queue_families.size()) : 0;
    create_info.pQueueFamilyIndices = ci.sharing_mode == VK_SHARING_MODE_CONCURRENT
        ? queue_families.data() : nullptr;
    create_info.preTransform = ci.pre_transform;
    create_info.compositeAlpha = ci.composite_alpha;
    create_info.presentMode = ci.present_mode;
    create_info.clipped = ci.clipped ? VK_TRUE : VK_FALSE;
    create_info.oldSwapchain = ci.old_swapchain
        ? ci.old_swapchain->get_system_handle() : VK_NULL_HANDLE;

    device = ci.device;
    surface = ci.surface;
    const auto allocator = device->get_instance()->get_vk_allocator();
    VkResult code = vkCreateSwapchainKHR(
        device->get_system_handle(), &create_info, allocator, &swapchain);
    if(code != VK_SUCCESS) {
        ci.ew.report(ave_vk_create_swapchain,
            "Failed to create Vulkan Swapchain ({}).", static_cast<int>(code));
        device.reset();
        surface.reset();
        return false;
    }

    alib6::u32 actual_count = 0;
    std::vector<VkImage> queried_images;
    do {
        code = vkGetSwapchainImagesKHR(
            device->get_system_handle(), swapchain, &actual_count, nullptr);
        if(code != VK_SUCCESS) break;
        queried_images.resize(actual_count);
        code = vkGetSwapchainImagesKHR(
            device->get_system_handle(), swapchain, &actual_count,
            queried_images.data());
    } while(code == VK_INCOMPLETE);
    if(code != VK_SUCCESS || actual_count == 0) {
        ci.ew.report(ave_vk_create_swapchain,
            "Failed to obtain Vulkan Swapchain images ({}).", static_cast<int>(code));
        destroy();
        return false;
    }
    image_count = actual_count;
    image_usage = ci.image_usage;

    surface_format = ci.surface_format;
    present_mode = ci.present_mode;
    extent = ci.extent;
    graphics_queue_family = *with.graphics_queue_family;
    present_queue_family = *with.present_queue_family;
    return true;
}

Swapchain::~Swapchain() { destroy(); }

std::shared_ptr<Swapchain> Swapchain::create(
    CreateSwapchainInfo ci, const WithSwapchain& with
) {
    auto result = std::shared_ptr<Swapchain>(new Swapchain());
    if(!result->initialize(std::move(ci), with)) return {};
    return result;
}

void Swapchain::destroy() noexcept {
    if(device && device->get_system_handle() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device->get_system_handle());
        const auto allocator = device->get_instance()->get_vk_allocator();
        if(swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device->get_system_handle(), swapchain, allocator);
        }
    }
    image_count = 0;
    image_usage = 0;
    swapchain = VK_NULL_HANDLE;
    surface_format = {};
    present_mode = VK_PRESENT_MODE_FIFO_KHR;
    extent = {};
    graphics_queue_family = 0;
    present_queue_family = 0;
    surface.reset();
    device.reset();
}

VkSwapchainKHR Swapchain::get_system_handle() const noexcept { return swapchain; }
const std::shared_ptr<Device>& Swapchain::get_device() const noexcept { return device; }
const std::shared_ptr<Surface>& Swapchain::get_surface() const noexcept { return surface; }
alib6::u32 Swapchain::get_image_count() const noexcept { return image_count; }
VkImageUsageFlags Swapchain::get_image_usage() const noexcept {
    return image_usage;
}

std::optional<std::vector<VkImage>> Swapchain::enumerate_images(
    alib6::ErrorWrapper ew
) const {
    if(!device || device->get_system_handle() == VK_NULL_HANDLE ||
       swapchain == VK_NULL_HANDLE) {
        ew.report(ave_vk_create_image,
            "Cannot enumerate Images from an invalid Swapchain.");
        return std::nullopt;
    }
    alib6::u32 count = 0;
    std::vector<VkImage> result;
    VkResult code = VK_SUCCESS;
    do {
        code = vkGetSwapchainImagesKHR(
            device->get_system_handle(), swapchain, &count, nullptr);
        if(code != VK_SUCCESS) break;
        result.resize(count);
        code = vkGetSwapchainImagesKHR(
            device->get_system_handle(), swapchain, &count, result.data());
    } while(code == VK_INCOMPLETE);
    if(code != VK_SUCCESS || count == 0) {
        ew.report(ave_vk_create_image,
            "Failed to enumerate Swapchain Images ({}).", static_cast<int>(code));
        return std::nullopt;
    }
    result.resize(count);
    return result;
}
VkSurfaceFormatKHR Swapchain::get_surface_format() const noexcept { return surface_format; }
VkPresentModeKHR Swapchain::get_present_mode() const noexcept { return present_mode; }
VkExtent2D Swapchain::get_extent() const noexcept { return extent; }
alib6::u32 Swapchain::get_graphics_queue_family() const noexcept {
    return graphics_queue_family;
}
alib6::u32 Swapchain::get_present_queue_family() const noexcept {
    return present_queue_family;
}
Swapchain::operator bool() const noexcept { return swapchain != VK_NULL_HANDLE; }

}
