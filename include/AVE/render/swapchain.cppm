/**
 * @file swapchain.cppm
 * @brief Vulkan swapchain and image-view RAII wrapper
 * @version 5.0
 * @date 2026-09-11
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.render:swapchain;

import std;
import alib6;
import :device;
import :surface;

export namespace ave {
    struct AVE_API WithSwapchain {
        VkSurfaceCapabilitiesKHR capabilities {};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
        VkExtent2D framebuffer_extent {};
        std::optional<alib6::u32> graphics_queue_family;
        std::optional<alib6::u32> present_queue_family;

        [[nodiscard]] bool supports(VkSurfaceFormatKHR value) const noexcept;
        [[nodiscard]] bool supports(VkPresentModeKHR value) const noexcept;
    };

    class Swapchain;

    struct AVE_API CreateSwapchainInfo {
        std::shared_ptr<Device> device;
        std::shared_ptr<Surface> surface;

        VkSurfaceFormatKHR surface_format {};
        VkPresentModeKHR present_mode { VK_PRESENT_MODE_FIFO_KHR };
        VkExtent2D extent {};
        alib6::u32 image_count { 0 };
        alib6::u32 image_array_layers { 1 };
        VkImageUsageFlags image_usage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT };
        VkSwapchainCreateFlagsKHR flags { 0 };
        VkSharingMode sharing_mode { VK_SHARING_MODE_EXCLUSIVE };
        std::vector<alib6::u32> queue_family_indices;
        VkSurfaceTransformFlagBitsKHR pre_transform {
            VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
        };
        VkCompositeAlphaFlagBitsKHR composite_alpha {
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
        };
        bool clipped { true };

        std::shared_ptr<Swapchain> old_swapchain;
        mutable alib6::ErrorWrapper ew {};
    };

    [[nodiscard]] AVE_API std::optional<WithSwapchain>
    query_swapchain_support(
        const std::shared_ptr<Device>& device,
        const std::shared_ptr<Surface>& surface,
        alib6::ErrorWrapper ew = {}
    );

    /// LearnVulkan2 风格的完整默认配置；自定义回调可先调用它再覆盖字段。
    AVE_API void default_configure_swapchain(
        WithSwapchain& with,
        CreateSwapchainInfo& ci
    );

    class AVE_API Swapchain final {
    private:
        std::shared_ptr<Device> device;
        std::shared_ptr<Surface> surface;
        VkSwapchainKHR swapchain { VK_NULL_HANDLE };
        alib6::u32 image_count { 0 };
        VkImageUsageFlags image_usage { 0 };
        VkSurfaceFormatKHR surface_format {};
        VkPresentModeKHR present_mode { VK_PRESENT_MODE_FIFO_KHR };
        VkExtent2D extent {};
        alib6::u32 graphics_queue_family { 0 };
        alib6::u32 present_queue_family { 0 };

        Swapchain() = default;
        [[nodiscard]] bool initialize(
            CreateSwapchainInfo ci,
            const WithSwapchain& with
        );

    public:
        ~Swapchain();
        Swapchain(const Swapchain&) = delete;
        Swapchain& operator=(const Swapchain&) = delete;
        Swapchain(Swapchain&&) = delete;
        Swapchain& operator=(Swapchain&&) = delete;

        [[nodiscard]] static std::shared_ptr<Swapchain> create(
            CreateSwapchainInfo ci,
            const WithSwapchain& with
        );
        void destroy() noexcept;

        [[nodiscard]] VkSwapchainKHR get_system_handle() const noexcept;
        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept;
        [[nodiscard]] const std::shared_ptr<Surface>& get_surface() const noexcept;
        [[nodiscard]] alib6::u32 get_image_count() const noexcept;
        [[nodiscard]] VkImageUsageFlags get_image_usage() const noexcept;
        [[nodiscard]] std::optional<std::vector<VkImage>> enumerate_images(
            alib6::ErrorWrapper ew = {}
        ) const;
        [[nodiscard]] VkSurfaceFormatKHR get_surface_format() const noexcept;
        [[nodiscard]] VkPresentModeKHR get_present_mode() const noexcept;
        [[nodiscard]] VkExtent2D get_extent() const noexcept;
        [[nodiscard]] alib6::u32 get_graphics_queue_family() const noexcept;
        [[nodiscard]] alib6::u32 get_present_queue_family() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;
    };
}
