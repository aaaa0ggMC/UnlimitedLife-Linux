module;
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

module ave.render;

import std;
import alib6;
import ave.ecode;
import :buffer;

#include "buffer_detail.inl"

namespace ave {
struct VMAAllocator::Impl {
    std::shared_ptr<Device> device;
    VmaAllocator handle { VK_NULL_HANDLE };
    bool buffer_device_address { false };
    ~Impl() { if(handle) vmaDestroyAllocator(handle); }
};

VMAAllocator::VMAAllocator(std::shared_ptr<Impl> value) : impl(std::move(value)) {}
VMAAllocator::~VMAAllocator() = default;

const std::shared_ptr<Device>& VMAAllocator::get_device() const noexcept {
    return impl->device;
}

std::shared_ptr<VMAAllocator> VMAAllocator::create_shared(CreateVMAAllocatorInfo ci) {
    if(!ci.device || !ci.device->get_system_handle()) {
        ci.ew.report(ave_vk_create_buffer, "VMAAllocator requires a valid Device.");
        return {};
    }
    VkPhysicalDeviceProperties physical {};
    vkGetPhysicalDeviceProperties(ci.device->get_physical_device(), &physical);
    const auto instance_version = ci.device->get_instance()->get_api_version().to();
    if(ci.api_version < VK_API_VERSION_1_0 ||
       ci.api_version > std::min(instance_version, physical.apiVersion) ||
       VK_API_VERSION_VARIANT(ci.api_version) != 0 ||
       VK_API_VERSION_MAJOR(ci.api_version) != 1 ||
       VK_API_VERSION_MINOR(ci.api_version) > 3) {
        ci.ew.report(ave_vk_create_buffer, "Unsupported VMAAllocator API version (supports Vulkan 1.0-1.3).");
        return {};
    }
    auto impl = std::make_shared<Impl>();
    impl->device = ci.device;
    impl->buffer_device_address = ci.buffer_device_address;
    VmaAllocatorCreateInfo vk {};
    vk.instance = ci.device->get_instance()->get_system_handle();
    vk.physicalDevice = ci.device->get_physical_device();
    vk.device = ci.device->get_system_handle();
    vk.vulkanApiVersion = ci.api_version;
    vk.pAllocationCallbacks = ci.device->get_instance()->get_vk_allocator();
    if(ci.buffer_device_address) vk.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    const auto code = vmaCreateAllocator(&vk, &impl->handle);
    if(code != VK_SUCCESS) {
        ci.ew.report(ave_vk_create_buffer, "Failed to create VMAAllocator ({}).", int(code));
        return {};
    }
    return std::shared_ptr<VMAAllocator>(new VMAAllocator(std::move(impl)));
}

struct VmaMemoryPolicy::State {
    detail::BufferProperties info;
    std::shared_ptr<VMAAllocator> owner;
    VmaAllocation allocation { VK_NULL_HANDLE };
    BufferHostAccess host_access { BufferHostAccess::DeviceOnly };
    VmaVirtualBlock regions { VK_NULL_HANDLE };
    std::mutex region_mutex;

    struct Region {
        std::shared_ptr<State> state;
        VmaVirtualAllocation allocation { VK_NULL_HANDLE };
        ~Region() {
            if(allocation) {
                std::lock_guard lock(state->region_mutex);
                vmaVirtualFree(state->regions, allocation);
            }
        }
    };

