/**
 * @file sync_objects.cppm
 * @brief Per-frame Vulkan synchronization objects
 * @version 5.0
 * @date 2026-09-11
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.render:sync_objects;

import std;
import alib6;
import :device;
import :swapchain;

export namespace ave {
    struct AVE_API FrameSyncObjects {
        VkSemaphore image_available { VK_NULL_HANDLE };
        VkSemaphore render_finished { VK_NULL_HANDLE };
        VkFence in_flight { VK_NULL_HANDLE };
    };

    class AVE_API WithSyncObjectsInput {
    private:
        std::shared_ptr<Device> device;
        std::shared_ptr<Swapchain> swapchain;

    public:
        WithSyncObjectsInput(
            std::shared_ptr<Device> target_device,
            std::shared_ptr<Swapchain> target_swapchain
        );
        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept;
        [[nodiscard]] const std::shared_ptr<Swapchain>& get_swapchain() const noexcept;
        [[nodiscard]] alib6::u32 get_swapchain_image_count() const noexcept;
    };

    using SelectSyncObjectsCount = std::function<alib6::u32(
        WithSyncObjectsInput&
    )>;

    [[nodiscard]] AVE_API alib6::u32 default_select_sync_objects_count(
        WithSyncObjectsInput& input
    );

    struct AVE_API CreateSyncObjectsInfo {
        std::shared_ptr<Device> device;
        alib6::u32 count { 0 };
        VkSemaphoreCreateFlags image_available_flags { 0 };
        VkSemaphoreCreateFlags render_finished_flags { 0 };
        VkFenceCreateFlags fence_flags { VK_FENCE_CREATE_SIGNALED_BIT };
        mutable alib6::ErrorWrapper ew {};
    };

    class AVE_API SyncObjects final {
    private:
        std::shared_ptr<Device> device;
        std::vector<FrameSyncObjects> frames;

        SyncObjects() = default;
        [[nodiscard]] bool initialize(CreateSyncObjectsInfo ci);

    public:
        ~SyncObjects();
        SyncObjects(const SyncObjects&) = delete;
        SyncObjects& operator=(const SyncObjects&) = delete;
        SyncObjects(SyncObjects&&) = delete;
        SyncObjects& operator=(SyncObjects&&) = delete;

        [[nodiscard]] static std::shared_ptr<SyncObjects> create(
            CreateSyncObjectsInfo ci
        );
        void destroy() noexcept;

        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept;
        [[nodiscard]] const std::vector<FrameSyncObjects>& get_frames() const noexcept;
        [[nodiscard]] alib6::u32 size() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;
    };
}
