#pragma once

#include <AVE/alib.h>
#include <AVE/render/device.h>
#include <AVE/render/swapchain.h>
#include <AVE/render/sync_objects.h>

/**
 * @file command.cppm
 * @brief Vulkan command pool and command buffer RAII wrappers
 * @version 5.0
 * @date 2026-09-11
 */
#include <AVE/config.h>
#include <vulkan/vulkan.h>



namespace ave {
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
        [[nodiscard]] ave::u32 get_graphics_queue_family() const noexcept;
    };

    struct AVE_API CreateCommandPoolInfo {
        std::shared_ptr<Device> device;
        ave::u32 queue_family { 0 };
        VkCommandPoolCreateFlags flags {
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        };
        mutable ave::ErrorWrapper ew {};
    };

    AVE_API void default_configure_command_pool(
        WithCommandPoolInput& input,
        CreateCommandPoolInfo& ci
    );

    class AVE_API CommandPool final {
    private:
        std::shared_ptr<Device> device;
        VkCommandPool pool { VK_NULL_HANDLE };
        ave::u32 queue_family { 0 };
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
        [[nodiscard]] ave::u32 get_queue_family() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;
    };

    struct AVE_API CreateCommandBuffersInfo {
        std::shared_ptr<CommandPool> pool;
        ave::u32 count { 0 };
        VkCommandBufferLevel level { VK_COMMAND_BUFFER_LEVEL_PRIMARY };
        mutable ave::ErrorWrapper ew {};
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
        [[nodiscard]] ave::u32 get_frame_count() const noexcept;
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
        [[nodiscard]] ave::u32 size() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;

        /// @brief 重置第 @p index 个命令缓冲；失败时写入 @p ew。
        bool reset(ave::u32 index,
                   VkCommandBufferResetFlags flags = 0,
                   ave::ErrorWrapper ew = {}) const;
        /// @brief 开始录制第 @p index 个命令缓冲；默认一次性提交。
        bool begin(ave::u32 index,
                   VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                   ave::ErrorWrapper ew = {}) const;
        /// @brief 结束第 @p index 个命令缓冲的录制。
        bool end(ave::u32 index, ave::ErrorWrapper ew = {}) const;
        /// @brief 提交第 @p index 个命令缓冲到 @p queue 并等待执行完成（同步读回/回归用）。
        bool submit_and_wait(ave::u32 index,
                             VkQueue queue,
                             ave::ErrorWrapper ew = {}) const;
    };

    /// @brief 一次 vkCmdPipelineBarrier 的全局内存屏障（执行 / 访问依赖）。
    struct AVE_API MemoryBarrier {
        VkPipelineStageFlags src_stage { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
        VkAccessFlags src_access { 0 };
        VkPipelineStageFlags dst_stage { VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT };
        VkAccessFlags dst_access { 0 };
    };

    /// @brief 图像布局迁移；子资源默认单 mip、单层，队列族固定为 IGNORED。
    struct AVE_API ImageBarrier {
        VkImage image { VK_NULL_HANDLE };
        VkImageLayout old_layout { VK_IMAGE_LAYOUT_UNDEFINED };
        VkImageLayout new_layout { VK_IMAGE_LAYOUT_GENERAL };
        VkImageAspectFlags aspect { VK_IMAGE_ASPECT_COLOR_BIT };
        VkPipelineStageFlags src_stage { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
        VkPipelineStageFlags dst_stage { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
        VkAccessFlags src_access { 0 };
        VkAccessFlags dst_access { 0 };
        ave::u32 base_mip { 0 };
        ave::u32 mip_count { 1 };
        ave::u32 base_layer { 0 };
        ave::u32 layer_count { 1 };
    };

    /// @brief 记录一次全局内存屏障。
    AVE_API void memory_barrier(
        VkCommandBuffer command_buffer,
        const MemoryBarrier& barrier
    ) noexcept;
    /// @brief 记录一次图像布局迁移。
    AVE_API void image_barrier(
        VkCommandBuffer command_buffer,
        const ImageBarrier& barrier
    ) noexcept;
    /// @brief 记录一批图像布局迁移；stage 取各条目的并集，只产生一次 vkCmdPipelineBarrier。
    AVE_API void image_barriers(
        VkCommandBuffer command_buffer,
        std::span<const ImageBarrier> barriers
    ) noexcept;
}
