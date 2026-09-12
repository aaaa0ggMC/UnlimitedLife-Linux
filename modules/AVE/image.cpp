module;
#include <vulkan/vulkan.h>

module ave.render;

import std;
import alib6;
import ave.ecode;
import :image;

namespace ave {
namespace {
    VkImageAspectFlags infer_aspect_mask(VkFormat format) noexcept {
        switch(format) {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_D32_SFLOAT:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            case VK_FORMAT_S8_UINT:
                return VK_IMAGE_ASPECT_STENCIL_BIT;
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            default:
                return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    std::optional<alib6::u32> find_memory_type(
        VkPhysicalDevice physical_device,
        alib6::u32 allowed_types,
        VkMemoryPropertyFlags required_properties
    ) {
        VkPhysicalDeviceMemoryProperties properties {};
        vkGetPhysicalDeviceMemoryProperties(physical_device, &properties);
        for(alib6::u32 i = 0; i < properties.memoryTypeCount; ++i) {
            const bool allowed = (allowed_types & (1u << i)) != 0;
            const bool supported =
                (properties.memoryTypes[i].propertyFlags & required_properties) ==
                required_properties;
            if(allowed && supported) return i;
        }
        return std::nullopt;
    }
}

bool Image::initialize(CreateImageInfo ci) {
    if(!ci.device || ci.device->get_system_handle() == VK_NULL_HANDLE ||
       ci.format == VK_FORMAT_UNDEFINED ||
       ci.extent.width == 0 || ci.extent.height == 0 || ci.extent.depth == 0 ||
       ci.mip_levels == 0 || ci.array_layers == 0 || ci.usage == 0 ||
       (ci.sharing_mode == VK_SHARING_MODE_CONCURRENT &&
        ci.queue_family_indices.size() < 2)) {
        ci.ew.report(ave_vk_create_image,
            "CreateImageInfo is incomplete or invalid.");
        return false;
    }

    auto queue_families = ci.queue_family_indices;
    std::ranges::sort(queue_families);
    queue_families.erase(
        std::unique(queue_families.begin(), queue_families.end()),
        queue_families.end());
    if(ci.sharing_mode == VK_SHARING_MODE_CONCURRENT &&
       queue_families.size() < 2) {
        ci.ew.report(ave_vk_create_image,
            "Concurrent Image sharing requires at least two distinct queue families.");
        return false;
    }

    VkImageCreateInfo image_info {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.pNext = ci.next;
    image_info.flags = ci.flags;
    image_info.imageType = ci.image_type;
    image_info.format = ci.format;
    image_info.extent = ci.extent;
    image_info.mipLevels = ci.mip_levels;
    image_info.arrayLayers = ci.array_layers;
    image_info.samples = ci.samples;
    image_info.tiling = ci.tiling;
    image_info.usage = ci.usage;
    image_info.sharingMode = ci.sharing_mode;
    image_info.queueFamilyIndexCount = ci.sharing_mode == VK_SHARING_MODE_CONCURRENT
        ? static_cast<alib6::u32>(queue_families.size()) : 0;
    image_info.pQueueFamilyIndices = ci.sharing_mode == VK_SHARING_MODE_CONCURRENT
        ? queue_families.data() : nullptr;
    image_info.initialLayout = ci.initial_layout;

    device = ci.device;
    const auto handle = device->get_system_handle();
    const auto allocator = device->get_instance()->get_vk_allocator();
    VkResult code = vkCreateImage(handle, &image_info, allocator, &image);
    if(code != VK_SUCCESS) {
        ci.ew.report(ave_vk_create_image,
            "Failed to create Vulkan Image ({}).", static_cast<int>(code));
        destroy();
        return false;
    }

    VkMemoryRequirements requirements {};
    vkGetImageMemoryRequirements(handle, image, &requirements);
    const auto memory_type = find_memory_type(
        device->get_physical_device(),
        requirements.memoryTypeBits,
        ci.memory_properties
    );
    if(!memory_type) {
        ci.ew.report(ave_vk_create_image,
            "No compatible Vulkan memory type exists for the Image.");
        destroy();
        return false;
    }

    VkMemoryAllocateInfo allocate_info {};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.pNext = ci.memory_allocate_next;
    allocate_info.allocationSize = requirements.size;
    allocate_info.memoryTypeIndex = *memory_type;
    code = vkAllocateMemory(handle, &allocate_info, allocator, &memory);
    if(code == VK_SUCCESS) code = vkBindImageMemory(handle, image, memory, 0);
    if(code != VK_SUCCESS) {
        ci.ew.report(ave_vk_create_image,
            "Failed to allocate or bind Vulkan Image memory ({}).",
            static_cast<int>(code));
        destroy();
        return false;
    }

    aspect_mask = ci.image_view_subresource_range.aspectMask != 0
        ? ci.image_view_subresource_range.aspectMask
        : infer_aspect_mask(ci.format);
    if(ci.create_view) {
        auto range = ci.image_view_subresource_range;
        range.aspectMask = aspect_mask;

        VkImageViewCreateInfo view_info {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.pNext = ci.image_view_next;
        view_info.flags = ci.image_view_flags;
        view_info.image = image;
        view_info.viewType = ci.image_view_type;
        view_info.format = ci.image_view_format == VK_FORMAT_UNDEFINED
            ? ci.format : ci.image_view_format;
        view_info.components = ci.image_view_components;
        view_info.subresourceRange = range;
        code = vkCreateImageView(handle, &view_info, allocator, &image_view);
        if(code != VK_SUCCESS) {
            ci.ew.report(ave_vk_create_image,
                "Failed to create Vulkan ImageView ({}).", static_cast<int>(code));
            destroy();
            return false;
        }
    }

    format = ci.format;
    extent = ci.extent;
    usage = ci.usage;
    return true;
}

Image::~Image() { destroy(); }

std::shared_ptr<Image> Image::create(CreateImageInfo ci) {
    auto result = std::shared_ptr<Image>(new Image());
    if(!result->initialize(std::move(ci))) return {};
    return result;
}

bool Image::initialize_swapchain_image(CreateSwapchainImageInfo ci) {
    if(!ci.swapchain || !*ci.swapchain || ci.image == VK_NULL_HANDLE) {
        ci.ew.report(ave_vk_create_image,
            "Cannot wrap an invalid Swapchain Image.");
        return false;
    }
    swapchain = ci.swapchain;
    device = swapchain->get_device();
    image = ci.image;
    format = swapchain->get_surface_format().format;
    const auto swapchain_extent = swapchain->get_extent();
    extent = { swapchain_extent.width, swapchain_extent.height, 1 };
    usage = swapchain->get_image_usage();
    aspect_mask = ci.image_view_subresource_range.aspectMask;

    VkImageViewCreateInfo view_info {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.pNext = ci.image_view_next;
    view_info.flags = ci.image_view_flags;
    view_info.image = image;
    view_info.viewType = ci.image_view_type;
    view_info.format = format;
    view_info.components = ci.image_view_components;
    view_info.subresourceRange = ci.image_view_subresource_range;
    const VkResult code = vkCreateImageView(
        device->get_system_handle(),
        &view_info,
        device->get_instance()->get_vk_allocator(),
        &image_view
    );
    if(code != VK_SUCCESS) {
        ci.ew.report(ave_vk_create_image,
            "Failed to create a Swapchain ImageView ({}).",
            static_cast<int>(code));
        destroy();
        return false;
    }
    return true;
}

std::shared_ptr<Image> Image::create_swapchain_image(
    CreateSwapchainImageInfo ci
) {
    auto result = std::shared_ptr<Image>(new Image());
    if(!result->initialize_swapchain_image(std::move(ci))) return {};
    return result;
}

void Image::destroy() noexcept {
    if(device && device->get_system_handle() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device->get_system_handle());
        const auto handle = device->get_system_handle();
        const auto allocator = device->get_instance()->get_vk_allocator();
        if(image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(handle, image_view, allocator);
        }
        if(!swapchain && image != VK_NULL_HANDLE) {
            vkDestroyImage(handle, image, allocator);
        }
        if(!swapchain && memory != VK_NULL_HANDLE) {
            vkFreeMemory(handle, memory, allocator);
        }
    }
    image_view = VK_NULL_HANDLE;
    image = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;
    format = VK_FORMAT_UNDEFINED;
    extent = {};
    usage = 0;
    aspect_mask = 0;
    swapchain.reset();
    device.reset();
}

const std::shared_ptr<Device>& Image::get_device() const noexcept { return device; }
const std::shared_ptr<Swapchain>& Image::get_swapchain() const noexcept {
    return swapchain;
}
VkImage Image::get_system_handle() const noexcept { return image; }
VkDeviceMemory Image::get_memory() const noexcept { return memory; }
VkImageView Image::get_image_view() const noexcept { return image_view; }
VkFormat Image::get_format() const noexcept { return format; }
VkExtent3D Image::get_extent() const noexcept { return extent; }
VkImageUsageFlags Image::get_usage() const noexcept { return usage; }
VkImageAspectFlags Image::get_aspect_mask() const noexcept { return aspect_mask; }
Image::operator bool() const noexcept { return image != VK_NULL_HANDLE; }

WithImagesInput::WithImagesInput(
    std::shared_ptr<Device> target_device,
    std::shared_ptr<Swapchain> target_swapchain
)
:device(std::move(target_device))
,swapchain(std::move(target_swapchain)){}

const std::shared_ptr<Device>& WithImagesInput::get_device() const noexcept {
    return device;
}

const std::shared_ptr<Swapchain>& WithImagesInput::get_swapchain() const noexcept {
    return swapchain;
}

VkExtent2D WithImagesInput::get_extent() const noexcept {
    return swapchain ? swapchain->get_extent() : VkExtent2D {};
}

alib6::u32 WithImagesInput::get_swapchain_image_count() const noexcept {
    return swapchain
        ? swapchain->get_image_count() : 0;
}

VkSurfaceFormatKHR WithImagesInput::get_surface_format() const noexcept {
    return swapchain ? swapchain->get_surface_format() : VkSurfaceFormatKHR {};
}

void default_configure_images(WithImagesInput& input, CreateImagesInfo& ci) {
    ci.images.clear();
    const auto device = input.get_device();
    const auto extent = input.get_extent();
    const auto image_count = input.get_swapchain_image_count();
    if(!device || device->get_physical_device() == VK_NULL_HANDLE ||
       extent.width == 0 || extent.height == 0 || image_count == 0) return;

    constexpr std::array candidates {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D16_UNORM
    };
    VkFormat depth_format = VK_FORMAT_UNDEFINED;
    for(const auto candidate : candidates) {
        VkFormatProperties properties {};
        vkGetPhysicalDeviceFormatProperties(
            device->get_physical_device(), candidate, &properties);
        if((properties.optimalTilingFeatures &
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
            depth_format = candidate;
            break;
        }
    }
    if(depth_format == VK_FORMAT_UNDEFINED) return;

    ci.images.reserve(image_count);
    for(alib6::u32 i = 0; i < image_count; ++i) {
        CreateImageInfo image;
        image.device = device;
        image.format = depth_format;
        image.extent = { extent.width, extent.height, 1 };
        image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        ci.images.push_back(std::move(image));
    }
}
}
