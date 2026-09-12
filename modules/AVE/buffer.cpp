module;
#include <vulkan/vulkan.h>
#include <alib6/debug.h>

module ave.render;

import std;
import alib6;
import ave.ecode;
import :buffer;

namespace ave {
namespace {
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

// ==========================================
// BufferData::ByteProxy 实现
// ==========================================

BufferData::ByteProxy& BufferData::ByteProxy::operator=(uint8_t val) noexcept {
    *ptr = val;
    if(parent) parent->mark_range_dirty(offset, 1);
    return *this;
}

BufferData::ByteProxy& BufferData::ByteProxy::operator=(const ByteProxy& other) noexcept {
    if(this == &other) return *this;
    return *this = static_cast<uint8_t>(other);
}

// ==========================================
// BufferData 实现
// ==========================================

BufferData::BufferData(
    std::shared_ptr<Device> dev,
    VkDeviceMemory mem,
    void* ptr,
    VkDeviceSize offset,
    VkDeviceSize size,
    VkDeviceSize capacity,
    VkDeviceSize alloc_size,
    VkDeviceSize c_size,
    bool coherent
) : device(std::move(dev)),
    memory(mem),
    mapped_ptr(ptr),
    map_offset(offset),
    map_size(size),
    mapped_capacity(capacity),
    allocation_size(alloc_size),
    chunk_size(c_size),
    chunk_count(c_size > 0 ? static_cast<alib6::usize>((size + c_size - 1) / c_size) : 0),
    is_coherent(coherent),
    auto_upload(true),
    dirty_mask(chunk_count) {
    if(chunk_count > 0) {
        dirty_mask.ensure(chunk_count);
        dirty_mask.fill(false);
    }
}

BufferData::~BufferData() noexcept {
    if(mapped_ptr) {
        if(auto_upload) {
            upload();
        }
        if(device && device->get_system_handle() != VK_NULL_HANDLE && memory != VK_NULL_HANDLE) {
            vkUnmapMemory(device->get_system_handle(), memory);
        }
        mapped_ptr = nullptr;
    }
}

BufferData::BufferData(BufferData&& other) noexcept
    : device(std::move(other.device)),
      memory(other.memory),
      mapped_ptr(other.mapped_ptr),
      map_offset(other.map_offset),
      map_size(other.map_size),
      mapped_capacity(other.mapped_capacity),
      allocation_size(other.allocation_size),
      chunk_size(other.chunk_size),
      chunk_count(other.chunk_count),
      is_coherent(other.is_coherent),
      auto_upload(other.auto_upload),
      dirty_mask(std::move(other.dirty_mask)) {
    other.mapped_ptr = nullptr;
    other.auto_upload = false;
    other.memory = VK_NULL_HANDLE;
}

BufferData& BufferData::operator=(BufferData&& other) noexcept {
    if(this == &other) return *this;
    if(mapped_ptr) {
        if(auto_upload) upload();
        if(device && device->get_system_handle() != VK_NULL_HANDLE && memory != VK_NULL_HANDLE) {
            vkUnmapMemory(device->get_system_handle(), memory);
        }
    }

    device = std::move(other.device);
    memory = other.memory;
    mapped_ptr = other.mapped_ptr;
    map_offset = other.map_offset;
    map_size = other.map_size;
    mapped_capacity = other.mapped_capacity;
    allocation_size = other.allocation_size;
    chunk_size = other.chunk_size;
    chunk_count = other.chunk_count;
    is_coherent = other.is_coherent;
    auto_upload = other.auto_upload;
    dirty_mask = std::move(other.dirty_mask);

    other.mapped_ptr = nullptr;
    other.auto_upload = false;
    other.memory = VK_NULL_HANDLE;
    return *this;
}

void BufferData::mark_range_dirty(alib6::usize byte_offset, alib6::usize byte_count) noexcept {
    if(is_coherent || chunk_size == 0 || chunk_count == 0 || byte_count == 0) return;
    const alib6::usize start_chunk = byte_offset / chunk_size;
    const alib6::usize end_byte = std::min(map_size, static_cast<VkDeviceSize>(byte_offset + byte_count));
    const alib6::usize end_chunk = std::min(chunk_count, (end_byte + chunk_size - 1) / chunk_size);
    for(alib6::usize i = start_chunk; i < end_chunk; ++i) {
        dirty_mask.set(i);
    }
}

void BufferData::memcpy(alib6::usize dst_offset, const void* src, alib6::usize bytes) {
    panic_debug(dst_offset + bytes > map_size, "BufferData::memcpy out of bounds.");
    if(!mapped_ptr || bytes == 0 || !src) return;
    std::memcpy(static_cast<uint8_t*>(mapped_ptr) + dst_offset, src, bytes);
    mark_range_dirty(dst_offset, bytes);
}

BufferData::ByteProxy BufferData::operator[](alib6::usize byte_offset) {
    panic_debug(byte_offset >= map_size, "BufferData::operator[] out of range.");
    return ByteProxy(static_cast<uint8_t*>(mapped_ptr) + byte_offset, this, byte_offset);
}

uint8_t BufferData::operator[](alib6::usize byte_offset) const noexcept {
    panic_debug(byte_offset >= map_size, "BufferData::operator[] out of range.");
    return static_cast<const uint8_t*>(mapped_ptr)[byte_offset];
}

void BufferData::upload() {
    if(is_coherent || !mapped_ptr || dirty_mask.none()) {
        return;
    }

    std::vector<VkMappedMemoryRange> ranges;
    alib6::usize cursor = 0;
    while(cursor < chunk_count) {
        auto next_dirty = dirty_mask.find_next_1(cursor);
        if(!next_dirty || *next_dirty >= chunk_count) break;

        const alib6::usize start_chunk = *next_dirty;
        auto next_clean = dirty_mask.find_next_0(start_chunk, chunk_count);
        const alib6::usize end_chunk = next_clean.value_or(chunk_count);

        VkMappedMemoryRange range {};
        range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory = memory;
        range.offset = map_offset + start_chunk * chunk_size;

        const VkDeviceSize chunk_span_size = (end_chunk - start_chunk) * chunk_size;
        // 遵循 Vulkan VUID-VkMappedMemoryRange-size-01390 规则：
        // size 要么是 nonCoherentAtomSize 的倍数，要么 offset + size == allocation_size
        if(range.offset + chunk_span_size >= map_offset + mapped_capacity) {
            range.size = (map_offset + mapped_capacity > range.offset)
                ? (map_offset + mapped_capacity - range.offset)
                : 0;
        } else {
            range.size = chunk_span_size;
        }

        if(range.size > 0) {
            ranges.push_back(range);
        }

        cursor = end_chunk;
    }

    if(!ranges.empty() && device && device->get_system_handle() != VK_NULL_HANDLE) {
        vkFlushMappedMemoryRanges(
            device->get_system_handle(),
            static_cast<uint32_t>(ranges.size()),
            ranges.data()
        );
        dirty_mask.fill(false);
    }
}

void BufferData::upload_raw(VkDeviceSize raw_offset, VkDeviceSize raw_size) {
    if(is_coherent || !mapped_ptr || !device || device->get_system_handle() == VK_NULL_HANDLE) {
        return;
    }

    const VkDeviceSize actual_offset = map_offset + raw_offset;
    if(actual_offset >= map_offset + mapped_capacity) return;

    VkMappedMemoryRange range {};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = memory;
    range.offset = actual_offset;

    if(raw_size == VK_WHOLE_SIZE) {
        if(map_offset + mapped_capacity >= allocation_size) {
            range.size = VK_WHOLE_SIZE;
        } else {
            range.size = map_offset + mapped_capacity - actual_offset;
        }
    } else {
        const auto atom_size = device->get_non_coherent_atom_size();
        const VkDeviceSize aligned_size = ((raw_size + atom_size - 1) / atom_size) * atom_size;
        if(actual_offset + aligned_size >= map_offset + mapped_capacity) {
            range.size = map_offset + mapped_capacity - actual_offset;
        } else {
            range.size = aligned_size;
        }
    }

    if(range.size > 0 || range.size == VK_WHOLE_SIZE) {
        vkFlushMappedMemoryRanges(device->get_system_handle(), 1, &range);
    }
}

// ==========================================
// Buffer 实现
// ==========================================

Buffer::Buffer(Buffer&& other) noexcept
    : device(std::move(other.device)),
      buffer(other.buffer),
      memory(other.memory),
      size(other.size),
      allocation_size(other.allocation_size),
      usage(other.usage),
      memory_properties(other.memory_properties) {
    other.buffer = VK_NULL_HANDLE;
    other.memory = VK_NULL_HANDLE;
    other.size = 0;
    other.allocation_size = 0;
    other.usage = 0;
    other.memory_properties = 0;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if(this == &other) return *this;
    panic_debug(buffer != VK_NULL_HANDLE, "Cannot move buffer to an object that has a buffer already.");
    if(buffer != VK_NULL_HANDLE) [[unlikely]] {
        destroy();
    }
    device = std::move(other.device);
    buffer = other.buffer;
    memory = other.memory;
    size = other.size;
    allocation_size = other.allocation_size;
    usage = other.usage;
    memory_properties = other.memory_properties;

    other.buffer = VK_NULL_HANDLE;
    other.memory = VK_NULL_HANDLE;
    other.size = 0;
    other.allocation_size = 0;
    other.usage = 0;
    other.memory_properties = 0;
    return *this;
}

bool Buffer::create(CreateBufferInfo ci) {
    if(buffer != VK_NULL_HANDLE) {
        ci.ew.report(
            ave_already_created,
            "Error: Buffer has already been created."
        );
        return false;
    }

    if(!ci.device || ci.device->get_system_handle() == VK_NULL_HANDLE) {
        ci.ew.report(
            ave_vk_create_buffer,
            "Cannot create a Vulkan buffer without a valid logical device."
        );
        return false;
    }

    const auto atom_size = ci.device->get_non_coherent_atom_size();
    VkDeviceSize logical_size = 0;
    VkDeviceSize effective_size = 0;

    // 【优先级判定】：atom_multiply 优先级高于 size
    if(ci.atom_multiply > 0) {
        effective_size = static_cast<VkDeviceSize>(ci.atom_multiply) * atom_size;
        logical_size = effective_size;
    } else {
        if(ci.size == 0) {
            ci.ew.report(
                ave_vk_create_buffer,
                "Cannot create a Vulkan buffer with size 0."
            );
            return false;
        }
        logical_size = ci.size;
        // 底层 VkBuffer 大小自动向上对齐到 nonCoherentAtomSize 保底
        effective_size = ((ci.size + atom_size - 1) / atom_size) * atom_size;
    }

    auto queue_families = ci.queue_family_indices;
    std::ranges::sort(queue_families);
    queue_families.erase(
        std::unique(queue_families.begin(), queue_families.end()),
        queue_families.end()
    );
    if(ci.sharing_mode == VK_SHARING_MODE_CONCURRENT && queue_families.size() < 2) {
        ci.ew.report(
            ave_vk_create_buffer,
            "Concurrent buffer sharing requires at least two distinct queue families."
        );
        return false;
    }

    VkBufferCreateInfo buffer_info {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.pNext = ci.next;
    buffer_info.flags = ci.flags;
    buffer_info.size = effective_size;
    buffer_info.usage = ci.usage;
    buffer_info.sharingMode = ci.sharing_mode;
    buffer_info.queueFamilyIndexCount = ci.sharing_mode == VK_SHARING_MODE_CONCURRENT
        ? static_cast<alib6::u32>(queue_families.size()) : 0;
    buffer_info.pQueueFamilyIndices = ci.sharing_mode == VK_SHARING_MODE_CONCURRENT
        ? queue_families.data() : nullptr;

    device = ci.device;
    const auto handle = device->get_system_handle();
    const auto allocator = device->get_instance()->get_vk_allocator();

    VkResult code = vkCreateBuffer(handle, &buffer_info, allocator, &buffer);
    if(code != VK_SUCCESS) {
        ci.ew.report(
            ave_vk_create_buffer,
            "Failed to create Vulkan Buffer ({}).",
            static_cast<int>(code)
        );
        destroy();
        return false;
    }

    VkMemoryRequirements requirements {};
    vkGetBufferMemoryRequirements(handle, buffer, &requirements);

    const auto memory_type = find_memory_type(
        device->get_physical_device(),
        requirements.memoryTypeBits,
        ci.memory_properties
    );
    if(!memory_type) {
        ci.ew.report(
            ave_vk_create_buffer,
            "No compatible Vulkan memory type exists for the Buffer."
        );
        destroy();
        return false;
    }

    VkMemoryAllocateInfo allocate_info {};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.pNext = ci.memory_allocate_next;
    allocate_info.allocationSize = requirements.size;
    allocate_info.memoryTypeIndex = *memory_type;

    code = vkAllocateMemory(handle, &allocate_info, allocator, &memory);
    if(code == VK_SUCCESS) {
        code = vkBindBufferMemory(handle, buffer, memory, 0);
    }

    if(code != VK_SUCCESS) {
        ci.ew.report(
            ave_vk_create_buffer,
            "Failed to allocate or bind Vulkan Buffer memory ({}).",
            static_cast<int>(code)
        );
        destroy();
        return false;
    }

    size = logical_size;
    allocation_size = requirements.size;
    usage = ci.usage;
    memory_properties = ci.memory_properties;
    return true;
}

BufferData Buffer::map(MapBufferInfo mi) {
    panic_debug(
        !is_host_visible(),
        "Cannot map a buffer whose memory property is not HOST_VISIBLE."
    );

    if(!is_host_visible()) {
        mi.ew.report(
            ave_vk_map_buffer,
            "Cannot map a buffer that is not HOST_VISIBLE."
        );
        return {};
    }

    if(!device || device->get_system_handle() == VK_NULL_HANDLE || memory == VK_NULL_HANDLE) {
        mi.ew.report(
            ave_vk_map_buffer,
            "Cannot map an uninitialized buffer."
        );
        return {};
    }

    if(mi.offset >= size) {
        mi.ew.report(
            ave_vk_map_buffer,
            "Mapping offset ({}) exceeds buffer size ({}).",
            mi.offset,
            size
        );
        return {};
    }

    const VkDeviceSize real_size = (mi.size == VK_WHOLE_SIZE)
        ? (size - mi.offset)
        : mi.size;

    if(mi.offset + real_size > size || real_size == 0) {
        mi.ew.report(
            ave_vk_map_buffer,
            "Invalid mapping range: offset ({}) + size ({}) > buffer size ({}).",
            mi.offset,
            real_size,
            size
        );
        return {};
    }

    const auto atom_size = device->get_non_coherent_atom_size();
    const auto multiply = std::max(1u, mi.chunk_multiply);
    const VkDeviceSize chunk_size = atom_size * multiply;

    // 当映射整个缓冲区（VK_WHOLE_SIZE）时，直接向底层驱动传入 VK_WHOLE_SIZE 映射到分配末端；
    // 如果是局部映射，则将驱动映射大小按 atom_size 向上对齐至 mapped_capacity，确保 flush 时能合法操作完整 atom。
    const VkDeviceSize vk_map_size = (mi.size == VK_WHOLE_SIZE)
        ? VK_WHOLE_SIZE
        : std::min(allocation_size - mi.offset, ((real_size + atom_size - 1) / atom_size) * atom_size);

    const VkDeviceSize actual_mapped_capacity = (vk_map_size == VK_WHOLE_SIZE)
        ? (allocation_size - mi.offset)
        : vk_map_size;

    void* mapped = nullptr;
    const VkResult code = vkMapMemory(
        device->get_system_handle(),
        memory,
        mi.offset,
        vk_map_size,
        0,
        &mapped
    );

    if(code != VK_SUCCESS) {
        mi.ew.report(
            ave_vk_map_buffer,
            "Failed to map Vulkan buffer memory ({}).",
            static_cast<int>(code)
        );
        return {};
    }

    return BufferData(
        device,
        memory,
        mapped,
        mi.offset,
        real_size,
        actual_mapped_capacity,
        allocation_size,
        chunk_size,
        is_host_coherent()
    );
}

std::shared_ptr<Buffer> Buffer::create_shared(CreateBufferInfo ci) {
    auto result = std::make_shared<Buffer>();
    if(!result->create(std::move(ci))) return nullptr;
    return result;
}

void Buffer::destroy() noexcept {
    if(device && device->get_system_handle() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device->get_system_handle());
        const auto handle = device->get_system_handle();
        const auto allocator = device->get_instance()->get_vk_allocator();
        if(buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(handle, buffer, allocator);
        }
        if(memory != VK_NULL_HANDLE) {
            vkFreeMemory(handle, memory, allocator);
        }
    }
    buffer = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;
    size = 0;
    allocation_size = 0;
    usage = 0;
    memory_properties = 0;
    device.reset();
}

}
