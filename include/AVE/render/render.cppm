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

import :base;
import :instance;
import :debug_messenger;
import :surface;
import :physical_device;
import :device;
import :swapchain;
import :sync_objects;
import :legacy_render;
import :pipeline;
import :command;

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

        bool recording { false };
        bool finished { false };

        explicit GraphicsContext(alib6::ErrorWrapper target_ew);

    public:
        ~GraphicsContext();
        GraphicsContext(const GraphicsContext&) = delete;
        GraphicsContext& operator=(const GraphicsContext&) = delete;
        GraphicsContext(GraphicsContext&& other) noexcept;
        GraphicsContext& operator=(GraphicsContext&&) = delete;

        void begin(
            VkClearColorValue clear_color = {{ 0.02f, 0.02f, 0.03f, 1.0f }}
        );
        void bind_pipeline(const LegacyPipeline& pipeline);
        void draw(
            alib6::u32 vertex_count,
            alib6::u32 instance_count = 1,
            alib6::u32 first_vertex = 0,
            alib6::u32 first_instance = 0
        ) noexcept;
        void end();

        [[nodiscard]] alib6::u32 get_image_index() const noexcept;
        [[nodiscard]] VkExtent2D get_extent() const noexcept;
        [[nodiscard]] VkCommandBuffer get_command_buffer() const noexcept;
    };

    struct AVE_API Renderer {
        std::shared_ptr<Instance> instance;
        std::shared_ptr<DebugMessenger> debug_messenger;
        std::shared_ptr<Surface> surface;
        std::optional<PhysicalDeviceInfo> physical_device;
        std::shared_ptr<Device> device;
        std::shared_ptr<Swapchain> swapchain;
        std::shared_ptr<SyncObjects> sync_objects;
        std::shared_ptr<LegacyRender> legacy_render;
        std::shared_ptr<CommandPool> command_pool;
        std::shared_ptr<CommandBuffers> command_buffers;

        [[nodiscard]] GraphicsContext acquire_context(
            alib6::ErrorWrapper ew = {}
        );
        [[nodiscard]] bool invalidate_graphics_cache() noexcept;

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
            std::vector<FrameSyncObjects> frames;
            std::vector<VkFramebuffer> framebuffers;
            std::vector<VkCommandBuffer> command_buffers;
        } graphics_cache;

        [[nodiscard]] bool prepare_graphics_cache(
            alib6::ErrorWrapper& ew
        );
        std::size_t current_frame { 0 };
        std::vector<VkFence> images_in_flight;
        bool context_acquired { false };
    };
};
