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
import ave.reflect;
import :device;
import :buffer;

export namespace ave {

    template<class MemoryPolicy> class BasicBufferSlices;

    namespace detail {
        constexpr VkDeviceSize align_up(VkDeviceSize value, VkDeviceSize alignment) noexcept {
            if (alignment <= 1) return value;
            return (value + alignment - 1) / alignment * alignment;
        }
    }

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
            VkDeviceSize sub_size = VK_WHOLE_SIZE,
            alib6::ErrorWrapper ew = {}
        ) const {
            if(!*this || sub_offset >= size) {
                ew.report(ave_vk_map_buffer,
                    "sub_slice out of range: offset {} is invalid for a BufferSlice of size {}.",
                    sub_offset, size);
                return {};
            }
            const auto available = size - sub_offset;
            auto result = BasicBufferSlice(buffer, offset + sub_offset,
                sub_size == VK_WHOLE_SIZE ? available : std::min(sub_size, available));
            result.region_lifetime = region_lifetime;
            return result;
        }

        /// 将当前切片按指定元素大小及对齐等距切分为 count 个切片（步长为 align_up(element_size, alignment)）
        [[nodiscard]] BasicBufferSlices<MemoryPolicy> slice_n(
            std::size_t count,
            VkDeviceSize target_element_size,
            VkDeviceSize alignment = 1,
            VkDeviceSize sub_offset = 0,
            alib6::ErrorWrapper ew = {}
        ) const;

        template<typename T>
        [[nodiscard]] BasicBufferSlices<MemoryPolicy> slice_n(
            std::size_t count,
            VkDeviceSize alignment = 1,
            VkDeviceSize sub_offset = 0,
            alib6::ErrorWrapper ew = {}
        ) const;

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

        /// 底层连续字节写入
        bool upload_bytes(const void* ptr, VkDeviceSize bytes, VkDeviceSize dst_offset = 0, MapBufferInfo mi = {}) const {
            if(bytes == 0) return true;
            if(!*this) {
                mi.ew.report(ave_vk_map_buffer, "Cannot upload to an uninitialized BasicBufferSlice.");
                return false;
            }
            mi.offset = dst_offset;
            mi.size = bytes;
            auto mapped = this->map(mi);
            if(!mapped) return false;
            mapped.memcpy(0, ptr, bytes);
            mapped.cancel_auto_upload();
            return mapped.flush(mi.ew);
        }

        /// 映射、写入并立即上传/刷新单个 POD 对象或 POD 连续区间（如 std::vector, std::span, std::array）至该切片区域（dst_offset 相对于当前切片起始位置）
        template<typename T>
        bool upload(const T& val, VkDeviceSize dst_offset = 0, MapBufferInfo mi = {}) const {
            const auto span = detail::extract_pod_span(val);
            return upload_bytes(span.ptr, span.bytes, dst_offset, mi);
        }

        template<typename T>
        bool upload(const T& val, VkDeviceSize dst_offset, alib6::ErrorWrapper ew) const {
            return this->upload(val, dst_offset, MapBufferInfo{ .ew = ew });
        }

        template<typename T>
        bool upload(VkDeviceSize dst_offset, const T& val, alib6::ErrorWrapper ew = {}) const {
            return this->upload(val, dst_offset, MapBufferInfo{ .ew = ew });
        }

        /// @brief 通过成员指针自动反射其偏移与大小局部上传单个成员 (支持 base_offset 平移)
        template<auto MemberPtr, typename T>
        bool upload(
            const T& data,
            VkDeviceSize base_offset = 0,
            MapBufferInfo mi = {}
        ) const {
            using Traits = member_pointer_traits<decltype(MemberPtr)>;
            using ClassType = typename Traits::class_type;
            constexpr auto info = get_member_range_info<MemberPtr, MemberPtr>();
            const auto target_offset = base_offset + info.offset;

            if constexpr (std::is_same_v<std::remove_cvref_t<T>, ClassType>) {
                const void* ptr = reinterpret_cast<const char*>(std::addressof(data)) + info.offset;
                return upload_bytes(ptr, info.size, target_offset, mi);
            } else {
                static_assert(sizeof(T) == info.size, "Passed data size must match the reflected member size.");
                return upload_bytes(std::addressof(data), info.size, target_offset, mi);
            }
        }

        template<auto MemberPtr, typename T>
        bool upload(const T& data, VkDeviceSize base_offset, alib6::ErrorWrapper ew) const {
            return upload<MemberPtr>(data, base_offset, MapBufferInfo{ .ew = ew });
        }

        template<auto MemberPtr, typename T>
        bool upload(const T& data, alib6::ErrorWrapper ew) const {
            return upload<MemberPtr>(data, 0, MapBufferInfo{ .ew = ew });
        }

        /// @brief 显式别名：上传单个成员 (通过成员指针自动反射偏移与大小)
        template<auto MemberPtr, typename T>
        bool upload_member(const T& data, VkDeviceSize base_offset = 0, MapBufferInfo mi = {}) const {
            return upload<MemberPtr>(data, base_offset, mi);
        }

        template<auto MemberPtr, typename T>
        bool upload_member(const T& data, VkDeviceSize base_offset, alib6::ErrorWrapper ew) const {
            return upload<MemberPtr>(data, base_offset, MapBufferInfo{ .ew = ew });
        }

        template<auto MemberPtr, typename T>
        bool upload_member(const T& data, alib6::ErrorWrapper ew) const {
            return upload<MemberPtr>(data, 0, MapBufferInfo{ .ew = ew });
        }

        /// @brief 上传连续成员闭区间 [BeginPtr, EndPtr] (支持 base_offset 平移)
        template<auto BeginPtr, auto EndPtr, typename T>
        bool upload_range(
            const T& data,
            VkDeviceSize base_offset = 0,
            MapBufferInfo mi = {}
        ) const {
            using BeginTraits = member_pointer_traits<decltype(BeginPtr)>;
            using ClassType = typename BeginTraits::class_type;
            constexpr auto info = get_member_range_info<BeginPtr, EndPtr>();
            const auto target_offset = base_offset + info.offset;

            if constexpr (std::is_same_v<std::remove_cvref_t<T>, ClassType>) {
                const void* ptr = reinterpret_cast<const char*>(std::addressof(data)) + info.offset;
                return upload_bytes(ptr, info.size, target_offset, mi);
            } else {
                static_assert(sizeof(T) == info.size, "Passed data size must match the reflected range size.");
                return upload_bytes(std::addressof(data), info.size, target_offset, mi);
            }
        }

        template<auto BeginPtr, auto EndPtr, typename T>
        bool upload_range(const T& data, VkDeviceSize base_offset, alib6::ErrorWrapper ew) const {
            return upload_range<BeginPtr, EndPtr>(data, base_offset, MapBufferInfo{ .ew = ew });
        }

        template<auto BeginPtr, auto EndPtr, typename T>
        bool upload_range(const T& data, alib6::ErrorWrapper ew) const {
            return upload_range<BeginPtr, EndPtr>(data, 0, MapBufferInfo{ .ew = ew });
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return buffer != nullptr && bool(*buffer) && size > 0;
        }
    };

    template<class MemoryPolicy>
    class BasicBufferSlices {
    private:
        std::vector<BasicBufferSlice<MemoryPolicy>> slices;
        VkDeviceSize element_size { 0 };
        VkDeviceSize stride { 0 };

    public:
        BasicBufferSlices() = default;

        explicit BasicBufferSlices(
            std::vector<BasicBufferSlice<MemoryPolicy>> target_slices,
            VkDeviceSize target_element_size = 0,
            VkDeviceSize target_stride = 0
        ) : slices(std::move(target_slices)), element_size(target_element_size), stride(target_stride) {}

        BasicBufferSlices(const BasicBufferSlices&) = default;
        BasicBufferSlices& operator=(const BasicBufferSlices&) = default;
        BasicBufferSlices(BasicBufferSlices&&) noexcept = default;
        BasicBufferSlices& operator=(BasicBufferSlices&&) noexcept = default;
        ~BasicBufferSlices() = default;

        [[nodiscard]] BasicBufferSlice<MemoryPolicy>& operator[](std::size_t index) noexcept {
            return slices[index];
        }
        [[nodiscard]] const BasicBufferSlice<MemoryPolicy>& operator[](std::size_t index) const noexcept {
            return slices[index];
        }
        [[nodiscard]] BasicBufferSlice<MemoryPolicy>& at(std::size_t index) {
            return slices.at(index);
        }
        [[nodiscard]] const BasicBufferSlice<MemoryPolicy>& at(std::size_t index) const {
            return slices.at(index);
        }

        [[nodiscard]] BasicBufferSlice<MemoryPolicy>& front() noexcept { return slices.front(); }
        [[nodiscard]] const BasicBufferSlice<MemoryPolicy>& front() const noexcept { return slices.front(); }
        [[nodiscard]] BasicBufferSlice<MemoryPolicy>& back() noexcept { return slices.back(); }
        [[nodiscard]] const BasicBufferSlice<MemoryPolicy>& back() const noexcept { return slices.back(); }

        [[nodiscard]] std::size_t size() const noexcept { return slices.size(); }
        [[nodiscard]] bool empty() const noexcept { return slices.empty(); }
        [[nodiscard]] VkDeviceSize get_stride() const noexcept { return stride; }
        [[nodiscard]] VkDeviceSize get_element_size() const noexcept { return element_size; }

        [[nodiscard]] auto begin() noexcept { return slices.begin(); }
        [[nodiscard]] auto end() noexcept { return slices.end(); }
        [[nodiscard]] auto begin() const noexcept { return slices.begin(); }
        [[nodiscard]] auto end() const noexcept { return slices.end(); }
        [[nodiscard]] auto cbegin() const noexcept { return slices.cbegin(); }
        [[nodiscard]] auto cend() const noexcept { return slices.cend(); }

        [[nodiscard]] std::span<const BasicBufferSlice<MemoryPolicy>> as_span() const noexcept {
            return slices;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return !slices.empty() && bool(slices.front());
        }

        void clear() noexcept {
            slices.clear();
            element_size = 0;
            stride = 0;
        }

        template<typename T>
        bool upload_all(const T& val, VkDeviceSize dst_sub_offset = 0, MapBufferInfo mi = {}) const {
            for(const auto& s : slices) {
                if(!s.upload(val, dst_sub_offset, mi)) return false;
            }
            return true;
        }

        template<typename T>
        bool upload_all(const T& val, VkDeviceSize dst_sub_offset, alib6::ErrorWrapper ew) const {
            return this->upload_all(val, dst_sub_offset, MapBufferInfo{ .ew = ew });
        }

        template<typename T>
        bool upload_all(const T& val, alib6::ErrorWrapper ew) const {
            return this->upload_all(val, 0, MapBufferInfo{ .ew = ew });
        }

        template<typename T>
        bool upload_each(std::span<const T> data_list, VkDeviceSize dst_sub_offset = 0, MapBufferInfo mi = {}) const {
            const std::size_t count = std::min(slices.size(), data_list.size());
            for(std::size_t i = 0; i < count; ++i) {
                if(!slices[i].upload(data_list[i], dst_sub_offset, mi)) return false;
            }
            return true;
        }

        template<typename T>
        bool upload_each(std::span<const T> data_list, VkDeviceSize dst_sub_offset, alib6::ErrorWrapper ew) const {
            return this->upload_each(data_list, dst_sub_offset, MapBufferInfo{ .ew = ew });
        }

        template<typename T>
        bool upload_each(std::span<const T> data_list, alib6::ErrorWrapper ew) const {
            return this->upload_each(data_list, 0, MapBufferInfo{ .ew = ew });
        }

        /// @brief 通过成员指针自动反射其偏移与大小，向所有切片广播局部上传单个成员 (支持 base_offset 平移)
        template<auto MemberPtr, typename T>
        bool upload_all(
            const T& data,
            VkDeviceSize base_offset = 0,
            MapBufferInfo mi = {}
        ) const {
            for(const auto& s : slices) {
                if(!s.template upload<MemberPtr>(data, base_offset, mi)) return false;
            }
            return true;
        }

        template<auto MemberPtr, typename T>
        bool upload_all(const T& data, VkDeviceSize base_offset, alib6::ErrorWrapper ew) const {
            return this->template upload_all<MemberPtr>(data, base_offset, MapBufferInfo{ .ew = ew });
        }

        template<auto MemberPtr, typename T>
        bool upload_all(const T& data, alib6::ErrorWrapper ew) const {
            return this->template upload_all<MemberPtr>(data, 0, MapBufferInfo{ .ew = ew });
        }

        /// @brief 显式别名：向所有切片广播上传单个成员
        template<auto MemberPtr, typename T>
        bool upload_all_member(const T& data, VkDeviceSize base_offset = 0, MapBufferInfo mi = {}) const {
            return this->template upload_all<MemberPtr>(data, base_offset, mi);
        }

        template<auto MemberPtr, typename T>
        bool upload_all_member(const T& data, VkDeviceSize base_offset, alib6::ErrorWrapper ew) const {
            return this->template upload_all<MemberPtr>(data, base_offset, MapBufferInfo{ .ew = ew });
        }

        template<auto MemberPtr, typename T>
        bool upload_all_member(const T& data, alib6::ErrorWrapper ew) const {
            return this->template upload_all<MemberPtr>(data, 0, MapBufferInfo{ .ew = ew });
        }

        /// @brief 向所有切片广播上传连续成员闭区间 [BeginPtr, EndPtr] (支持 base_offset 平移)
        template<auto BeginPtr, auto EndPtr, typename T>
        bool upload_all_range(
            const T& data,
            VkDeviceSize base_offset = 0,
            MapBufferInfo mi = {}
        ) const {
            for(const auto& s : slices) {
                if(!s.template upload_range<BeginPtr, EndPtr>(data, base_offset, mi)) return false;
            }
            return true;
        }

        template<auto BeginPtr, auto EndPtr, typename T>
        bool upload_all_range(const T& data, VkDeviceSize base_offset, alib6::ErrorWrapper ew) const {
            return this->template upload_all_range<BeginPtr, EndPtr>(data, base_offset, MapBufferInfo{ .ew = ew });
        }

        template<auto BeginPtr, auto EndPtr, typename T>
        bool upload_all_range(const T& data, alib6::ErrorWrapper ew) const {
            return this->template upload_all_range<BeginPtr, EndPtr>(data, 0, MapBufferInfo{ .ew = ew });
        }
    };

    template<class MemoryPolicy>
    BasicBufferSlices<MemoryPolicy> BasicBufferSlice<MemoryPolicy>::slice_n(
        std::size_t count,
        VkDeviceSize target_element_size,
        VkDeviceSize alignment,
        VkDeviceSize sub_offset,
        alib6::ErrorWrapper ew
    ) const {
        if(!*this || count == 0 || target_element_size == 0) {
            ew.report(ave_vk_map_buffer,
                "slice_n requires a valid BufferSlice, nonzero count and element size.");
            return {};
        }
        const auto target_stride = detail::align_up(target_element_size, alignment);
        const auto total_needed = (count - 1) * target_stride + target_element_size;
        if(sub_offset + total_needed > size) {
            ew.report(ave_vk_map_buffer,
                "slice_n range [{} , {}) exceeds BufferSlice size ({}).",
                sub_offset, sub_offset + total_needed, size);
            return {};
        }

        std::vector<BasicBufferSlice<MemoryPolicy>> result_slices;
        result_slices.reserve(count);
        for(std::size_t i = 0; i < count; ++i) {
            auto s = BasicBufferSlice<MemoryPolicy>(buffer, offset + sub_offset + i * target_stride, target_element_size);
            s.region_lifetime = region_lifetime;
            result_slices.push_back(std::move(s));
        }
        return BasicBufferSlices<MemoryPolicy>(std::move(result_slices), target_element_size, target_stride);
    }

    template<class MemoryPolicy>
    template<typename T>
    BasicBufferSlices<MemoryPolicy> BasicBufferSlice<MemoryPolicy>::slice_n(
        std::size_t count,
        VkDeviceSize alignment,
        VkDeviceSize sub_offset,
        alib6::ErrorWrapper ew
    ) const {
        return slice_n(count, static_cast<VkDeviceSize>(sizeof(T)), alignment, sub_offset, ew);
    }

    template<class MemoryPolicy>
    BasicBufferSlices<MemoryPolicy> BasicBuffer<MemoryPolicy>::slice_n(
        std::size_t count,
        VkDeviceSize element_size,
        VkDeviceSize alignment,
        VkDeviceSize start_offset,
        alib6::ErrorWrapper ew
    ) const {
        if(!*this || count == 0 || element_size == 0) {
            ew.report(ave_vk_map_buffer,
                "slice_n requires an initialized Buffer, nonzero count and element size.");
            return {};
        }
        const auto target_stride = detail::align_up(element_size, alignment);
        const auto total_needed = (count - 1) * target_stride + element_size;
        if(start_offset + total_needed > get_size()) {
            ew.report(ave_vk_map_buffer,
                "slice_n range [{} , {}) exceeds Buffer size ({}).",
                start_offset, start_offset + total_needed, get_size());
            return {};
        }

        auto owner = std::make_shared<BasicBuffer>();
        owner->state = state;

        std::vector<BasicBufferSlice<MemoryPolicy>> result_slices;
        result_slices.reserve(count);
        for(std::size_t i = 0; i < count; ++i) {
            result_slices.emplace_back(owner, start_offset + i * target_stride, element_size);
        }
        return BasicBufferSlices<MemoryPolicy>(std::move(result_slices), element_size, target_stride);
    }

    template<class MemoryPolicy>
    template<typename T>
    BasicBufferSlices<MemoryPolicy> BasicBuffer<MemoryPolicy>::slice_n(
        std::size_t count,
        VkDeviceSize alignment,
        VkDeviceSize start_offset,
        alib6::ErrorWrapper ew
    ) const {
        return slice_n(count, static_cast<VkDeviceSize>(sizeof(T)), alignment, start_offset, ew);
    }

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
        auto mi = ai.map_info;
        if(ai.ew) mi.ew = ai.ew;
        if(!result.upload(data, 0, mi)) return {};
        return result;
    }

    template<class MemoryPolicy>
    BasicBufferSlices<MemoryPolicy> BasicBuffer<MemoryPolicy>::alloc_n(
        std::size_t count,
        VkDeviceSize element_size,
        AllocateBufferInfo ai
    ) requires std::same_as<MemoryPolicy, VmaMemoryPolicy> {
        if(!state || count == 0 || element_size == 0) {
            ai.ew.report(ave_vk_create_buffer, "alloc_n requires an initialized Buffer and nonzero count and size.");
            return {};
        }
        const auto target_stride = detail::align_up(element_size, ai.alignment);
        const auto total_bytes = (count - 1) * target_stride + element_size;

        auto region = MemoryPolicy::allocate_region(state, total_bytes, ai);
        if(!region.lifetime) return {};

        auto owner = std::make_shared<BasicBuffer>();
        owner->state = state;

        std::vector<BasicBufferSlice<MemoryPolicy>> result_slices;
        result_slices.reserve(count);
        for(std::size_t i = 0; i < count; ++i) {
            BasicBufferSlice<MemoryPolicy> s(owner, region.offset + i * target_stride, element_size);
            s.region_lifetime = region.lifetime;
            result_slices.push_back(std::move(s));
        }
        return BasicBufferSlices<MemoryPolicy>(std::move(result_slices), element_size, target_stride);
    }

    template<class MemoryPolicy>
    template<class T>
    BasicBufferSlices<MemoryPolicy> BasicBuffer<MemoryPolicy>::alloc_n(
        std::size_t count,
        AllocateBufferInfo ai
    ) requires std::same_as<MemoryPolicy, VmaMemoryPolicy> {
        return alloc_n(count, static_cast<VkDeviceSize>(sizeof(T)), ai);
    }

    template<class MemoryPolicy>
    template<class T>
    BasicBufferSlices<MemoryPolicy> BasicBuffer<MemoryPolicy>::alloc_n(
        std::size_t count,
        const T& initial_data,
        AllocateBufferInfo ai
    ) requires std::same_as<MemoryPolicy, VmaMemoryPolicy> {
        auto slices = alloc_n<T>(count, ai);
        if(!slices) return {};
        auto mi = ai.map_info;
        if(ai.ew) mi.ew = ai.ew;
        if(!slices.upload_all(initial_data, 0, mi)) return {};
        return slices;
    }

    using BufferSlice = BasicBufferSlice<NativeMemoryPolicy>;
    using VMABufferSlice = BasicBufferSlice<VmaMemoryPolicy>;
    using BufferSlices = BasicBufferSlices<NativeMemoryPolicy>;
    using VMABufferSlices = BasicBufferSlices<VmaMemoryPolicy>;

    template<class MemoryPolicy>
    StagingSource::StagingSource(BasicBufferSlice<MemoryPolicy>& s) {
        buffer = s.get_system_handle();
        offset = s.get_offset();
        capacity = s.get_size();
        if(s.get_buffer() && (s.get_buffer()->get_usage() & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) && s.get_buffer()->is_host_visible()) {
            auto mapped = s.map();
            if(mapped) {
                mapped_ptr = mapped.raw_data();
                lifetime = std::make_shared<BasicBufferData<MemoryPolicy>>(std::move(mapped));
            }
        }
    }
}