    ~State() {
        if(regions) vmaDestroyVirtualBlock(regions);
        if(owner && (info.buffer || allocation))
            vmaDestroyBuffer(owner->impl->handle, info.buffer, allocation);
    }
};

std::shared_ptr<VmaMemoryPolicy::State> VmaMemoryPolicy::create(CreateInfo ci) {
    if(!ci.allocator) {
        ci.ew.report(ave_vk_create_buffer, "VMABuffer requires a VMAAllocator.");
        return {};
    }
    if(ci.persistent_mapping && ci.host_access == BufferHostAccess::DeviceOnly) {
        ci.ew.report(ave_vk_create_buffer, "Persistent mapping requires CPU host access.");
        return {};
    }
    if((ci.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) &&
       !ci.allocator->impl->buffer_device_address) {
        ci.ew.report(ave_vk_create_buffer, "Allocator must enable buffer device address support.");
        return {};
    }
    auto state = std::make_shared<State>();
    state->owner = ci.allocator;
    state->host_access = ci.host_access;
    VkBufferCreateInfo vk {};
    std::vector<alib6::u32> families;
    if(!detail::prepare_buffer(ci, ci.allocator->get_device(), state->info, vk, families, false))
        return {};
    VmaAllocationCreateInfo allocation {};
    switch(ci.memory_preference) {
    case BufferMemoryPreference::Automatic: allocation.usage = VMA_MEMORY_USAGE_AUTO; break;
    case BufferMemoryPreference::PreferDevice: allocation.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE; break;
    case BufferMemoryPreference::PreferHost: allocation.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST; break;
    }
    switch(ci.host_access) {
    case BufferHostAccess::DeviceOnly: break;
    case BufferHostAccess::SequentialWrite:
        allocation.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        break;
    case BufferHostAccess::RandomAccess:
        allocation.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        break;
    }
    allocation.requiredFlags = ci.required_memory_properties;
    allocation.preferredFlags = ci.preferred_memory_properties;
    if(ci.persistent_mapping) allocation.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    if(ci.dedicated_allocation) allocation.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    VmaAllocationInfo info {};
    const auto code = vmaCreateBuffer(ci.allocator->impl->handle, &vk, &allocation,
                                      &state->info.buffer, &state->allocation, &info);
    if(code != VK_SUCCESS) {
        ci.ew.report(ave_vk_create_buffer, "Failed to create VMABuffer ({}).", int(code));
        return {};
    }
    state->info.memory = info.deviceMemory;
    state->info.memory_offset = info.offset;
    state->info.allocation_size = info.size;
    vmaGetAllocationMemoryProperties(ci.allocator->impl->handle, state->allocation,
                                     &state->info.memory_properties);
    return state;
}

const detail::BufferProperties& VmaMemoryPolicy::properties(const State& s) noexcept {
    return s.info;
}

detail::BufferRegion VmaMemoryPolicy::allocate_region(const std::shared_ptr<State>& state,
    VkDeviceSize bytes, AllocateBufferInfo ai) {
    if(!state) {
        ai.ew.report(ave_vk_create_buffer, "Cannot allocate from an uninitialized VMABuffer.");
        return {};
    }
    if(bytes == 0 || !std::has_single_bit(ai.alignment)) {
        ai.ew.report(ave_vk_create_buffer, "Allocation size must be nonzero and alignment a power of two.");
        return {};
    }
    const auto atom = state->info.device->get_non_coherent_atom_size();
    if(bytes > state->info.size) {
        ai.ew.report(ave_vk_create_buffer, "VMABuffer capacity is too small for this allocation.");
        return {};
    }
    auto lease = std::make_shared<State::Region>();
    lease->state = state;
    std::lock_guard lock(state->region_mutex);
    if(!state->regions) {
        VmaVirtualBlockCreateInfo ci {};
        ci.size = state->info.size;
        const auto code = vmaCreateVirtualBlock(&ci, &state->regions);
        if(code != VK_SUCCESS) {
            ai.ew.report(ave_vk_create_buffer, "Failed to create Buffer region allocator ({}).", int(code));
            return {};
        }
    }
    VmaVirtualAllocationCreateInfo ci {};
    // Aligned starts alone prevent two non-overlapping regions from sharing an atom.
    // Keep the exact size so the final partial atom of a logical Buffer remains usable.
    ci.size = bytes;
    ci.alignment = std::max(ai.alignment, atom);
    VkDeviceSize offset = 0;
    const auto code = vmaVirtualAllocate(state->regions, &ci, &lease->allocation, &offset);
    if(code != VK_SUCCESS) {
        lease->allocation = VK_NULL_HANDLE;
        ai.ew.report(ave_vk_create_buffer, "No suitable free Buffer region ({}).", int(code));
        return {};
    }
    return {std::move(lease), offset};
}

VkResult VmaMemoryPolicy::map(State& s, void** ptr) {
    if(s.host_access == BufferHostAccess::DeviceOnly) return VK_ERROR_MEMORY_MAP_FAILED;
    // Each mapping owns one VMA reference, including persistently mapped allocations.
    return vmaMapMemory(s.owner->impl->handle, s.allocation, ptr);
}

void VmaMemoryPolicy::unmap(State& s) noexcept {
    vmaUnmapMemory(s.owner->impl->handle, s.allocation);
}

VkResult VmaMemoryPolicy::flush(State& s, VkDeviceSize offset, VkDeviceSize size) {
    return vmaFlushAllocation(s.owner->impl->handle, s.allocation, offset, size);
}

VkResult VmaMemoryPolicy::invalidate(State& s, VkDeviceSize offset, VkDeviceSize size) {
    return vmaInvalidateAllocation(s.owner->impl->handle, s.allocation, offset, size);
}
}
