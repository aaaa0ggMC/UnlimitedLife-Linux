/**
 * @file render.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 提供profile来创建一个完整的Render
 * @version 5.0
 * @date 2026-09-10
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>
#include <alib6/debug.h>

export module ave.render:render;
import ave.context;
import alib6;
import std;

import ave.render.base;
import :instance;
import :debug_messenger;
import :surface;
import :physical_device;
import :device;
import :swapchain;
import :image;
import :sync_objects;
import :legacy_render;
import :dynamic_render;
import :pipeline;
import :command;
import :buffer;
import :buffer_slice;
import ave.reflect;

export namespace ave{
    struct Renderer;

    class AVE_API GraphicsContext final {
    private:
        friend struct Renderer;

        Renderer* renderer { nullptr };
        alib6::ErrorWrapper ew {};
        VkResult result_code { VK_ERROR_INITIALIZATION_FAILED };
        VkResult acquire_result { VK_ERROR_INITIALIZATION_FAILED };
        VkResult present_result { VK_NOT_READY };
        alib6::u32 image_index { 0 };
        std::size_t frame_index { 0 };
        std::size_t frame_count { 0 };
        const Device* device_owner { nullptr };

        VkDevice device { VK_NULL_HANDLE };
        VkQueue graphics_queue { VK_NULL_HANDLE };
        VkQueue present_queue { VK_NULL_HANDLE };
        VkSwapchainKHR swapchain { VK_NULL_HANDLE };
        VkRenderPass render_pass { VK_NULL_HANDLE };
        VkFramebuffer framebuffer { VK_NULL_HANDLE };
        VkCommandBuffer command_buffer { VK_NULL_HANDLE };
        VkSemaphore image_available { VK_NULL_HANDLE };
        VkSemaphore render_finished { VK_NULL_HANDLE };
        VkFence in_flight { VK_NULL_HANDLE };
        VkExtent2D extent {};
        std::span<const VkClearValue> default_clear_values {};
        VkFence* images_in_flight { nullptr };

        bool dynamic_rendering { false };
        PFN_vkCmdBeginRendering pfn_cmd_begin_rendering { nullptr };
        PFN_vkCmdEndRendering pfn_cmd_end_rendering { nullptr };
        VkImage swapchain_image { VK_NULL_HANDLE };
        VkImageView swapchain_view { VK_NULL_HANDLE };
        VkImage depth_image { VK_NULL_HANDLE };
        VkImageView depth_view { VK_NULL_HANDLE };
        VkFormat depth_format { VK_FORMAT_UNDEFINED };
        VkImageAspectFlags depth_aspect { 0 };

        bool recording { false };
        bool finished { false };
        const Pipeline* bound_pipeline { nullptr };

        explicit GraphicsContext(alib6::ErrorWrapper target_ew);

    public:
        ~GraphicsContext();
        GraphicsContext(const GraphicsContext&) = delete;
        GraphicsContext& operator=(const GraphicsContext&) = delete;
        GraphicsContext(GraphicsContext&& other) noexcept;
        GraphicsContext& operator=(GraphicsContext&&) = delete;

        void begin();
        void begin(
            VkClearColorValue clear_color
        );
        void begin(std::span<const VkClearValue> clear_values);
        void bind_pipeline(const Pipeline& pipeline);
        template<class MemoryPolicy>
        void bind_vertex_buffer(
            const BasicBuffer<MemoryPolicy>& buffer,
            VkDeviceSize offset = 0,
            alib6::u32 binding = 0
        ) noexcept {
            panic_debug(!recording, "Cannot bind vertex buffer outside of an active recording scope.");
            panic_debug(
                (buffer.get_usage() & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) == 0,
                "Cannot bind a Buffer as vertex buffer without VK_BUFFER_USAGE_VERTEX_BUFFER_BIT."
            );
            if(!recording) return;
            VkBuffer handle = buffer.get_system_handle();
            if(handle != VK_NULL_HANDLE) {
                vkCmdBindVertexBuffers(command_buffer, binding, 1, &handle, &offset);
            }
        }
        template<class MemoryPolicy>
        void bind_vertex_buffer(
            const BasicBufferSlice<MemoryPolicy>& slice,
            alib6::u32 binding = 0
        ) noexcept {
            panic_debug(!recording, "Cannot bind vertex buffer outside of an active recording scope.");
            panic_debug(
                !slice.get_buffer() || (slice.get_buffer()->get_usage() & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) == 0,
                "Cannot bind a BufferSlice as vertex buffer without VK_BUFFER_USAGE_VERTEX_BUFFER_BIT."
            );
            if(!recording) return;
            VkBuffer handle = slice.get_system_handle();
            VkDeviceSize offset = slice.get_offset();
            if(handle != VK_NULL_HANDLE) {
                vkCmdBindVertexBuffers(command_buffer, binding, 1, &handle, &offset);
            }
        }
        void bind_vertex_buffers(
            alib6::u32 first_binding,
            std::span<const VkBuffer> buffers,
            std::span<const VkDeviceSize> offsets
        ) noexcept;

        template<class MemoryPolicy>
        void bind_index_buffer(
            const BasicBuffer<MemoryPolicy>& buffer,
            VkDeviceSize offset = 0,
            VkIndexType index_type = VK_INDEX_TYPE_UINT32
        ) noexcept {
            panic_debug(!recording, "Cannot bind index buffer outside of an active recording scope.");
            panic_debug(
                (buffer.get_usage() & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) == 0,
                "Cannot bind a Buffer as index buffer without VK_BUFFER_USAGE_INDEX_BUFFER_BIT."
            );
            if(!recording) return;
            VkBuffer handle = buffer.get_system_handle();
            if(handle != VK_NULL_HANDLE) {
                vkCmdBindIndexBuffer(command_buffer, handle, offset, index_type);
            }
        }
        template<class MemoryPolicy>
        void bind_index_buffer(
            const BasicBufferSlice<MemoryPolicy>& slice,
            VkIndexType index_type = VK_INDEX_TYPE_UINT32
        ) noexcept {
            panic_debug(!recording, "Cannot bind index buffer outside of an active recording scope.");
            panic_debug(
                !slice.get_buffer() || (slice.get_buffer()->get_usage() & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) == 0,
                "Cannot bind a BufferSlice as index buffer without VK_BUFFER_USAGE_INDEX_BUFFER_BIT."
            );
            if(!recording) return;
            VkBuffer handle = slice.get_system_handle();
            VkDeviceSize offset = slice.get_offset();
            if(handle != VK_NULL_HANDLE) {
                vkCmdBindIndexBuffer(command_buffer, handle, offset, index_type);
            }
        }

        template<class MemoryPolicy>
        inline void bind_indice_buffer(
            const BasicBuffer<MemoryPolicy>& buffer,
            VkDeviceSize offset = 0,
            VkIndexType index_type = VK_INDEX_TYPE_UINT32
        ) noexcept {
            bind_index_buffer(buffer, offset, index_type);
        }
        template<class MemoryPolicy>
        inline void bind_indice_buffer(
            const BasicBufferSlice<MemoryPolicy>& slice,
            VkIndexType index_type = VK_INDEX_TYPE_UINT32
        ) noexcept {
            bind_index_buffer(slice, index_type);
        }

        void draw(
            alib6::u32 vertex_count,
            alib6::u32 instance_count = 1,
            alib6::u32 first_vertex = 0,
            alib6::u32 first_instance = 0
        ) noexcept;
        void draw_indexed(
            alib6::u32 index_count,
            alib6::u32 instance_count = 1,
            alib6::u32 first_index = 0,
            alib6::i32 vertex_offset = 0,
            alib6::u32 first_instance = 0
        ) noexcept;
        template<class MemoryPolicy>
        void draw_indirect(
            const BasicBuffer<MemoryPolicy>& buffer,
            VkDeviceSize offset = 0,
            alib6::u32 draw_count = 1,
            alib6::u32 stride = sizeof(VkDrawIndirectCommand)
        ) noexcept {
            panic_debug(!recording, "Cannot draw indirect outside of an active recording scope.");
            panic_debug(
                (buffer.get_usage() & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) == 0,
                "Cannot use a Buffer for indirect draw without VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT."
            );
            if(!recording) return;
            VkBuffer handle = buffer.get_system_handle();
            if(handle != VK_NULL_HANDLE) {
                vkCmdDrawIndirect(command_buffer, handle, offset, draw_count, stride);
            }
        }
        template<class MemoryPolicy>
        void draw_indexed_indirect(
            const BasicBuffer<MemoryPolicy>& buffer,
            VkDeviceSize offset = 0,
            alib6::u32 draw_count = 1,
            alib6::u32 stride = sizeof(VkDrawIndexedIndirectCommand)
        ) noexcept {
            panic_debug(!recording, "Cannot draw indexed indirect outside of an active recording scope.");
            panic_debug(
                (buffer.get_usage() & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) == 0,
                "Cannot use a Buffer for indexed indirect draw without VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT."
            );
            if(!recording) return;
            VkBuffer handle = buffer.get_system_handle();
            if(handle != VK_NULL_HANDLE) {
                vkCmdDrawIndexedIndirect(command_buffer, handle, offset, draw_count, stride);
            }
        }
        void end();

        /// @brief 获取最后一次执行操作的综合 VkResult 结果码 (Acquire / Submit / Present)
        [[nodiscard]] inline VkResult get_result() const noexcept { return result_code; }

        /// @brief 获取 vkAcquireNextImageKHR 的原始结果码
        [[nodiscard]] inline VkResult get_acquire_result() const noexcept { return acquire_result; }

        /// @brief 获取 vkQueuePresentKHR 的原始结果码 (若尚未呈现则为 VK_NOT_READY)
        [[nodiscard]] inline VkResult get_present_result() const noexcept { return present_result; }

        /// @brief 当前帧上下文是否有效可用 (可安全开始录制并提交渲染)
        [[nodiscard]] inline bool is_valid() const noexcept {
            return renderer != nullptr && (acquire_result == VK_SUCCESS || acquire_result == VK_SUBOPTIMAL_KHR);
        }

        /// @brief 隐式布尔转换 (等价于 is_valid())
        [[nodiscard]] inline explicit operator bool() const noexcept {
            return is_valid();
        }

        /// @brief 检查交换链是否因窗口尺寸变化或表面失效而过时 (VK_ERROR_OUT_OF_DATE_KHR)
        [[nodiscard]] inline bool is_out_of_date() const noexcept {
            return acquire_result == VK_ERROR_OUT_OF_DATE_KHR ||
                   present_result == VK_ERROR_OUT_OF_DATE_KHR ||
                   result_code == VK_ERROR_OUT_OF_DATE_KHR;
        }

        /// @brief 检查是否处于亚最优状态 (VK_SUBOPTIMAL_KHR，建议在合适时机重建交换链)
        [[nodiscard]] inline bool is_suboptimal() const noexcept {
            return acquire_result == VK_SUBOPTIMAL_KHR ||
                   present_result == VK_SUBOPTIMAL_KHR ||
                   result_code == VK_SUBOPTIMAL_KHR;
        }

        /// @brief 检查本帧整体流程是否完全成功 (VK_SUCCESS)
        [[nodiscard]] inline bool is_success() const noexcept {
            return result_code == VK_SUCCESS;
        }

        [[nodiscard]] inline const Pipeline* get_bound_pipeline() const noexcept { return bound_pipeline; }
        [[nodiscard]] inline alib6::u32 get_push_constant_size() const noexcept {
            return bound_pipeline ? bound_pipeline->get_push_constant_size() : 0;
        }

        void push_constant_raw(
            VkShaderStageFlags stage_flags,
            alib6::u32 offset,
            alib6::u32 size,
            const void* data
        ) noexcept;

        template<typename T>
        inline void push_constant(
            const T& data,
            VkShaderStageFlags stage_flags = 0,
            alib6::u32 offset = 0
        ) noexcept {
            push_constant_raw(stage_flags, offset, static_cast<alib6::u32>(sizeof(T)), std::addressof(data));
        }

        /// @brief 上传单个成员 (通过成员指针自动反射其偏移与大小，支持 base offset 平移)
        template<auto MemberPtr, typename T>
        inline void push_constant(
            const T& data,
            VkShaderStageFlags stage_flags = 0,
            alib6::u32 offset = 0
        ) noexcept {
            using Traits = member_pointer_traits<decltype(MemberPtr)>;
            using ClassType = typename Traits::class_type;
            constexpr auto info = get_member_range_info<MemberPtr, MemberPtr>();
            const auto target_offset = static_cast<alib6::u32>(offset + info.offset);

            if constexpr (std::is_same_v<std::remove_cvref_t<T>, ClassType>) {
                const void* ptr = reinterpret_cast<const char*>(std::addressof(data)) + info.offset;
                push_constant_raw(stage_flags, target_offset, static_cast<alib6::u32>(info.size), ptr);
            } else {
                static_assert(sizeof(T) == info.size, "Passed data size must match the push constant member size.");
                push_constant_raw(stage_flags, target_offset, static_cast<alib6::u32>(info.size), std::addressof(data));
            }
        }

        /// @brief 上传连续成员闭区间 [BeginPtr, EndPtr] (支持 base offset 平移)
        template<auto BeginPtr, auto EndPtr, typename T>
        inline void push_constant(
            const T& data,
            VkShaderStageFlags stage_flags = 0,
            alib6::u32 offset = 0
        ) noexcept {
            using BeginTraits = member_pointer_traits<decltype(BeginPtr)>;
            using ClassType = typename BeginTraits::class_type;
            constexpr auto info = get_member_range_info<BeginPtr, EndPtr>();
            const auto target_offset = static_cast<alib6::u32>(offset + info.offset);

            if constexpr (std::is_same_v<std::remove_cvref_t<T>, ClassType>) {
                const void* ptr = reinterpret_cast<const char*>(std::addressof(data)) + info.offset;
                push_constant_raw(stage_flags, target_offset, static_cast<alib6::u32>(info.size), ptr);
            } else {
                static_assert(sizeof(T) == info.size, "Passed data size must match the push constant range size.");
                push_constant_raw(stage_flags, target_offset, static_cast<alib6::u32>(info.size), std::addressof(data));
            }
        }

        /// @brief 显式别名：上传单个成员 (支持 base offset 平移)
        template<auto MemberPtr, typename T>
        inline void push_constant_member(
            const T& data,
            VkShaderStageFlags stage_flags = 0,
            alib6::u32 offset = 0
        ) noexcept {
            push_constant<MemberPtr>(data, stage_flags, offset);
        }

        /// @brief 显式别名：上传连续成员闭区间 (支持 base offset 平移)
        template<auto BeginPtr, auto EndPtr, typename T>
        inline void push_constant_range(
            const T& data,
            VkShaderStageFlags stage_flags = 0,
            alib6::u32 offset = 0
        ) noexcept {
            push_constant<BeginPtr, EndPtr>(data, stage_flags, offset);
        }

        [[nodiscard]] alib6::u32 get_image_index() const noexcept;
        [[nodiscard]] VkExtent2D get_extent() const noexcept;
        [[nodiscard]] VkCommandBuffer get_command_buffer() const noexcept;
    };

    struct AVE_API Renderer {
        Context* context { nullptr };
        std::shared_ptr<Instance> instance;
        std::shared_ptr<DebugMessenger> debug_messenger;
        std::shared_ptr<Surface> surface;
        std::optional<PhysicalDeviceInfo> physical_device;
        std::shared_ptr<Device> device;
        std::shared_ptr<Swapchain> swapchain;
        std::vector<std::shared_ptr<Image>> images;
        std::shared_ptr<SyncObjects> sync_objects;
        std::shared_ptr<LegacyRender> legacy_render;
        std::shared_ptr<DynamicRender> dynamic_render;
        std::shared_ptr<CommandPool> command_pool;
        std::shared_ptr<CommandBuffers> command_buffers;
        std::vector<VkClearValue> default_clear_values;

        [[nodiscard]] bool supports_dynamic_rendering() const noexcept {
            return device && device->supports_dynamic_rendering();
        }

        [[nodiscard]] GraphicsContext acquire_context(
            alib6::ErrorWrapper ew = {}
        );
        [[nodiscard]] bool invalidate_graphics_cache() noexcept;

        // Render backend creation
        std::shared_ptr<LegacyRender> create_legacy_render(
            ConfigureLegacyRender configure = default_configure_legacy_render,
            LegacyRenderCreateStatus* status = nullptr,
            alib6::ErrorWrapper ew = {}
        );

        std::shared_ptr<DynamicRender> create_dynamic_render(
            ConfigureDynamicRender configure = default_configure_dynamic_render,
            alib6::ErrorWrapper ew = {}
        );

        // Dynamic graphics pipeline creation (forwards to dynamic_render, panics if not in dynamic mode)
        [[nodiscard]] std::shared_ptr<DynamicPipeline> create_dynamic_graphics_pipeline(
            GraphicsShaderBytecode shaders,
            ConfigureDynamicPipeline configure = nullptr
        );
        [[nodiscard]] std::shared_ptr<DynamicPipeline> create_dynamic_graphics_pipeline(
            ShaderBytecode vertex,
            ShaderBytecode fragment,
            ShaderBytecode geometry = {},
            TessellationShaderBytecode tessellation = {},
            ConfigureDynamicPipeline configure = nullptr
        );
        [[nodiscard]] std::shared_ptr<DynamicPipeline> create_dynamic_graphics_pipeline(
            GraphicsShaderPaths shaders,
            ConfigureDynamicPipeline configure = nullptr
        );
        [[nodiscard]] std::shared_ptr<DynamicPipeline> create_dynamic_graphics_pipeline(
            std::string_view vertex,
            std::string_view fragment,
            std::string_view geometry = {},
            TessellationShaderPaths tessellation = {},
            ConfigureDynamicPipeline configure = nullptr
        );

        template<typename... Args>
        [[nodiscard]] auto create_dynamic_pipeline(Args&&... args) {
            return create_dynamic_graphics_pipeline(std::forward<Args>(args)...);
        }

        // Legacy graphics pipeline creation (forwards to legacy_render, panics if not in legacy mode)
        [[nodiscard]] std::shared_ptr<LegacyPipeline> create_legacy_graphics_pipeline(
            GraphicsShaderBytecode shaders,
            ConfigureLegacyPipeline configure = nullptr
        );
        [[nodiscard]] std::shared_ptr<LegacyPipeline> create_legacy_graphics_pipeline(
            ShaderBytecode vertex,
            ShaderBytecode fragment,
            ShaderBytecode geometry = {},
            TessellationShaderBytecode tessellation = {},
            ConfigureLegacyPipeline configure = nullptr
        );
        [[nodiscard]] std::shared_ptr<LegacyPipeline> create_legacy_graphics_pipeline(
            GraphicsShaderPaths shaders,
            ConfigureLegacyPipeline configure = nullptr
        );
        [[nodiscard]] std::shared_ptr<LegacyPipeline> create_legacy_graphics_pipeline(
            std::string_view vertex,
            std::string_view fragment,
            std::string_view geometry = {},
            TessellationShaderPaths tessellation = {},
            ConfigureLegacyPipeline configure = nullptr
        );

        template<typename... Args>
        [[nodiscard]] auto create_legacy_pipeline(Args&&... args) {
            return create_legacy_graphics_pipeline(std::forward<Args>(args)...);
        }

        // Graceful-degradation graphics pipeline creation (returns Pipeline base, dynamically branches)
        [[nodiscard]] std::shared_ptr<Pipeline> create_graphics_pipeline(
            GraphicsShaderBytecode shaders,
            ConfigureGraphicsPipeline configure = nullptr
        );
        [[nodiscard]] std::shared_ptr<Pipeline> create_graphics_pipeline(
            ShaderBytecode vertex,
            ShaderBytecode fragment,
            ShaderBytecode geometry = {},
            TessellationShaderBytecode tessellation = {},
            ConfigureGraphicsPipeline configure = nullptr
        );
        [[nodiscard]] std::shared_ptr<Pipeline> create_graphics_pipeline(
            GraphicsShaderPaths shaders,
            ConfigureGraphicsPipeline configure = nullptr
        );
        [[nodiscard]] std::shared_ptr<Pipeline> create_graphics_pipeline(
            std::string_view vertex,
            std::string_view fragment,
            std::string_view geometry = {},
            TessellationShaderPaths tessellation = {},
            ConfigureGraphicsPipeline configure = nullptr
        );
        [[nodiscard]] std::shared_ptr<Pipeline> create_graphics_pipeline(
            GraphicsPipelineConfig config
        );

    private:
        friend class GraphicsContext;
        struct GraphicsCache {
            bool ready { false };
            const Device* device_owner { nullptr };
            VkDevice device { VK_NULL_HANDLE };
            VkQueue graphics_queue { VK_NULL_HANDLE };
            VkQueue present_queue { VK_NULL_HANDLE };
            VkSwapchainKHR swapchain { VK_NULL_HANDLE };
            VkRenderPass render_pass { VK_NULL_HANDLE };
            VkExtent2D extent {};
            std::size_t image_count { 0 };
            std::vector<VkClearValue> default_clear_values;
            std::vector<FrameSyncObjects> frames;
            std::vector<VkFramebuffer> framebuffers;
            std::vector<VkCommandBuffer> command_buffers;
            bool dynamic_rendering { false };
            PFN_vkCmdBeginRendering pfn_cmd_begin_rendering { nullptr };
            PFN_vkCmdEndRendering pfn_cmd_end_rendering { nullptr };
            std::vector<VkImage> swapchain_images;
            std::vector<VkImageView> swapchain_views;
            VkImage depth_image { VK_NULL_HANDLE };
            VkImageView depth_view { VK_NULL_HANDLE };
            VkFormat depth_format { VK_FORMAT_UNDEFINED };
            VkImageAspectFlags depth_aspect { 0 };
        } graphics_cache;

        [[nodiscard]] bool prepare_graphics_cache(
            alib6::ErrorWrapper& ew
        );
        std::size_t current_frame { 0 };
        std::vector<VkFence> images_in_flight;
        bool context_acquired { false };
    };
};
