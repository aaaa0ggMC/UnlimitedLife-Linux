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
import ave.reflect;
import :device;
import :upload_context;

export namespace ave {

    template<class MemoryPolicy> class BasicBuffer;
    template<class MemoryPolicy> class BasicBufferData;
    template<class MemoryPolicy> class BasicBufferSlice;
    template<class MemoryPolicy> class BasicBufferSlices;
    class UploadContext;
    class UploadTicket;

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

    struct StagingSource {
        VkBuffer buffer { VK_NULL_HANDLE };
        VkDeviceSize offset { 0 };
        VkDeviceSize capacity { 0 };
        void* mapped_ptr { nullptr };
        std::shared_ptr<void> lifetime;

        constexpr StagingSource() = default;
        constexpr StagingSource(std::nullptr_t) noexcept {}

        template<class MemoryPolicy>
        StagingSource(BasicBuffer<MemoryPolicy>& b);
        template<class MemoryPolicy>
        StagingSource(BasicBuffer<MemoryPolicy>* b) {
            if(b) *this = StagingSource(*b);
        }
        template<class MemoryPolicy>
        StagingSource(BasicBufferSlice<MemoryPolicy>& s);
        template<class MemoryPolicy>
        StagingSource(BasicBufferSlice<MemoryPolicy>* s) {
            if(s) *this = StagingSource(*s);
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return buffer != VK_NULL_HANDLE && mapped_ptr != nullptr;
        }
        bool operator==(std::nullptr_t) const noexcept {
            return buffer == VK_NULL_HANDLE;
        }
        bool operator!=(std::nullptr_t) const noexcept {
            return buffer != VK_NULL_HANDLE;
        }
    };

    struct AVE_API MapBufferInfo {
        /// 多少倍硬件最低 nonCoherentAtomSize（每个 chunk 的尺寸为 nonCoherentAtomSize * chunk_multiply）
        alib6::u32 chunk_multiply { 16 };

        /// 映射的起始偏移量与范围
        VkDeviceSize offset { 0 };
        VkDeviceSize size { VK_WHOLE_SIZE };

        /// 可选 Staging 暂存缓冲源；启用时写入重定向至 staging，并在 submit/upload 时复制到目标 Buffer
        StagingSource staging {};

        /// 启用 staging 时必须提供，负责分配单次命令并提交至图形/传输队列
        UploadContext* upload_context { nullptr };

        mutable alib6::ErrorWrapper ew {};
    };

    struct AVE_API AllocateBufferInfo {
        /// 区域起点对齐（2 的幂）；内部还会满足 nonCoherentAtomSize。
        VkDeviceSize alignment { 1 };
        MapBufferInfo map_info {};
        mutable alib6::ErrorWrapper ew {};
    };

    /// CPU 访问模式；DeviceOnly 不允许 map/upload。
    enum class BufferHostAccess { DeviceOnly, SequentialWrite, RandomAccess };
    enum class BufferMemoryPreference { Automatic, PreferDevice, PreferHost };

    struct AVE_API CreateVMAAllocatorInfo {
        std::shared_ptr<Device> device;
        /// 必须是 Instance 与物理设备共同支持的版本，默认保守使用 Vulkan 1.0。
        alib6::u32 api_version { VK_API_VERSION_1_0 };
        /// 仅在逻辑设备已启用 bufferDeviceAddress 特性时设置。
        bool buffer_device_address { false };
        mutable alib6::ErrorWrapper ew {};
    };

    struct VmaMemoryPolicy;

    /// 不暴露 VMA 原生类型；由所有所属 Buffer 共同持有。
    class AVE_API VMAAllocator {
        struct Impl;
        std::shared_ptr<Impl> impl;
        explicit VMAAllocator(std::shared_ptr<Impl>);
        friend struct VmaMemoryPolicy;
    public:
        ~VMAAllocator();
        VMAAllocator(const VMAAllocator&) = delete;
        VMAAllocator& operator=(const VMAAllocator&) = delete;
        [[nodiscard]] static std::shared_ptr<VMAAllocator> create_shared(CreateVMAAllocatorInfo);
        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept;
    };

