module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>
#include <stb/stb_image.h>
#include <cstring>
#include <limits>

module ave.render;

import std;
import alib6;
import ave.ecode;
import :image;
import :buffer;
import :upload_context;

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

    VkImageViewType infer_view_type(
        VkImageType type,
        VkImageCreateFlags flags,
        alib6::u32 layers
    ) noexcept {
        switch(type) {
        case VK_IMAGE_TYPE_1D:
            return layers > 1
                ? VK_IMAGE_VIEW_TYPE_1D_ARRAY
                : VK_IMAGE_VIEW_TYPE_1D;
        case VK_IMAGE_TYPE_3D:
            return VK_IMAGE_VIEW_TYPE_3D;
        case VK_IMAGE_TYPE_2D:
        default:
            if(flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
                return VK_IMAGE_VIEW_TYPE_CUBE;
            return layers > 1
                ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                : VK_IMAGE_VIEW_TYPE_2D;
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

void Image::adopt_parent(const Image& parent) noexcept {
    device = parent.device;
    swapchain = parent.swapchain;
    allocator = parent.allocator;
    image = parent.image;
    format = parent.format;
    extent = parent.extent;
    mip_levels = parent.mip_levels;
    array_layers = parent.array_layers;
    usage = parent.usage;
    aspect_mask = parent.aspect_mask;
    image_type = parent.image_type;
    image_flags = parent.image_flags;
    owns_image = false;
}

void Image::resolve_self_view_config(ImageViewConfig& cfg) const noexcept {
    if(cfg.format == VK_FORMAT_UNDEFINED) cfg.format = format;
    auto& range = cfg.subresource_range;
    if(range.aspectMask == 0) range.aspectMask = infer_aspect_mask(cfg.format);
    if(range.levelCount == 0) range.levelCount = mip_levels ? mip_levels : 1;
    if(range.layerCount == 0) range.layerCount = array_layers ? array_layers : 1;
    // view_type 保持默认 2D 时，与图像形态不匹配才推断（用户显式指定非默认值则尊重）。
    if(cfg.view_type == VK_IMAGE_VIEW_TYPE_2D) {
        const auto inferred = infer_view_type(image_type, image_flags, array_layers);
        if(inferred != VK_IMAGE_VIEW_TYPE_2D) cfg.view_type = inferred;
    }
}

bool Image::initialize_default_view(
    const ImageViewConfig& config,
    alib6::ErrorWrapper& ew
) {
    ImageViewConfig cfg = config;
    resolve_self_view_config(cfg);

    default_view = ImageView::create(CreateImageViewInfo{
        .image = shared_from_this(),
        .view = std::move(cfg),
        .ew = ew
    });
    return static_cast<bool>(default_view);
}

bool Image::initialize(CreateImageInfo ci) {
    if(!ci.allocator && !ci.device) {
        ci.ew.report(ave_vk_create_image,
            "CreateImageInfo requires either a VMAAllocator or a Device.");
        return false;
    }

    std::shared_ptr<Device> target_device = ci.device;
    if(ci.allocator) {
        if(!*ci.allocator) {
            ci.ew.report(ave_vk_create_image, "CreateImageInfo has an invalid VMAAllocator.");
            return false;
        }
        allocator = ci.allocator;
        if(!target_device) target_device = allocator->get_device();
    }
    if(!target_device || target_device->get_system_handle() == VK_NULL_HANDLE ||
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

    device = target_device;
    const auto handle = device->get_system_handle();
    const auto vk_allocator = device->get_instance()->get_vk_allocator();

    // VkImage 统一在此创建（保持与其他 AVE 完全相同的 loader 分发路径），
    // 之后按后端二选一：VMA 分配并绑定显存，或手工分配。
    auto code = vkCreateImage(handle, &image_info, vk_allocator, &image);
    if(code != VK_SUCCESS) {
        ci.ew.report(ave_vk_create_image,
            "Failed to create Vulkan Image ({}).", int(code));
        device.reset();
        allocator.reset();
        return false;
    }

    if(allocator) {
        vma_binding = VmaMemoryPolicy::create_image(
            allocator,
            image,
            ci.required_memory_properties,
            ci.preferred_memory_properties,
            ci.dedicated_allocation,
            ci.ew
        );
        if(!vma_binding) {
            vkDestroyImage(handle, image, vk_allocator);
            image = VK_NULL_HANDLE;
            device.reset();
            allocator.reset();
            return false;
        }
    }else{
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
        code = vkAllocateMemory(handle, &allocate_info, vk_allocator, &memory);
        if(code == VK_SUCCESS) code = vkBindImageMemory(handle, image, memory, 0);
        if(code != VK_SUCCESS) {
            ci.ew.report(ave_vk_create_image,
                "Failed to allocate or bind Vulkan Image memory ({}).", int(code));
            destroy();
            return false;
        }
        owns_image = true;
    }

    format = ci.format;
    extent = ci.extent;
    mip_levels = ci.mip_levels;
    array_layers = ci.array_layers;
    image_type = ci.image_type;
    image_flags = ci.flags;
    usage = ci.usage;
    aspect_mask = ci.default_view && ci.default_view->subresource_range.aspectMask != 0
        ? ci.default_view->subresource_range.aspectMask
        : infer_aspect_mask(ci.format);

    if(ci.default_view) {
        if(!initialize_default_view(*ci.default_view, ci.ew)) {
            destroy();
            return false;
        }
    }
    return true;
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
    image_type = VK_IMAGE_TYPE_2D;
    mip_levels = 1;
    array_layers = 1;
    owns_image = false;
    aspect_mask = ci.view.subresource_range.aspectMask != 0
        ? ci.view.subresource_range.aspectMask
        : infer_aspect_mask(format);

    if(!initialize_default_view(ci.view, ci.ew)) {
        destroy();
        return false;
    }
    return true;
}

Image::~Image() { destroy(); }

std::shared_ptr<Image> Image::create(CreateImageInfo ci) {
    auto result = std::shared_ptr<Image>(new Image());
    if(!result->initialize(std::move(ci))) return {};
    return result;
}

std::shared_ptr<Image> Image::create_swapchain_image(
    CreateSwapchainImageInfo ci
) {
    auto result = std::shared_ptr<Image>(new Image());
    if(!result->initialize_swapchain_image(std::move(ci))) return {};
    return result;
}

std::shared_ptr<Image> Image::create_from_file(
    CreateImageInfo ci,
    const alib6::io::FileEntry& entry,
    AllocateBufferInfo upload
) {
    if(entry.invalid()) {
        ci.ew.report(ave_vk_image_upload,
            "create_from_file requires a valid alib6::io::FileEntry.");
        return {};
    }

    const auto bytes = entry.read(0, alib6::get_default_resource(), ci.ew);
    if(bytes.empty()) {
        ci.ew.report(ave_vk_image_upload,
            "Failed to read image file '{}'.", entry.path);
        return {};
    }

    // stb 解码始终输出 4 通道（与 AGE 的纹理管线一致，规避 3 通道对齐问题）。
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(bytes.data()),
        static_cast<int>(bytes.size()),
        &width, &height, &channels, 4);
    if(!pixels) {
        ci.ew.report(ave_vk_image_upload,
            "stb failed to decode image '{}' ({}).", entry.path, stbi_failure_reason());
        return {};
    }
    struct PixelGuard {
        stbi_uc* ptr;
        ~PixelGuard() { if(ptr) stbi_image_free(ptr); }
    } guard { pixels };

    if(width <= 0 || height <= 0) {
        ci.ew.report(ave_vk_image_upload,
            "Decoded image '{}' has an invalid size.", entry.path);
        return {};
    }

    if(ci.format == VK_FORMAT_UNDEFINED) ci.format = VK_FORMAT_R8G8B8A8_UNORM;
    if(ci.extent.width == 0 && ci.extent.height == 0 && ci.extent.depth == 0) {
        ci.extent = {
            static_cast<alib6::u32>(width),
            static_cast<alib6::u32>(height),
            1
        };
    }
    if(ci.mip_levels == 0) ci.mip_levels = 1;
    if(ci.array_layers == 0) ci.array_layers = 1;
    if(ci.usage == 0) {
        ci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    auto image = create(std::move(ci));
    if(!image) return {};

    const auto byte_count =
        static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4u;
    if(!image->upload_pixels(pixels, byte_count, std::move(upload))) {
        return {};
    }
    return image;
}

bool Image::upload_pixels(
    const void* data,
    VkDeviceSize bytes,
    AllocateBufferInfo ai
) {
    if(!*this || !device || data == nullptr || bytes == 0) {
        ai.ew.report(ave_vk_image_upload, "Cannot upload pixels to an invalid Image.");
        return false;
    }
    auto* upload_context = ai.map_info.upload_context;
    if(!upload_context || !*upload_context) {
        ai.ew.report(ave_vk_image_upload,
            "Uploading image pixels requires a valid UploadContext (map_info.upload_context).");
        return false;
    }
    if((usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0) {
        ai.ew.report(ave_vk_image_upload,
            "Target Image lacks VK_IMAGE_USAGE_TRANSFER_DST_BIT.");
        return false;
    }

    VkBuffer src = VK_NULL_HANDLE;
    VkDeviceSize src_offset = 0;
    // 内部临时 staging：生命周期包住同步上传（submit + fence wait）全程。
    std::unique_ptr<VMABuffer> transient;

    if(ai.map_info.staging) {
        // 用户自带 staging（与 VMABuffer::alloc 的 .staging 语义一致），
        // 要求 host_access 为 SequentialWrite（HOST_COHERENT）以便直写后提交。
        src = ai.map_info.staging.buffer;
        src_offset = ai.map_info.staging.offset;
        if(src == VK_NULL_HANDLE || !ai.map_info.staging.mapped_ptr ||
           ai.map_info.staging.capacity < bytes) {
            ai.ew.report(ave_vk_image_upload,
                "User-provided staging buffer cannot host the image data ({} bytes).", bytes);
            return false;
        }
        std::memcpy(
            static_cast<uint8_t*>(ai.map_info.staging.mapped_ptr), data, bytes);
    }else{
        if(!allocator) {
            ai.ew.report(ave_vk_image_upload,
                "Either provide map_info.staging or set CreateImageInfo::allocator for an internal one.");
            return false;
        }
        transient = std::make_unique<VMABuffer>(CreateVMABufferInfo{
            .allocator = allocator,
            .size = bytes,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .host_access = BufferHostAccess::SequentialWrite,
        });
        if(!*transient) {
            ai.ew.report(ave_vk_image_upload, "Failed to create internal staging buffer.");
            return false;
        }
        auto mapped = transient->map();
        if(!mapped) {
            ai.ew.report(ave_vk_image_upload, "Failed to map internal staging buffer.");
            return false;
        }
        mapped.memcpy(0, data, bytes);
        mapped.cancel_auto_upload();
        if(!mapped.flush(ai.ew)) return false;
        src = transient->get_system_handle();
        src_offset = 0;
    }

    UploadToImageInfo copy {};
    copy.src = src;
    copy.src_offset = src_offset;
    copy.dst = image;
    copy.subresource = VkImageSubresourceLayers{ aspect_mask, 0, 0, 1 };
    copy.extent = extent;
    copy.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    copy.final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    copy.ew = ai.ew;

    auto ticket = upload_context->submit_copy_to_image(copy);
    if(!ticket) return false;
    return ticket.wait(std::numeric_limits<uint64_t>::max(), ai.ew);
}

void Image::destroy() noexcept {
    // 视图必须先于 image 销毁。
    const bool had_default_view = static_cast<bool>(default_view);
    if(default_view) {
        default_view->destroy();
        default_view.reset();
    }

    if(device && device->get_system_handle() != VK_NULL_HANDLE &&
       (owns_image || vma_binding || had_default_view)) {
        vkDeviceWaitIdle(device->get_system_handle());
    }

    if(device && device->get_system_handle() != VK_NULL_HANDLE) {
        const auto handle = device->get_system_handle();
        const auto vk_allocator = device->get_instance()->get_vk_allocator();
        // VMA 后端：ImageBinding 析构负责释放 VMA 显存。
        vma_binding.reset();
        // swapchain 持有的 image 不归本对象销毁。
        if(!swapchain && image != VK_NULL_HANDLE) {
            vkDestroyImage(handle, image, vk_allocator);
        }
        if(memory != VK_NULL_HANDLE) {
            vkFreeMemory(handle, memory, vk_allocator);
        }
    }
    // swapchain 拥有的 image：既无 memory 也不归本对象销毁。

    image = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;
    format = VK_FORMAT_UNDEFINED;
    extent = {};
    mip_levels = 1;
    array_layers = 1;
    image_type = VK_IMAGE_TYPE_2D;
    image_flags = 0;
    usage = 0;
    aspect_mask = 0;
    owns_image = false;
    swapchain.reset();
    allocator.reset();
    vma_binding.reset();
    device.reset();
}

const std::shared_ptr<Device>& Image::get_device() const noexcept { return device; }
const std::shared_ptr<Swapchain>& Image::get_swapchain() const noexcept {
    return swapchain;
}
const std::shared_ptr<VMAAllocator>& Image::get_allocator() const noexcept {
    return allocator;
}
VkImage Image::get_system_handle() const noexcept { return image; }
VkDeviceMemory Image::get_memory() const noexcept { return memory; }
VkFormat Image::get_format() const noexcept { return format; }
VkExtent3D Image::get_extent() const noexcept { return extent; }
VkImageUsageFlags Image::get_usage() const noexcept { return usage; }
VkImageAspectFlags Image::get_aspect_mask() const noexcept { return aspect_mask; }
alib6::u32 Image::get_mip_levels() const noexcept { return mip_levels; }
alib6::u32 Image::get_array_layers() const noexcept { return array_layers; }
Image::operator bool() const noexcept { return image != VK_NULL_HANDLE; }

VkImageView Image::get_image_view() const noexcept {
    return default_view ? default_view->get_image_view() : VK_NULL_HANDLE;
}

std::shared_ptr<ImageView> Image::get_default_view() const noexcept {
    return default_view;
}

ImageView::~ImageView() { destroy(); }

bool ImageView::initialize(CreateImageViewInfo ci) {
    auto parent_sp = ci.image;
    if(!parent_sp || !*parent_sp) {
        ci.ew.report(ave_vk_create_image_view,
            "Cannot create an ImageView without a valid Image.");
        return false;
    }

    Image& source = *parent_sp;
    adopt_parent(source);

    auto cfg = ci.view;
    source.resolve_self_view_config(cfg);

    VkImageViewCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext = cfg.next;
    info.flags = cfg.flags;
    info.image = source.image;
    info.viewType = cfg.view_type;
    info.format = cfg.format;
    info.components = cfg.components;
    info.subresourceRange = cfg.subresource_range;

    const auto code = vkCreateImageView(
        device->get_system_handle(), &info,
        device->get_instance()->get_vk_allocator(), &view);
    if(code != VK_SUCCESS) {
        ci.ew.report(ave_vk_create_image_view,
            "Failed to create Vulkan ImageView ({}).", int(code));
        destroy();
        return false;
    }

    format = cfg.format;
    aspect_mask = cfg.subresource_range.aspectMask;
    parent = std::move(parent_sp);
    return true;
}

std::shared_ptr<ImageView> ImageView::create(CreateImageViewInfo ci) {
    auto result = std::shared_ptr<ImageView>(new ImageView());
    if(!result->initialize(std::move(ci))) return {};
    return result;
}

void ImageView::destroy() noexcept {
    if(view != VK_NULL_HANDLE && device &&
       device->get_system_handle() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device->get_system_handle());
        vkDestroyImageView(
            device->get_system_handle(), view,
            device->get_instance()->get_vk_allocator());
    }
    view = VK_NULL_HANDLE;
    parent.reset();
}

VkImageView ImageView::get_image_view() const noexcept { return view; }

const std::weak_ptr<Image>& ImageView::get_parent() const noexcept {
    return parent;
}

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
} // namespace ave
