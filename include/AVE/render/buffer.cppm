/**
 * @file buffer.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Vulkan 缓冲区及数据映射封装
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

export module ave.render:buffer;

import std;
import alib6;
import ave.ecode;
import :device;

export namespace ave {

    class Buffer;
    class BufferData;

    struct AVE_API CreateBufferInfo {
        std::shared_ptr<Device> device;
        
        /// 缓冲区大小（以字节为单位）。
        /// 若 atom_multiply == 0，底层将使用此 size 作为业务逻辑大小，并在向 Vulkan 创建 VkBuffer 时自动向上对齐到 hardware nonCoherentAtomSize。
        /// 若 atom_multiply > 0，此字段将被忽略，转而使用 atom_size * atom_multiply。
        VkDeviceSize size { 0 };

        /// 硬件 nonCoherentAtomSize 的倍数大小。
        /// 【重要】：atom_multiply 的优先级高于 size！
        /// - 当 atom_multiply > 0 时：优先以此值计算分配大小（实际尺寸 = nonCoherentAtomSize * atom_multiply），size 字段被忽略。
        ///   适合用于对象池、Staging 环形缓冲区、按页预分配等场景。
        /// - 当 atom_multiply == 0（默认）时：回退使用 size 字段，底层自动按 nonCoherentAtomSize 向上对齐保底。
        alib6::u32 atom_multiply { 0 };

        /// 缓冲区用途标志，默认提供传输源与目标
        VkBufferUsageFlags usage {
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        };

        /// 内存属性标志，默认可见性为 HOST_VISIBLE
        VkMemoryPropertyFlags memory_properties {
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        };

        const void* next { nullptr };
        VkBufferCreateFlags flags { 0 };
        VkSharingMode sharing_mode { VK_SHARING_MODE_EXCLUSIVE };
        std::vector<alib6::u32> queue_family_indices;
        const void* memory_allocate_next { nullptr };

        mutable alib6::ErrorWrapper ew {};

        inline CreateBufferInfo& enable_usage(VkBufferUsageFlags u) noexcept {
            usage |= u;
            return *this;
        }

        inline CreateBufferInfo& add_memory_property(VkMemoryPropertyFlags p) noexcept {
            memory_properties |= p;
            return *this;
        }
    };

    struct AVE_API MapBufferInfo {
        /// 多少倍硬件最低 nonCoherentAtomSize（每个 chunk 的尺寸为 nonCoherentAtomSize * chunk_multiply）
        alib6::u32 chunk_multiply { 16 };

        /// 映射的起始偏移量与范围
        VkDeviceSize offset { 0 };
        VkDeviceSize size { VK_WHOLE_SIZE };

        mutable alib6::ErrorWrapper ew {};
    };

    namespace detail {
        struct PodSpan {
            const void* ptr { nullptr };
            alib6::usize bytes { 0 };
        };

        template<typename T>
        constexpr auto extract_pod_span(const T& data) -> PodSpan {
            if constexpr (std::ranges::contiguous_range<T>) {
                using ValueType = std::ranges::range_value_t<T>;
                static_assert(
                    std::is_trivially_copyable_v<ValueType>,
                    "AVE: Range element type must be trivially copyable (POD)."
                );
                return PodSpan{
                    .ptr = static_cast<const void*>(std::ranges::data(data)),
                    .bytes = static_cast<alib6::usize>(std::ranges::size(data) * sizeof(ValueType))
                };
            } else if constexpr (std::is_trivially_copyable_v<T>) {
                static_assert(
                    !std::is_pointer_v<T>,
                    "AVE: Raw pointers cannot be uploaded/written directly to avoid sizeof(pointer) bugs. Wrap with std::span<const T> instead."
                );
                return PodSpan{
                    .ptr = static_cast<const void*>(std::addressof(data)),
                    .bytes = static_cast<alib6::usize>(sizeof(T))
                };
            } else if constexpr (std::ranges::range<T>) {
                static_assert(
                    sizeof(T) == 0,
                    "AVE: Range must be contiguous in memory (e.g. std::vector, std::span, std::array, raw array)."
                );
            } else {
                static_assert(
                    sizeof(T) == 0,
                    "AVE: Buffer upload/write requires a trivially copyable POD object or a contiguous range of PODs."
                );
            }
        }
    }

    class AVE_API BufferData {
    public:
        /// 代理字节引用，拦截写操作并打上脏块标记
        class AVE_API ByteProxy {
        private:
            uint8_t* ptr { nullptr };
            BufferData* parent { nullptr };
            alib6::usize offset { 0 };

        public:
            ByteProxy(uint8_t* p, BufferData* par, alib6::usize off) noexcept
                : ptr(p), parent(par), offset(off) {}

            ByteProxy& operator=(uint8_t val) noexcept;
            ByteProxy& operator=(const ByteProxy& other) noexcept;

            operator uint8_t() const noexcept { return *ptr; }
        };

        /// 代理迭代器，保证 non-const 遍历修改可拦截打标
        class Iterator {
        private:
            uint8_t* ptr { nullptr };
            BufferData* parent { nullptr };
            alib6::usize offset { 0 };

        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = uint8_t;
            using difference_type = std::ptrdiff_t;
            using pointer = void;
            using reference = ByteProxy;

            Iterator() = default;
            Iterator(uint8_t* p, BufferData* par, alib6::usize off) noexcept
                : ptr(p), parent(par), offset(off) {}

            reference operator*() const noexcept {
                return ByteProxy(ptr, parent, offset);
            }

            reference operator[](difference_type n) const noexcept {
                return ByteProxy(ptr + n, parent, offset + static_cast<alib6::usize>(n));
            }

            Iterator& operator++() noexcept { ++ptr; ++offset; return *this; }
            Iterator operator++(int) noexcept { auto tmp = *this; ++(*this); return tmp; }
            Iterator& operator--() noexcept { --ptr; --offset; return *this; }
            Iterator operator--(int) noexcept { auto tmp = *this; --(*this); return tmp; }

            Iterator& operator+=(difference_type n) noexcept {
                ptr += n;
                offset += static_cast<alib6::usize>(n);
                return *this;
            }
            Iterator& operator-=(difference_type n) noexcept {
                ptr -= n;
                offset -= static_cast<alib6::usize>(n);
                return *this;
            }

            friend Iterator operator+(Iterator it, difference_type n) noexcept { it += n; return it; }
            friend Iterator operator+(difference_type n, Iterator it) noexcept { it += n; return it; }
            friend Iterator operator-(Iterator it, difference_type n) noexcept { it -= n; return it; }
            friend difference_type operator-(const Iterator& a, const Iterator& b) noexcept {
                return a.ptr - b.ptr;
            }

            friend auto operator<=>(const Iterator& a, const Iterator& b) noexcept = default;
            friend bool operator==(const Iterator& a, const Iterator& b) noexcept {
                return a.ptr == b.ptr;
            }
        };

    private:
        friend class Buffer;

        std::shared_ptr<Device> device { nullptr };
        VkDeviceMemory memory { VK_NULL_HANDLE };
        void* mapped_ptr { nullptr };

        VkDeviceSize map_offset { 0 };
        VkDeviceSize map_size { 0 };
        VkDeviceSize mapped_capacity { 0 };
        VkDeviceSize allocation_size { 0 };
        VkDeviceSize chunk_size { 64 };
        alib6::usize chunk_count { 0 };

        bool is_coherent { false };
        bool auto_upload { true };

        alib6::storage::MonoBitSet dirty_mask;

        BufferData(
            std::shared_ptr<Device> dev,
            VkDeviceMemory mem,
            void* ptr,
            VkDeviceSize offset,
            VkDeviceSize size,
            VkDeviceSize capacity,
            VkDeviceSize alloc_size,
            VkDeviceSize c_size,
            bool coherent
        );

    public:
        BufferData() = default;
        ~BufferData() noexcept;

        // 移动语义
        BufferData(BufferData&& other) noexcept;
        BufferData& operator=(BufferData&& other) noexcept;

        // 禁用拷贝
        BufferData(const BufferData&) = delete;
        BufferData& operator=(const BufferData&) = delete;

        // 内存写入接口
        void memcpy(alib6::usize dst_offset, const void* src, alib6::usize bytes);

        /// 写入单个 POD 对象或 POD 连续区间（如 std::vector, std::span, std::array, C 数组）至 mapped 内存
        /// 自动标记对应 chunk 为 dirty，不立即触发 GPU Upload/Flush
        template<typename T>
        void write(const T& val, alib6::usize dst_offset = 0) {
            const auto span = detail::extract_pod_span(val);
            if(span.bytes > 0 && span.ptr) {
                this->memcpy(dst_offset, span.ptr, span.bytes);
            }
        }

        template<typename T>
        void write(alib6::usize dst_offset, const T& val) {
            this->write(val, dst_offset);
        }

        [[nodiscard]] ByteProxy operator[](alib6::usize byte_offset);
        [[nodiscard]] uint8_t operator[](alib6::usize byte_offset) const noexcept;

        // 迭代器支持
        [[nodiscard]] Iterator begin() noexcept {
            return Iterator(static_cast<uint8_t*>(mapped_ptr), this, 0);
        }
        [[nodiscard]] Iterator end() noexcept {
            return Iterator(static_cast<uint8_t*>(mapped_ptr) + map_size, this, map_size);
        }
        [[nodiscard]] const uint8_t* cbegin() const noexcept {
            return static_cast<const uint8_t*>(mapped_ptr);
        }
        [[nodiscard]] const uint8_t* cend() const noexcept {
            return static_cast<const uint8_t*>(mapped_ptr) + map_size;
        }

        // 脏标记与上传控制
        void mark_range_dirty(alib6::usize byte_offset, alib6::usize byte_count) noexcept;

        /// 按照修改过的 dirty chunks 动态上传（合并连续区间并执行 vkFlushMappedMemoryRanges）
        void upload();

        /// 强制上传（忽略脏位图，直接按指定区间或全量上传）
        void upload_raw(VkDeviceSize raw_offset = 0, VkDeviceSize raw_size = VK_WHOLE_SIZE);

        void cancel_auto_upload() noexcept { auto_upload = false; }

        // 查询
        [[nodiscard]] bool is_dirty() const noexcept { return !dirty_mask.none(); }
        [[nodiscard]] void* raw_data() noexcept { return mapped_ptr; }
        [[nodiscard]] const void* raw_data() const noexcept { return mapped_ptr; }
        [[nodiscard]] VkDeviceSize get_chunk_size() const noexcept { return chunk_size; }
        [[nodiscard]] alib6::usize get_chunk_count() const noexcept { return chunk_count; }
        [[nodiscard]] VkDeviceSize get_map_offset() const noexcept { return map_offset; }
        [[nodiscard]] VkDeviceSize get_map_size() const noexcept { return map_size; }
        [[nodiscard]] VkDeviceSize get_mapped_capacity() const noexcept { return mapped_capacity; }

        [[nodiscard]] explicit operator bool() const noexcept {
            return mapped_ptr != nullptr;
        }
    };

    class AVE_API Buffer {
    private:
        std::shared_ptr<Device> device;
        VkBuffer buffer { VK_NULL_HANDLE };
        VkDeviceMemory memory { VK_NULL_HANDLE };
        VkDeviceSize size { 0 };
        VkDeviceSize allocation_size { 0 };
        VkBufferUsageFlags usage { 0 };
        VkMemoryPropertyFlags memory_properties { 0 };

    public:
        Buffer() = default;
        explicit Buffer(CreateBufferInfo ci) { (void)create(std::move(ci)); }
        ~Buffer() { destroy(); }

        // 移动语义（参考 Instance 范式）
        Buffer(Buffer&& other) noexcept;
        Buffer& operator=(Buffer&& other) noexcept;

        // 禁用拷贝
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;

        /// 创建或重置缓冲区
        [[nodiscard]] bool create(CreateBufferInfo ci);

        /// 静态工厂模式创建（参考 Device / Image 范式）
        [[nodiscard]] static std::shared_ptr<Buffer> create_shared(CreateBufferInfo ci);

        /// 释放并清理持有的 Vulkan 资源
        void destroy() noexcept;

        /// 映射 Buffer 内存，返回具备修改追踪和 RAII 自动上传/解除映射能力的 BufferData
        [[nodiscard]] BufferData map(MapBufferInfo mi = {});

        /// 映射、写入并立即上传/刷新单个 POD 对象或 POD 连续区间（如 std::vector, std::span, std::array）至 GPU 显存
        template<typename T>
        bool upload(const T& val, VkDeviceSize dst_offset = 0) {
            const auto span = detail::extract_pod_span(val);
            if(span.bytes == 0) return true;
            auto mapped = this->map({ .offset = dst_offset, .size = span.bytes });
            if(!mapped) return false;
            mapped.memcpy(0, span.ptr, span.bytes);
            return true; // mapped 析构时自动 upload() 并 unmap()
        }

        template<typename T>
        bool upload(VkDeviceSize dst_offset, const T& val) {
            return this->upload(val, dst_offset);
        }

        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept {
            return device;
        }

        [[nodiscard]] VkBuffer get_system_handle() const noexcept {
            return buffer;
        }

        [[nodiscard]] VkDeviceMemory get_memory() const noexcept {
            return memory;
        }

        [[nodiscard]] VkDeviceSize get_size() const noexcept {
            return size;
        }

        [[nodiscard]] VkDeviceSize get_allocation_size() const noexcept {
            return allocation_size;
        }

        [[nodiscard]] VkBufferUsageFlags get_usage() const noexcept {
            return usage;
        }

        [[nodiscard]] VkMemoryPropertyFlags get_memory_properties() const noexcept {
            return memory_properties;
        }

        [[nodiscard]] bool is_host_visible() const noexcept {
            return (memory_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
        }

        [[nodiscard]] bool is_host_coherent() const noexcept {
            return (memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return buffer != VK_NULL_HANDLE;
        }
    };

}