    struct AVE_API CreateVMABufferInfo {
        std::shared_ptr<VMAAllocator> allocator;
        VkDeviceSize size { 0 };
        alib6::u32 atom_multiply { 0 };
        VkBufferUsageFlags usage {
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        };
        BufferHostAccess host_access { BufferHostAccess::SequentialWrite };
        BufferMemoryPreference memory_preference { BufferMemoryPreference::Automatic };
        VkMemoryPropertyFlags required_memory_properties { 0 };
        VkMemoryPropertyFlags preferred_memory_properties { 0 };
        bool persistent_mapping { false };
        bool dedicated_allocation { false };
        const void* next { nullptr };
        VkBufferCreateFlags flags { 0 };
        VkSharingMode sharing_mode { VK_SHARING_MODE_EXCLUSIVE };
        std::vector<alib6::u32> queue_family_indices;
        mutable alib6::ErrorWrapper ew {};

        CreateVMABufferInfo& enable_usage(VkBufferUsageFlags u) noexcept {
            usage |= u;
            return *this;
        }
        CreateVMABufferInfo& add_memory_property(VkMemoryPropertyFlags p) noexcept {
            required_memory_properties |= p;
            return *this;
        }
    };

    namespace detail {
        struct BufferRegion {
            std::shared_ptr<void> lifetime;
            VkDeviceSize offset { 0 };
        };
        struct BufferProperties {
            std::shared_ptr<Device> device;
            VkBuffer buffer { VK_NULL_HANDLE };
            VkDeviceMemory memory { VK_NULL_HANDLE };
            VkDeviceSize memory_offset { 0 };
            VkDeviceSize size { 0 };
            VkDeviceSize allocation_size { 0 };
            VkBufferUsageFlags usage { 0 };
            VkMemoryPropertyFlags memory_properties { 0 };
        };
    }

    struct AVE_API NativeMemoryPolicy {
        struct State;
        using CreateInfo = CreateBufferInfo;
        [[nodiscard]] static std::shared_ptr<State> create(CreateInfo);
        [[nodiscard]] static const detail::BufferProperties& properties(const State&) noexcept;
        static VkResult map(State&, void**);
        static void unmap(State&) noexcept;
        static VkResult flush(State&, VkDeviceSize offset, VkDeviceSize size);
        static VkResult invalidate(State&, VkDeviceSize offset, VkDeviceSize size);
        static bool try_acquire_stagedby(State&, const void* session_ptr);
        static void release_stagedby(State&, const void* session_ptr) noexcept;
        static const void* get_stagedby(const State&) noexcept;
    };

    struct AVE_API VmaMemoryPolicy {
        struct State;
        using CreateInfo = CreateVMABufferInfo;
        [[nodiscard]] static std::shared_ptr<State> create(CreateInfo);
        [[nodiscard]] static const detail::BufferProperties& properties(const State&) noexcept;
        static VkResult map(State&, void**);
        static void unmap(State&) noexcept;
        static VkResult flush(State&, VkDeviceSize offset, VkDeviceSize size);
        static VkResult invalidate(State&, VkDeviceSize offset, VkDeviceSize size);
        static bool try_acquire_stagedby(State&, const void* session_ptr);
        static void release_stagedby(State&, const void* session_ptr) noexcept;
        static const void* get_stagedby(const State&) noexcept;
        static detail::BufferRegion allocate_region(const std::shared_ptr<State>&,
            VkDeviceSize bytes, AllocateBufferInfo);
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

    template<class MemoryPolicy>
    class BasicBufferData {
    public:
        /// 代理字节引用，拦截写操作并打上脏块标记
        class ByteProxy {
        private:
            uint8_t* ptr { nullptr };
            BasicBufferData* parent { nullptr };
            alib6::usize offset { 0 };

        public:
            ByteProxy(uint8_t* p, BasicBufferData* par, alib6::usize off) noexcept
                : ptr(p), parent(par), offset(off) {}

            ByteProxy& operator=(uint8_t val) noexcept;
            ByteProxy& operator=(const ByteProxy& other) noexcept;

            operator uint8_t() const noexcept { return *ptr; }
        };

        /// 代理迭代器，保证 non-const 遍历修改可拦截打标
        class Iterator {
        private:
            uint8_t* ptr { nullptr };
            BasicBufferData* parent { nullptr };
            alib6::usize offset { 0 };

        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = uint8_t;
            using difference_type = std::ptrdiff_t;
            using pointer = void;
            using reference = ByteProxy;

