/**
 * @file buffer_slice.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 轻量级 BasicBuffer<MemoryPolicy> 区间切片（约定对象）
 * @version 5.0
 * @date 2026-09-12
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>
#include <alib6/debug.h>

export module ave.render:buffer_slice;

import std;
import alib6;
import :device;
import :buffer;

export namespace ave {

    template<class MemoryPolicy>
    class BasicBufferSlice {
    private:
        friend class BasicBuffer<MemoryPolicy>;
        std::shared_ptr<BasicBuffer<MemoryPolicy>> buffer { nullptr };
        std::shared_ptr<void> region_lifetime;
        VkDeviceSize offset { 0 };
        VkDeviceSize size { 0 };

    public:
        BasicBufferSlice() = default;

        /// 使用 BasicBuffer<MemoryPolicy>、起始 offset 以及切片大小 size 进行初始化
        /// 若 size 为 VK_WHOLE_SIZE，则默认覆盖从 offset 到 BasicBuffer<MemoryPolicy> 末尾的全部范围
        BasicBufferSlice(
            std::shared_ptr<BasicBuffer<MemoryPolicy>> target_buffer,
            VkDeviceSize target_offset,
            VkDeviceSize target_size = VK_WHOLE_SIZE
        ) : buffer(std::move(target_buffer)), offset(target_offset) {
            if(!buffer || !*buffer || offset >= buffer->get_size()) return;
            const auto available = buffer->get_size() - offset;
            size = target_size == VK_WHOLE_SIZE ? available : std::min(target_size, available);
        }

        // 自动分配的区域由切片副本、子切片和映射共同持有。
        BasicBufferSlice(const BasicBufferSlice&) = default;
        BasicBufferSlice& operator=(const BasicBufferSlice&) = default;
        BasicBufferSlice(BasicBufferSlice&&) noexcept = default;
        BasicBufferSlice& operator=(BasicBufferSlice&&) noexcept = default;
        ~BasicBufferSlice() = default;

        /// 清空当前切片并释放其所有权，可重复调用。
        /// 自动分配区域在最后一个切片/子切片/映射释放后归还；不清零或等待 GPU。
        void reset() noexcept {
            offset = 0;
            size = 0;
            region_lifetime.reset();
            buffer.reset();
        }

        [[nodiscard]] const std::shared_ptr<BasicBuffer<MemoryPolicy>>& get_buffer() const noexcept {
            return buffer;
        }

        [[nodiscard]] VkBuffer get_system_handle() const noexcept {
            return buffer ? buffer->get_system_handle() : VK_NULL_HANDLE;
        }

        [[nodiscard]] VkDeviceSize get_offset() const noexcept {
            return offset;
        }

        [[nodiscard]] VkDeviceSize get_size() const noexcept {
            return size;
        }

        /// 对当前切片做进一步的子切片（sub_offset 相对于当前切片的起始位置）
        [[nodiscard]] BasicBufferSlice sub_slice(
            VkDeviceSize sub_offset,
            VkDeviceSize sub_size = VK_WHOLE_SIZE
        ) const {
            if(!*this || sub_offset >= size) return {};
            const auto available = size - sub_offset;
            auto result = BasicBufferSlice(buffer, offset + sub_offset,
                sub_size == VK_WHOLE_SIZE ? available : std::min(sub_size, available));
            result.region_lifetime = region_lifetime;
            return result;
        }

        /// 映射该切片对应的显存区间
        [[nodiscard]] BasicBufferData<MemoryPolicy> map(MapBufferInfo mi = {}) const {
            if(!*this || mi.offset >= size) {
                mi.ew.report(ave_vk_map_buffer, "Invalid BufferSlice mapping or offset.");
                return {};
            }
            const auto bytes = mi.size == VK_WHOLE_SIZE ? size - mi.offset : mi.size;
            if(bytes == 0 || bytes > size - mi.offset) {
                mi.ew.report(ave_vk_map_buffer, "BufferSlice mapping exceeds slice size.");
                return {};
            }
            mi.offset += offset;
            mi.size = bytes;
            auto result = buffer->map(mi);
            result.region_lifetime = region_lifetime;
            return result;
        }

        /// 映射、写入并立即上传/刷新单个 POD 对象或 POD 连续区间（如 std::vector, std::span, std::array）至该切片区域（dst_offset 相对于当前切片起始位置）
        template<typename T>
        bool upload(const T& val, VkDeviceSize dst_offset = 0, alib6::ErrorWrapper ew = {}) const {
            const auto span = detail::extract_pod_span(val);
            if(span.bytes == 0) return true;
            if(!*this) {
                ew.report(ave_vk_map_buffer, "Cannot upload to an uninitialized BasicBufferSlice.");
                return false;
            }
            auto mapped = this->map({ .offset = dst_offset, .size = span.bytes, .ew = ew });
            if(!mapped) return false;
            mapped.memcpy(0, span.ptr, span.bytes);
            mapped.cancel_auto_upload();
            return mapped.flush(ew);
        }

        template<typename T>
        bool upload(VkDeviceSize dst_offset, const T& val, alib6::ErrorWrapper ew = {}) const {
            return this->upload(val, dst_offset, ew);
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return buffer != nullptr && bool(*buffer) && size > 0;
        }
    };

    template<class MemoryPolicy>
    BasicBufferSlice<MemoryPolicy> BasicBuffer<MemoryPolicy>::alloc_bytes(VkDeviceSize bytes, AllocateBufferInfo ai)
        requires std::same_as<MemoryPolicy, VmaMemoryPolicy> {
        if(!state || bytes == 0) {
            ai.ew.report(ave_vk_create_buffer, "alloc_bytes requires an initialized Buffer and nonzero size.");
            return {};
        }
        auto region = MemoryPolicy::allocate_region(state, bytes, ai);
        if(!region.lifetime) return {};
        // Snapshot the allocation state: also works for stack-owned Buffers, and survives recreate.
        auto owner = std::make_shared<BasicBuffer>();
        owner->state = state;
        BasicBufferSlice<MemoryPolicy> result(owner, region.offset, bytes);
        result.region_lifetime = std::move(region.lifetime);
        return result;
    }

    template<class MemoryPolicy>
    template<class T>
    BasicBufferSlice<MemoryPolicy> BasicBuffer<MemoryPolicy>::alloc(const T& data, AllocateBufferInfo ai)
        requires std::same_as<MemoryPolicy, VmaMemoryPolicy> {
        const auto span = detail::extract_pod_span(data);
        auto result = alloc_bytes(span.bytes, ai);
        if(!result) return {};
        auto mapped = result.map({.ew = ai.ew});
        if(!mapped) return {};
        mapped.memcpy(0, span.ptr, span.bytes);
        mapped.cancel_auto_upload();
        if(!mapped.flush(ai.ew)) return {};
        return result;
    }

    using BufferSlice = BasicBufferSlice<NativeMemoryPolicy>;
    using VMABufferSlice = BasicBufferSlice<VmaMemoryPolicy>;
}
