module;
#include <vulkan/vulkan.h>
#include <alib6/debug.h>

module ave.render;

import std;
import alib6;
import :buffer;
import :buffer_slice;

namespace ave {

BufferSlice::BufferSlice(
    std::shared_ptr<Buffer> target_buffer,
    VkDeviceSize target_offset,
    VkDeviceSize target_size
) : buffer(std::move(target_buffer)), offset(target_offset), size(0) {
    if(!buffer || buffer->get_system_handle() == VK_NULL_HANDLE) {
        return;
    }

    const auto total_size = buffer->get_size();
    if(offset >= total_size) {
        panic_debug(offset < total_size, "BufferSlice offset exceeds buffer size.");
        return;
    }

    const auto available_size = total_size - offset;
    if(target_size == VK_WHOLE_SIZE) {
        size = available_size;
    } else {
        panic_debug(
            target_size <= available_size,
            "BufferSlice size exceeds available buffer capacity."
        );
        size = std::min(target_size, available_size);
    }
}

BufferSlice BufferSlice::sub_slice(
    VkDeviceSize sub_offset,
    VkDeviceSize sub_size
) const {
    if(!*this || sub_offset >= size) {
        panic_debug(sub_offset < size, "sub_slice offset exceeds BufferSlice size.");
        return {};
    }

    const auto available_size = size - sub_offset;
    const auto real_sub_size = (sub_size == VK_WHOLE_SIZE)
        ? available_size
        : std::min(sub_size, available_size);

    return BufferSlice(buffer, offset + sub_offset, real_sub_size);
}

BufferData BufferSlice::map(MapBufferInfo mi) const {
    panic_debug(!buffer, "Cannot map an uninitialized BufferSlice.");
    if(!buffer) return {};

    if(mi.offset >= size) {
        mi.ew.report(
            ave_vk_map_buffer,
            "Mapping offset ({}) exceeds BufferSlice size ({}).",
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
            "Invalid mapping range: offset ({}) + size ({}) > BufferSlice size ({}).",
            mi.offset,
            real_size,
            size
        );
        return {};
    }

    MapBufferInfo target_mi = mi;
    target_mi.offset = offset + mi.offset;
    target_mi.size = real_size;
    return buffer->map(target_mi);
}

}
