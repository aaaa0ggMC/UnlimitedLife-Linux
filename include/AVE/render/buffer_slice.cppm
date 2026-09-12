/**
 * @file buffer_slice.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 轻量级 Buffer 区间切片（约定对象）
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

    class AVE_API BufferSlice {
    private:
        std::shared_ptr<Buffer> buffer { nullptr };
        VkDeviceSize offset { 0 };
        VkDeviceSize size { 0 };

    public:
        BufferSlice() = default;

        /// 使用 Buffer、起始 offset 以及切片大小 size 进行初始化
        /// 若 size 为 VK_WHOLE_SIZE，则默认覆盖从 offset 到 Buffer 末尾的全部范围
        BufferSlice(
            std::shared_ptr<Buffer> target_buffer,
            VkDeviceSize target_offset,
            VkDeviceSize target_size = VK_WHOLE_SIZE
        );

        // 默认且完全无副作用的复制、移动与析构
        BufferSlice(const BufferSlice&) = default;
        BufferSlice& operator=(const BufferSlice&) = default;
        BufferSlice(BufferSlice&&) noexcept = default;
        BufferSlice& operator=(BufferSlice&&) noexcept = default;
        ~BufferSlice() = default;

        [[nodiscard]] const std::shared_ptr<Buffer>& get_buffer() const noexcept {
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
        [[nodiscard]] BufferSlice sub_slice(
            VkDeviceSize sub_offset,
            VkDeviceSize sub_size = VK_WHOLE_SIZE
        ) const;

        /// 映射该切片对应的显存区间
        [[nodiscard]] BufferData map(MapBufferInfo mi = {}) const;

        /// 映射、写入并立即上传/刷新单个 POD 对象或 POD 连续区间（如 std::vector, std::span, std::array）至该切片区域（dst_offset 相对于当前切片起始位置）
        template<typename T>
        bool upload(const T& val, VkDeviceSize dst_offset = 0) const {
            const auto span = detail::extract_pod_span(val);
            if(span.bytes == 0) return true;
            auto mapped = this->map({ .offset = dst_offset, .size = span.bytes });
            if(!mapped) return false;
            mapped.memcpy(0, span.ptr, span.bytes);
            return true;
        }

        template<typename T>
        bool upload(VkDeviceSize dst_offset, const T& val) const {
            return this->upload(val, dst_offset);
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return buffer != nullptr && bool(*buffer) && size > 0;
        }
    };

}
