module;
#include <vulkan/vulkan.h>

module ave.render;

import std;
import alib6;
import ave.ecode;
import :sampler;

namespace ave {

bool Sampler::initialize(CreateSamplerInfo ci) {
    if(!ci.device || ci.device->get_system_handle() == VK_NULL_HANDLE) {
        ci.ew.report(ave_vk_create_sampler,
            "Cannot create Sampler without a valid Device.");
        return false;
    }
    if(ci.anisotropy_enable) {
        VkPhysicalDeviceFeatures supported {};
        vkGetPhysicalDeviceFeatures(
            ci.device->get_physical_device(), &supported);
        if(!supported.samplerAnisotropy) {
            ci.ew.report(ave_vk_create_sampler,
                "Anisotropic filtering requires the samplerAnisotropy device feature.");
            return false;
        }
        if(ci.max_anisotropy < 1.0f) {
            ci.ew.report(ave_vk_create_sampler,
                "max_anisotropy must be >= 1.0 when anisotropy is enabled.");
            return false;
        }
    }

    VkSamplerCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.pNext = ci.next;
    info.flags = ci.flags;
    info.magFilter = ci.mag_filter;
    info.minFilter = ci.min_filter;
    info.mipmapMode = ci.mipmap_mode;
    info.addressModeU = ci.address_mode_u;
    info.addressModeV = ci.address_mode_v;
    info.addressModeW = ci.address_mode_w;
    info.mipLodBias = ci.mip_lod_bias;
    info.anisotropyEnable = ci.anisotropy_enable ? VK_TRUE : VK_FALSE;
    info.maxAnisotropy = ci.max_anisotropy;
    info.compareEnable = ci.compare_enable ? VK_TRUE : VK_FALSE;
    info.compareOp = ci.compare_op;
    info.minLod = ci.min_lod;
    info.maxLod = ci.max_lod;
    info.borderColor = ci.border_color;
    info.unnormalizedCoordinates = ci.unnormalized_coordinates ? VK_TRUE : VK_FALSE;

    device = ci.device;
    const auto code = vkCreateSampler(
        device->get_system_handle(), &info,
        device->get_instance()->get_vk_allocator(), &sampler);
    if(code != VK_SUCCESS) {
        ci.ew.report(ave_vk_create_sampler,
            "Failed to create Vulkan Sampler ({}).", int(code));
        device.reset();
        return false;
    }
    return true;
}

Sampler::~Sampler() { destroy(); }

std::shared_ptr<Sampler> Sampler::create(CreateSamplerInfo ci) {
    auto result = std::shared_ptr<Sampler>(new Sampler());
    if(!result->initialize(std::move(ci))) return {};
    return result;
}

void Sampler::destroy() noexcept {
    if(sampler != VK_NULL_HANDLE && device &&
       device->get_system_handle() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device->get_system_handle());
        vkDestroySampler(
            device->get_system_handle(), sampler,
            device->get_instance()->get_vk_allocator());
    }
    sampler = VK_NULL_HANDLE;
    device.reset();
}

const std::shared_ptr<Device>& Sampler::get_device() const noexcept {
    return device;
}

VkSampler Sampler::get_system_handle() const noexcept { return sampler; }

Sampler::operator bool() const noexcept { return sampler != VK_NULL_HANDLE; }

} // namespace ave
