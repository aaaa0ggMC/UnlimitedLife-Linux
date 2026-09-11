/**
 * @file command.cppm
 * @brief Vulkan command pool and command buffer RAII wrappers
 * @version 5.0
 * @date 2026-09-11
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.render:command;

import std;
import alib6;
import :device;
import :swapchain;
import :sync_objects;

export namespace ave {
    class AVE_API WithCommandPoolInput {
    private:
        std::shared_ptr<Device> device;
        std::shared_ptr<Swapchain> swapchain;

    public:
        WithCommandPoolInput(
            std::shared_ptr<Device> target_device,
            std::shared_ptr<Swapchain> target_swapchain
        );
        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept;
        [[nodiscard]] const std::shared_ptr<Swapchain>& get_swapchain() const noexcept;
        [[nodiscard]] alib6::u32 get_graphics_queue_family() const noexcept;
    };

    struct AVE_API CreateCommandPoolInfo {
        std::shared_ptr<Device> device;
        alib6::u32 queue_family { 0 };
        VkCommandPoolCreateFlags flags {
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        };
        mutable alib6::ErrorWrapper ew {};
    };

    AVE_API void default_configure_command_pool(
        WithCommandPoolInput& input,
        CreateCommandPoolInfo& ci
    );

    class AVE_API CommandPool final {
    private:
        std::shared_ptr<Device> device;
        VkCommandPool pool { VK_NULL_HANDLE };
        alib6::u32 queue_family { 0 };
        CommandPool() = default;
        [[nodiscard]] bool initialize(CreateCommandPoolInfo ci);
    public:
        ~CommandPool();
        CommandPool(const CommandPool&) = delete;
        CommandPool& operator=(const CommandPool&) = delete;
        CommandPool(CommandPool&&) = delete;
        CommandPool& operator=(CommandPool&&) = delete;

        [[nodiscard]] static std::shared_ptr<CommandPool> create(
            CreateCommandPoolInfo ci
        );
        void destroy() noexcept;
        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept;
        [[nodiscard]] VkCommandPool get_system_handle() const noexcept;
        [[nodiscard]] alib6::u32 get_queue_family() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;
    };

    struct AVE_API CreateCommandBuffersInfo {
        std::shared_ptr<CommandPool> pool;
        alib6::u32 count { 0 };
        VkCommandBufferLevel level { VK_COMMAND_BUFFER_LEVEL_PRIMARY };
        mutable alib6::ErrorWrapper ew {};
    };

    class AVE_API WithCommandBuffersInput {
    private:
        std::shared_ptr<CommandPool> pool;
        std::shared_ptr<SyncObjects> sync_objects;

    public:
        WithCommandBuffersInput(
            std::shared_ptr<CommandPool> target_pool,
            std::shared_ptr<SyncObjects> target_sync_objects
        );
        [[nodiscard]] const std::shared_ptr<CommandPool>& get_pool() const noexcept;
        [[nodiscard]] const std::shared_ptr<SyncObjects>& get_sync_objects() const noexcept;
        [[nodiscard]] alib6::u32 get_frame_count() const noexcept;
    };

    AVE_API void default_configure_command_buffers(
        WithCommandBuffersInput& input,
        CreateCommandBuffersInfo& ci
    );

    class AVE_API CommandBuffers final {
    private:
        std::shared_ptr<CommandPool> pool;
        std::vector<VkCommandBuffer> buffers;
        CommandBuffers() = default;
        [[nodiscard]] bool initialize(CreateCommandBuffersInfo ci);
    public:
        ~CommandBuffers();
        CommandBuffers(const CommandBuffers&) = delete;
        CommandBuffers& operator=(const CommandBuffers&) = delete;
        CommandBuffers(CommandBuffers&&) = delete;
        CommandBuffers& operator=(CommandBuffers&&) = delete;

        [[nodiscard]] static std::shared_ptr<CommandBuffers> create(
            CreateCommandBuffersInfo ci
        );
        void destroy() noexcept;
        [[nodiscard]] const std::shared_ptr<CommandPool>& get_pool() const noexcept;
        [[nodiscard]] const std::vector<VkCommandBuffer>& get_buffers() const noexcept;
        [[nodiscard]] alib6::u32 size() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;
    };
}
