module;
#include <vulkan/vulkan.h>
#include <alib6/debug.h>

module ave.render;

import std;
import alib6;
import ave.ecode;
import :buffer;

#include "buffer_detail.inl"

namespace ave {
struct NativeMemoryPolicy::State {
    detail::BufferProperties info;
    std::mutex mutex;
    void* mapped { nullptr };
    size_t map_count { 0 };

    ~State() {
        if(!info.device) return;
        const auto device = info.device->get_system_handle();
        const auto callbacks = info.device->get_instance()->get_vk_allocator();
        if(mapped) vkUnmapMemory(device, info.memory);
        if(info.buffer) vkDestroyBuffer(device, info.buffer, callbacks);
        if(info.memory) vkFreeMemory(device, info.memory, callbacks);
    }

    VkResult sync(VkDeviceSize offset, VkDeviceSize size, bool invalidate) {
        std::lock_guard lock(mutex);
        if(!mapped || offset >= info.allocation_size ||
           size == 0 || size > info.allocation_size - offset)
            return VK_ERROR_MEMORY_MAP_FAILED;
        if(info.memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) return VK_SUCCESS;
        const auto atom = info.device->get_non_coherent_atom_size();
        const auto begin = offset / atom * atom;
        const auto end = offset + size;
        const auto remainder = end % atom;
        const auto padding = remainder ? atom - remainder : 0;
        const auto aligned_end = end + std::min(padding, info.allocation_size - end);
        VkMappedMemoryRange range {};
        range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory = info.memory;
        range.offset = begin;
        range.size = aligned_end - begin;
        return invalidate
            ? vkInvalidateMappedMemoryRanges(info.device->get_system_handle(), 1, &range)
            : vkFlushMappedMemoryRanges(info.device->get_system_handle(), 1, &range);
    }
};

std::shared_ptr<NativeMemoryPolicy::State> NativeMemoryPolicy::create(CreateInfo ci) {
    auto state = std::make_shared<State>();
    VkBufferCreateInfo vk {};
    std::vector<alib6::u32> families;
    if(!detail::prepare_buffer(ci, ci.device, state->info, vk, families, true)) return {};
    const auto device = ci.device->get_system_handle();
    const auto callbacks = ci.device->get_instance()->get_vk_allocator();
    auto code = vkCreateBuffer(device, &vk, callbacks, &state->info.buffer);
    if(code != VK_SUCCESS) {
        ci.ew.report(ave_vk_create_buffer, "Failed to create Buffer ({}).", int(code));
        return {};
    }
    VkMemoryRequirements requirements {};
    vkGetBufferMemoryRequirements(device, state->info.buffer, &requirements);
    VkPhysicalDeviceMemoryProperties memory {};
    vkGetPhysicalDeviceMemoryProperties(ci.device->get_physical_device(), &memory);
    std::optional<uint32_t> type;
    for(uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
        if((requirements.memoryTypeBits & (1u << i)) &&
           (memory.memoryTypes[i].propertyFlags & ci.memory_properties) == ci.memory_properties) {
            type = i;
            break;
        }
    }
    if(!type) {
        ci.ew.report(ave_vk_create_buffer, "No compatible Buffer memory type.");
        return {};
    }
    VkMemoryAllocateInfo allocation {};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.pNext = ci.memory_allocate_next;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = *type;
    code = vkAllocateMemory(device, &allocation, callbacks, &state->info.memory);
    if(code == VK_SUCCESS) code = vkBindBufferMemory(device, state->info.buffer, state->info.memory, 0);
    if(code != VK_SUCCESS) {
        ci.ew.report(ave_vk_create_buffer, "Failed to allocate/bind Buffer ({}).", int(code));
        return {};
    }
    state->info.allocation_size = requirements.size;
    state->info.memory_properties = memory.memoryTypes[*type].propertyFlags;
    return state;
}

const detail::BufferProperties& NativeMemoryPolicy::properties(const State& s) noexcept {
    return s.info;
}

VkResult NativeMemoryPolicy::map(State& s, void** ptr) {
    std::lock_guard lock(s.mutex);
    if(!s.map_count) {
        const auto code = vkMapMemory(s.info.device->get_system_handle(), s.info.memory,
                                      0, VK_WHOLE_SIZE, 0, &s.mapped);
        if(code != VK_SUCCESS) return code;
    }
    ++s.map_count;
    *ptr = s.mapped;
    return VK_SUCCESS;
}

void NativeMemoryPolicy::unmap(State& s) noexcept {
    std::lock_guard lock(s.mutex);
    if(s.map_count && --s.map_count == 0) {
        vkUnmapMemory(s.info.device->get_system_handle(), s.info.memory);
        s.mapped = nullptr;
    }
}

VkResult NativeMemoryPolicy::flush(State& s, VkDeviceSize offset, VkDeviceSize size) {
    return s.sync(offset, size, false);
}
VkResult NativeMemoryPolicy::invalidate(State& s, VkDeviceSize offset, VkDeviceSize size) {
    return s.sync(offset, size, true);
}
}
