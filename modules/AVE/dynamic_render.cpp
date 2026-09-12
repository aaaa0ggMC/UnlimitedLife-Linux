module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>
#include <alib6/debug.h>

module ave.render;

import std;
import alib6;
import ave.ecode;
import :dynamic_render;
import :pipeline;

namespace ave {

WithDynamicRenderInput::WithDynamicRenderInput(
    std::shared_ptr<Device> target_device,
    std::shared_ptr<Swapchain> target_swapchain,
    std::vector<std::shared_ptr<Image>> target_images
)
    : device(std::move(target_device))
    , swapchain(std::move(target_swapchain))
    , images(std::move(target_images))
{}

const std::shared_ptr<Device>& WithDynamicRenderInput::get_device() const noexcept {
    return device;
}

const std::shared_ptr<Swapchain>& WithDynamicRenderInput::get_swapchain() const noexcept {
    return swapchain;
}

VkSurfaceFormatKHR WithDynamicRenderInput::get_surface_format() const noexcept {
    return swapchain ? swapchain->get_surface_format() : VkSurfaceFormatKHR {};
}

VkExtent2D WithDynamicRenderInput::get_extent() const noexcept {
    return swapchain ? swapchain->get_extent() : VkExtent2D {};
}

const std::vector<std::shared_ptr<Image>>& WithDynamicRenderInput::get_images() const noexcept {
    return images;
}

void default_configure_dynamic_render(
    WithDynamicRenderInput& input,
    CreateDynamicRenderInfo& ci
) {
    ci.color_attachment_formats = { input.get_surface_format().format };
    ci.image_dependencies.clear();
    for(const auto& img : input.get_images()) {
        if(img && (img->get_usage() & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
            ci.depth_attachment_format = img->get_format();
            if(img->get_aspect_mask() & VK_IMAGE_ASPECT_STENCIL_BIT) {
                ci.stencil_attachment_format = img->get_format();
            }
            ci.image_dependencies.push_back(img);
            break;
        }
    }

    VkClearValue color_clear {};
    color_clear.color = {{ 0.02f, 0.02f, 0.03f, 1.0f }};
    ci.default_clear_values = { color_clear };
    if(ci.depth_attachment_format != VK_FORMAT_UNDEFINED) {
        VkClearValue depth_clear {};
        depth_clear.depthStencil = { 1.0f, 0 };
        ci.default_clear_values.push_back(depth_clear);
    }
}

DynamicRender::~DynamicRender() {
    destroy();
}

std::shared_ptr<DynamicRender> DynamicRender::create(CreateDynamicRenderInfo ci) {
    auto render = std::shared_ptr<DynamicRender>(new DynamicRender());
    if(!render->initialize(std::move(ci))) {
        return nullptr;
    }
    return render;
}

void DynamicRender::destroy() noexcept {
    swapchain.reset();
    image_dependencies.clear();
    color_attachment_formats.clear();
    depth_attachment_format = VK_FORMAT_UNDEFINED;
    stencil_attachment_format = VK_FORMAT_UNDEFINED;
    default_clear_values.clear();
}

bool DynamicRender::initialize(CreateDynamicRenderInfo ci) {
    if(!ci.swapchain || ci.swapchain->get_system_handle() == VK_NULL_HANDLE) {
        ci.ew.report(ave_vk_create_swapchain,
            "Cannot create DynamicRender without a valid Swapchain.");
        return false;
    }
    const auto dev = ci.swapchain->get_device();
    if(!dev || !dev->supports_dynamic_rendering()) {
        ci.ew.report(ave_vk_create_device,
            "Cannot create DynamicRender: underlying Device does not support dynamic rendering.");
        return false;
    }

    swapchain = std::move(ci.swapchain);
    image_dependencies = std::move(ci.image_dependencies);
    color_attachment_formats = std::move(ci.color_attachment_formats);
    depth_attachment_format = ci.depth_attachment_format;
    stencil_attachment_format = ci.stencil_attachment_format;
    default_clear_values = std::move(ci.default_clear_values);
    return true;
}

const std::shared_ptr<Swapchain>& DynamicRender::get_swapchain() const noexcept {
    return swapchain;
}

const std::shared_ptr<Device>& DynamicRender::get_device() const noexcept {
    static const std::shared_ptr<Device> empty;
    return swapchain ? swapchain->get_device() : empty;
}

const std::vector<std::shared_ptr<Image>>& DynamicRender::get_image_dependencies() const noexcept {
    return image_dependencies;
}

const std::vector<VkFormat>& DynamicRender::get_color_attachment_formats() const noexcept {
    return color_attachment_formats;
}

VkFormat DynamicRender::get_depth_attachment_format() const noexcept {
    return depth_attachment_format;
}

VkFormat DynamicRender::get_stencil_attachment_format() const noexcept {
    return stencil_attachment_format;
}

const std::vector<VkClearValue>& DynamicRender::get_default_clear_values() const noexcept {
    return default_clear_values;
}

DynamicRender::operator bool() const noexcept {
    return swapchain != nullptr;
}

}