            Iterator() = default;
            Iterator(uint8_t* p, BasicBufferData* par, alib6::usize off) noexcept
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
        friend class BasicBuffer<MemoryPolicy>;
        friend class BasicBufferSlice<MemoryPolicy>;

        std::shared_ptr<typename MemoryPolicy::State> state;
        std::shared_ptr<void> region_lifetime;
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

    public:
        struct StagingSession {
            std::shared_ptr<typename MemoryPolicy::State> target_state;
            VkBuffer dst_buffer { VK_NULL_HANDLE };
            VkDeviceSize dst_offset { 0 };
            VkBuffer src_buffer { VK_NULL_HANDLE };
            VkDeviceSize src_offset { 0 };
            VkDeviceSize size { 0 };
            UploadContext* upload_context { nullptr };
            std::shared_ptr<void> staging_lifetime;
            ~StagingSession() {
                if(target_state) {
                    if constexpr (requires { MemoryPolicy::release_stagedby(*target_state, this); }) {
                        MemoryPolicy::release_stagedby(*target_state, this);
                    }
                }
            }
        };

    private:
        std::shared_ptr<StagingSession> staging_session;
        UploadTicket last_ticket;

        void release() noexcept;

        BasicBufferData(
            std::shared_ptr<typename MemoryPolicy::State> resource,
            void* ptr,
            VkDeviceSize offset,
            VkDeviceSize size,
            VkDeviceSize capacity,
            VkDeviceSize alloc_size,
            VkDeviceSize c_size,
            bool coherent
        );

        BasicBufferData(
            std::shared_ptr<typename MemoryPolicy::State> resource,
            std::shared_ptr<StagingSession> session,
            void* ptr,
            VkDeviceSize offset,
            VkDeviceSize size,
            VkDeviceSize capacity,
            VkDeviceSize alloc_size,
            VkDeviceSize c_size
        );

    public:
        BasicBufferData() = default;
        ~BasicBufferData() noexcept;

        // 移动语义
        BasicBufferData(BasicBufferData&& other) noexcept;
        BasicBufferData& operator=(BasicBufferData&& other) noexcept;

        // 禁用拷贝
        BasicBufferData(const BasicBufferData&) = delete;
        BasicBufferData& operator=(const BasicBufferData&) = delete;

        // 内存写入接口
        void memcpy(alib6::usize dst_offset, const void* src, alib6::usize bytes);

        /// 写入单个 POD 对象或 POD 连续区间（如 std::vector, std::span, std::array, C 数组）至 mapped 内存
        /// 自动标记对应 chunk 为 dirty，不立即触发 GPU Upload/Flush
        template<typename T>
        bool write(const T& val, alib6::usize dst_offset = 0, alib6::ErrorWrapper ew = {}) {
            const auto span = detail::extract_pod_span(val);
            if(span.bytes == 0 || !span.ptr) return true;
            if(dst_offset > map_size || span.bytes > map_size - dst_offset) {
                ew.report(
                    ave_vk_map_buffer,
                    "BasicBufferData::write out of bounds: dst_offset ({}) + bytes ({}) > map_size ({}).",
                    dst_offset, span.bytes, map_size
                );
                return false;
            }
            this->memcpy(dst_offset, span.ptr, span.bytes);
            return true;
        }

