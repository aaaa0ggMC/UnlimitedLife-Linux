module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

module ave.render;

import std;
import alib6;
import ave.ecode;
import :descriptor;

namespace ave {

WithDescriptorPoolInput::WithDescriptorPoolInput(
    std::shared_ptr<Device> target_device,
    alib6::u32 target_frame_count
)
:device(std::move(target_device))
,frame_count(target_frame_count){}

const std::shared_ptr<Device>& WithDescriptorPoolInput::get_device() const noexcept {
    return device;
}

alib6::u32 WithDescriptorPoolInput::get_frame_count() const noexcept {
    return frame_count;
}

void default_configure_descriptor_pool(
    WithDescriptorPoolInput& input,
    CreateDescriptorPoolInfo& ci
) {
    ci.device = input.get_device();
    alib6::u32 max_sets = ci.max_sets ? ci.max_sets : input.get_frame_count();
    if(max_sets == 0) max_sets = 1;
    ci.max_sets = max_sets;
    if(ci.pool_sizes.empty()) {
        ci.pool_sizes.push_back(DescriptorPoolSize::uniform_buffer(max_sets));
    }
}

bool DescriptorPool::initialize(CreateDescriptorPoolInfo ci) {
    if(!ci.device || ci.device->get_system_handle() == VK_NULL_HANDLE) {
        ci.ew.report(ave_vk_create_descriptor_pool,
            "Cannot create DescriptorPool without a valid Device.");
        return false;
    }
    if(ci.max_sets == 0 || ci.pool_sizes.empty()) {
        ci.ew.report(ave_vk_create_descriptor_pool,
            "Cannot create DescriptorPool with zero max_sets or empty pool sizes.");
        return false;
    }

    auto vk_sizes = ci.pool_sizes
        | std::views::transform([](const DescriptorPoolSize& size) {
              return static_cast<VkDescriptorPoolSize>(size);
          })
        | std::ranges::to<std::vector>();

    VkDescriptorPoolCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.flags = ci.flags;
    info.maxSets = ci.max_sets;
    info.poolSizeCount = static_cast<uint32_t>(vk_sizes.size());
    info.pPoolSizes = vk_sizes.data();

    device = ci.device;
    const VkResult code = vkCreateDescriptorPool(
        device->get_system_handle(), &info,
        device->get_instance()->get_vk_allocator(), &pool);
    if(code != VK_SUCCESS) {
        ci.ew.report(ave_vk_create_descriptor_pool,
            "Failed to create Vulkan DescriptorPool ({}).", static_cast<int>(code));
        device.reset();
        return false;
    }
    return true;
}

DescriptorPool::~DescriptorPool() { destroy(); }

std::shared_ptr<DescriptorPool> DescriptorPool::create(CreateDescriptorPoolInfo ci) {
    auto result = std::shared_ptr<DescriptorPool>(new DescriptorPool());
    if(!result->initialize(std::move(ci))) return {};
    return result;
}

void DescriptorPool::destroy() noexcept {
    if(pool != VK_NULL_HANDLE && device) {
        // pool 内分配出的 descriptor set 可能仍被已提交未执行的命令引用，
        // 直接销毁会触发 VUID-vkDestroyDescriptorPool-descriptorPool-00303，
        // 因此先等待设备空闲（与 CommandPool 的销毁策略一致）。
        vkDeviceWaitIdle(device->get_system_handle());
        vkDestroyDescriptorPool(device->get_system_handle(), pool,
            device->get_instance()->get_vk_allocator());
    }
    pool = VK_NULL_HANDLE;
    device.reset();
}

const std::shared_ptr<Device>& DescriptorPool::get_device() const noexcept { return device; }
VkDescriptorPool DescriptorPool::get_system_handle() const noexcept { return pool; }
DescriptorPool::operator bool() const noexcept { return pool != VK_NULL_HANDLE; }

VkDescriptorSet DescriptorPool::allocate_descriptor_set(
    VkDescriptorSetLayout layout,
    alib6::ErrorWrapper ew
) {
    if(pool == VK_NULL_HANDLE || !device) {
        ew.report(ave_vk_create_descriptor_pool,
            "Cannot allocate descriptor set from an invalid DescriptorPool.");
        return VK_NULL_HANDLE;
    }
    if(layout == VK_NULL_HANDLE) {
        ew.report(ave_vk_create_descriptor_pool,
            "Cannot allocate descriptor set with a null DescriptorSetLayout.");
        return VK_NULL_HANDLE;
    }

    VkDescriptorSetAllocateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &layout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    const VkResult code = vkAllocateDescriptorSets(
        device->get_system_handle(), &info, &set);
    if(code != VK_SUCCESS) {
        ew.report(ave_vk_create_descriptor_pool,
            "Failed to allocate Vulkan DescriptorSet ({}).", static_cast<int>(code));
        return VK_NULL_HANDLE;
    }
    return set;
}

std::vector<VkDescriptorSet> DescriptorPool::allocate_descriptor_sets(
    std::span<const VkDescriptorSetLayout> layouts,
    alib6::ErrorWrapper ew
) {
    if(pool == VK_NULL_HANDLE || !device) {
        ew.report(ave_vk_create_descriptor_pool,
            "Cannot allocate descriptor sets from an invalid DescriptorPool.");
        return {};
    }
    if(layouts.empty()) return {};

    auto result = std::vector<VkDescriptorSet>(layouts.size(), VK_NULL_HANDLE);

    VkDescriptorSetAllocateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = pool;
    info.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    info.pSetLayouts = layouts.data();

    const VkResult code = vkAllocateDescriptorSets(
        device->get_system_handle(), &info, result.data());
    if(code != VK_SUCCESS) {
        ew.report(ave_vk_create_descriptor_pool,
            "Failed to allocate Vulkan DescriptorSets ({}).", static_cast<int>(code));
        return {};
    }
    return result;
}

void DescriptorPool::write_buffer(
    VkDescriptorSet dst_set,
    alib6::u32 dst_binding,
    VkDescriptorType descriptor_type,
    VkBuffer buffer,
    VkDeviceSize offset,
    VkDeviceSize range,
    alib6::u32 dst_array_element
) {
    if(!device) return;

    VkDescriptorBufferInfo buffer_info {
        .buffer = buffer,
        .offset = offset,
        .range = range
    };
    VkWriteDescriptorSet write {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = dst_set;
    write.dstBinding = dst_binding;
    write.dstArrayElement = dst_array_element;
    write.descriptorCount = 1;
    write.descriptorType = descriptor_type;
    write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(device->get_system_handle(), 1, &write, 0, nullptr);
}

void DescriptorPool::write_image(
    VkDescriptorSet dst_set,
    alib6::u32 dst_binding,
    VkSampler sampler,
    VkImageView view,
    VkImageLayout image_layout,
    alib6::u32 dst_array_element
) {
    if(!device) return;

    VkDescriptorImageInfo image_info {
        .sampler = sampler,
        .imageView = view,
        .imageLayout = image_layout
    };
    VkWriteDescriptorSet write {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = dst_set;
    write.dstBinding = dst_binding;
    write.dstArrayElement = dst_array_element;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(device->get_system_handle(), 1, &write, 0, nullptr);
}

} // namespace ave
