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

export namespace ave{
    struct Renderer;

    class AVE_API GraphicsContext final {
    private:
        friend struct Renderer;

        Renderer* renderer { nullptr };
        alib6::ErrorWrapper ew {};
        VkResult result_code { VK_ERROR_INITIALIZATION_FAILED };
        VkResult acquire_result { VK_ERROR_INITIALIZATION_FAILED };
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
        void bind_vertex_buffer(
            const Buffer& buffer,
            VkDeviceSize offset = 0,
            alib6::u32 binding = 0
        ) noexcept;
        void bind_vertex_buffer(
            const BufferSlice& slice,
            alib6::u32 binding = 0
        ) noexcept;
        void bind_vertex_buffers(
            alib6::u32 first_binding,
            std::span<const VkBuffer> buffers,
            std::span<const VkDeviceSize> offsets
        ) noexcept;

        void bind_index_buffer(
            const Buffer& buffer,
            VkDeviceSize offset = 0,
            VkIndexType index_type = VK_INDEX_TYPE_UINT32
        ) noexcept;
        void bind_index_buffer(
            const BufferSlice& slice,
            VkIndexType index_type = VK_INDEX_TYPE_UINT32
        ) noexcept;

        inline void bind_indice_buffer(
            const Buffer& buffer,
            VkDeviceSize offset = 0,
            VkIndexType index_type = VK_INDEX_TYPE_UINT32
        ) noexcept {
            bind_index_buffer(buffer, offset, index_type);
        }
        inline void bind_indice_buffer(
            const BufferSlice& slice,
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
        void draw_indirect(
            const Buffer& buffer,
            VkDeviceSize offset = 0,
            alib6::u32 draw_count = 1,
            alib6::u32 stride = sizeof(VkDrawIndirectCommand)
        ) noexcept;
        void draw_indexed_indirect(
            const Buffer& buffer,
            VkDeviceSize offset = 0,
            alib6::u32 draw_count = 1,
            alib6::u32 stride = sizeof(VkDrawIndexedIndirectCommand)
        ) noexcept;
        void end();

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