        template<typename T>
        bool write(alib6::usize dst_offset, const T& val, alib6::ErrorWrapper ew = {}) {
            return this->write(val, dst_offset, ew);
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
        /// 显式 flush 返回错误；析构仍提供 best-effort 自动刷新。
        [[nodiscard]] bool flush(alib6::ErrorWrapper ew = {});
        [[nodiscard]] bool invalidate(VkDeviceSize offset = 0,
                                      VkDeviceSize size = VK_WHOLE_SIZE,
                                      alib6::ErrorWrapper ew = {});
        /// 仅在 staging 映射时可用：录制并提交 vkCmdCopyBuffer，返回异步凭据
        [[nodiscard]] UploadTicket submit(alib6::ErrorWrapper ew = {});
        [[nodiscard]] bool is_staging() const noexcept { return bool(staging_session); }

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

    template<class MemoryPolicy>
    class BasicBuffer {
        std::shared_ptr<typename MemoryPolicy::State> state;

        const detail::BufferProperties& properties() const noexcept {
            static const detail::BufferProperties empty {};
            return state ? MemoryPolicy::properties(*state) : empty;
        }
    public:
        using CreateInfo = typename MemoryPolicy::CreateInfo;
        using Data = BasicBufferData<MemoryPolicy>;

        /// 只分配字节区域，不初始化内容；也适用于 DeviceOnly Buffer。
        [[nodiscard]] BasicBufferSlice<MemoryPolicy> alloc_bytes(VkDeviceSize bytes, AllocateBufferInfo ai = {})
            requires std::same_as<MemoryPolicy, VmaMemoryPolicy>;

        /// 在现有容量内自动分配并写入 POD 或 POD 连续区间；失败返回空切片。
        template<class T>
        [[nodiscard]] BasicBufferSlice<MemoryPolicy> alloc(const T& data, AllocateBufferInfo ai = {})
            requires std::same_as<MemoryPolicy, VmaMemoryPolicy>;

        /// 将当前 Buffer 按指定元素大小及对齐等距切分为 count 个切片（步长为 align_up(element_size, alignment)）
        [[nodiscard]] BasicBufferSlices<MemoryPolicy> slice_n(
            std::size_t count,
            VkDeviceSize element_size,
            VkDeviceSize alignment = 1,
            VkDeviceSize start_offset = 0
        ) const;

        template<typename T>
        [[nodiscard]] BasicBufferSlices<MemoryPolicy> slice_n(
            std::size_t count,
            VkDeviceSize alignment = 1,
            VkDeviceSize start_offset = 0
        ) const;

        /// 在现有容量内自动分配一段包含 count 个槽位的连续区域，切分为 count 个切片（步长按 ai.alignment 对齐），所有切片共享同一次分配的生命周期。
        [[nodiscard]] BasicBufferSlices<MemoryPolicy> alloc_n(
            std::size_t count,
            VkDeviceSize element_size,
            AllocateBufferInfo ai = {}
        ) requires std::same_as<MemoryPolicy, VmaMemoryPolicy>;

        template<class T>
        [[nodiscard]] BasicBufferSlices<MemoryPolicy> alloc_n(
            std::size_t count,
            AllocateBufferInfo ai = {}
        ) requires std::same_as<MemoryPolicy, VmaMemoryPolicy>;

        template<class T>
        [[nodiscard]] BasicBufferSlices<MemoryPolicy> alloc_n(
            std::size_t count,
            const T& initial_data,
            AllocateBufferInfo ai = {}
        ) requires std::same_as<MemoryPolicy, VmaMemoryPolicy>;

        BasicBuffer() = default;
        explicit BasicBuffer(CreateInfo ci) { (void)create(std::move(ci)); }
        ~BasicBuffer() = default;
        BasicBuffer(BasicBuffer&&) noexcept = default;
        BasicBuffer& operator=(BasicBuffer&&) noexcept = default;
        BasicBuffer(const BasicBuffer&) = delete;
        BasicBuffer& operator=(const BasicBuffer&) = delete;

        [[nodiscard]] bool create(CreateInfo ci) {
            if(state) {
                ci.ew.report(ave_already_created, "Buffer has already been created.");
                return false;
            }
            state = MemoryPolicy::create(std::move(ci));
            return bool(state);
        }

        [[nodiscard]] static std::shared_ptr<BasicBuffer> create_shared(CreateInfo ci) {
            auto result = std::make_shared<BasicBuffer>();
            return result->create(std::move(ci)) ? result : nullptr;
        }

        /// 释放本对象的所有权；已有映射会延长分配寿命。
        /// 调用方须保证 GPU 已不再使用该资源（不隐式 vkDeviceWaitIdle）。
        void destroy() noexcept { state.reset(); }

        [[nodiscard]] const void* get_stagedby() const noexcept {
            if constexpr (requires { MemoryPolicy::get_stagedby(*state); }) {
                return state ? MemoryPolicy::get_stagedby(*state) : nullptr;
            } else {
                return nullptr;
            }
        }

        [[nodiscard]] Data map(MapBufferInfo mi = {}) {
            if(!state) {
                mi.ew.report(ave_vk_map_buffer, "Buffer is uninitialized.");
                return {};
            }
            const auto total = get_size();
            if(mi.offset >= total) {
                mi.ew.report(ave_vk_map_buffer, "Buffer mapping offset is out of bounds.");
                return {};
            }
            const auto bytes = mi.size == VK_WHOLE_SIZE ? total - mi.offset : mi.size;
            if(bytes == 0 || bytes > total - mi.offset) {
                mi.ew.report(ave_vk_map_buffer, "Buffer mapping size is out of bounds.");
                return {};
            }

            if(mi.staging != nullptr) {
                if(!mi.upload_context) {
                    mi.ew.report(ave_vk_map_buffer, "Staging mapping requires a valid UploadContext.");
                    return {};
                }
                if(!mi.staging) {
                    mi.ew.report(ave_vk_map_buffer,
                        "Staging buffer must be valid, host-visible, and have VK_BUFFER_USAGE_TRANSFER_SRC_BIT.");
                    return {};
                }
                if((get_usage() & VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0) {
                    mi.ew.report(ave_vk_map_buffer,
                        "Target buffer requires VK_BUFFER_USAGE_TRANSFER_DST_BIT for staging mapping.");
                    return {};
                }
                if(mi.staging.capacity < bytes) {
                    mi.ew.report(ave_vk_map_buffer,
                        "Staging buffer capacity ({}) is smaller than mapped size ({}).",
                        mi.staging.capacity, bytes);
                    return {};
                }

                auto session = std::make_shared<typename Data::StagingSession>();
                session->target_state = state;
                session->dst_buffer = get_system_handle();
                session->dst_offset = mi.offset;
                session->src_buffer = mi.staging.buffer;
                session->src_offset = mi.staging.offset;
                session->size = bytes;
                session->upload_context = mi.upload_context;
                session->staging_lifetime = mi.staging.lifetime;

                if constexpr (requires { MemoryPolicy::try_acquire_stagedby(*state, session.get()); }) {
                    if(!MemoryPolicy::try_acquire_stagedby(*state, session.get())) {
                        mi.ew.report(ave_vk_map_buffer,
                            "Buffer conflict: Buffer is already being staged by another staging session.");
                        return {};
                    }
                } else {
                    mi.ew.report(ave_vk_map_buffer, "Memory policy does not support staging mapping.");
                    return {};
                }

                const auto chunk = get_device()->get_non_coherent_atom_size()
                                 * std::max(1u, mi.chunk_multiply);
                return Data(state, session, static_cast<uint8_t*>(mi.staging.mapped_ptr) + mi.staging.offset,
                            mi.offset, bytes, bytes, bytes, chunk);
            }

            if(!is_host_visible()) {
                mi.ew.report(ave_vk_map_buffer, "Buffer is uninitialized or not host visible.");
                return {};
            }
            void* base = nullptr;
            const auto code = MemoryPolicy::map(*state, &base);
            if(code != VK_SUCCESS) {
                mi.ew.report(ave_vk_map_buffer, "Failed to map Buffer ({}).", int(code));
                return {};
            }
            const auto chunk = get_device()->get_non_coherent_atom_size()
                             * std::max(1u, mi.chunk_multiply);
            try {
                return Data(state, static_cast<uint8_t*>(base) + mi.offset,
                            mi.offset, bytes, get_allocation_size() - mi.offset,
                            get_allocation_size(), chunk, is_host_coherent());
            } catch(...) {
                MemoryPolicy::unmap(*state);
                throw;
            }
        }

        /// 底层连续字节写入
        bool upload_bytes(const void* ptr, VkDeviceSize bytes, VkDeviceSize dst_offset = 0, MapBufferInfo mi = {}) {
            if(bytes == 0) return true;
            mi.offset = dst_offset;
            mi.size = bytes;
            auto mapped = map(mi);
            if(!mapped) return false;
            mapped.memcpy(0, ptr, bytes);
            mapped.cancel_auto_upload();
            return mapped.flush(mi.ew);
        }

        /// 仅映射写入；DeviceOnly 内存需要显式 staging copy。
        template<typename T>
        bool upload(const T& val, VkDeviceSize dst_offset = 0, MapBufferInfo mi = {}) {
            const auto span = detail::extract_pod_span(val);
            return upload_bytes(span.ptr, span.bytes, dst_offset, mi);
        }

        template<typename T>
        bool upload(const T& val, VkDeviceSize dst_offset, alib6::ErrorWrapper ew) {
            return upload(val, dst_offset, MapBufferInfo{ .ew = ew });
        }

        template<typename T>
        bool upload(VkDeviceSize dst_offset, const T& val, alib6::ErrorWrapper ew = {}) {
            return upload(val, dst_offset, MapBufferInfo{ .ew = ew });
        }

        /// @brief 通过成员指针自动反射其偏移与大小上传单个成员 (支持 base_offset 平移)
        template<auto MemberPtr, typename T>
        bool upload(
            const T& data,
            VkDeviceSize base_offset = 0,
            MapBufferInfo mi = {}
        ) {
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
        bool upload(const T& data, VkDeviceSize base_offset, alib6::ErrorWrapper ew) {
            return upload<MemberPtr>(data, base_offset, MapBufferInfo{ .ew = ew });
        }

        template<auto MemberPtr, typename T>
        bool upload(const T& data, alib6::ErrorWrapper ew) {
            return upload<MemberPtr>(data, 0, MapBufferInfo{ .ew = ew });
        }

        /// @brief 显式别名：上传单个成员 (通过成员指针自动反射偏移与大小)
        template<auto MemberPtr, typename T>
        bool upload_member(const T& data, VkDeviceSize base_offset = 0, MapBufferInfo mi = {}) {
            return upload<MemberPtr>(data, base_offset, mi);
        }

        template<auto MemberPtr, typename T>
        bool upload_member(const T& data, VkDeviceSize base_offset, alib6::ErrorWrapper ew) {
            return upload<MemberPtr>(data, base_offset, MapBufferInfo{ .ew = ew });
        }

        template<auto MemberPtr, typename T>
        bool upload_member(const T& data, alib6::ErrorWrapper ew) {
            return upload<MemberPtr>(data, 0, MapBufferInfo{ .ew = ew });
        }

        /// @brief 上传连续成员闭区间 [BeginPtr, EndPtr] (支持 base_offset 平移)
        template<auto BeginPtr, auto EndPtr, typename T>
        bool upload_range(
            const T& data,
            VkDeviceSize base_offset = 0,
            MapBufferInfo mi = {}
        ) {
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
        bool upload_range(const T& data, VkDeviceSize base_offset, alib6::ErrorWrapper ew) {
            return upload_range<BeginPtr, EndPtr>(data, base_offset, MapBufferInfo{ .ew = ew });
        }

        template<auto BeginPtr, auto EndPtr, typename T>
        bool upload_range(const T& data, alib6::ErrorWrapper ew) {
            return upload_range<BeginPtr, EndPtr>(data, 0, MapBufferInfo{ .ew = ew });
        }

        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept {
            return properties().device;
        }
        [[nodiscard]] VkBuffer get_system_handle() const noexcept { return properties().buffer; }
        /// 借用句柄；不得直接 free/map/unmap，VMA 后端可能与其他 Buffer 共享内存。
        [[nodiscard]] VkDeviceMemory get_memory() const noexcept { return properties().memory; }
        [[nodiscard]] VkDeviceSize get_memory_offset() const noexcept { return properties().memory_offset; }
        [[nodiscard]] VkDeviceSize get_size() const noexcept { return properties().size; }
        [[nodiscard]] VkDeviceSize get_allocation_size() const noexcept { return properties().allocation_size; }
        [[nodiscard]] VkBufferUsageFlags get_usage() const noexcept { return properties().usage; }
        [[nodiscard]] VkMemoryPropertyFlags get_memory_properties() const noexcept {
            return properties().memory_properties;
        }
        [[nodiscard]] bool is_host_visible() const noexcept {
            return (get_memory_properties() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
        }
        [[nodiscard]] bool is_host_coherent() const noexcept {
            return (get_memory_properties() & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
        }
        [[nodiscard]] explicit operator bool() const noexcept { return bool(state); }
    };

    #include "buffer_data.inl"

    using Buffer = BasicBuffer<NativeMemoryPolicy>;
    using VMABuffer = BasicBuffer<VmaMemoryPolicy>;
    using BufferData = BasicBufferData<NativeMemoryPolicy>;
    using VMABufferData = BasicBufferData<VmaMemoryPolicy>;

}
