/**
 * @file image.cppm
 * @brief Vulkan owned image, memory and image-view RAII wrapper
 * @version 5.0
 * @date 2026-09-12
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.render:image;

import std;
import alib6;
import :device;
import :swapchain;

export namespace ave {
    struct AVE_API CreateImageInfo {
        std::shared_ptr<Device> device;

        const void* next { nullptr };
        VkImageCreateFlags flags { 0 };
        VkImageType image_type { VK_IMAGE_TYPE_2D };
        VkFormat format { VK_FORMAT_UNDEFINED };
        VkExtent3D extent {};
        alib6::u32 mip_levels { 1 };
        alib6::u32 array_layers { 1 };
        VkSampleCountFlagBits samples { VK_SAMPLE_COUNT_1_BIT };
        VkImageTiling tiling { VK_IMAGE_TILING_OPTIMAL };
        VkImageUsageFlags usage { 0 };
        VkSharingMode sharing_mode { VK_SHARING_MODE_EXCLUSIVE };
        std::vector<alib6::u32> queue_family_indices;
        VkImageLayout initial_layout { VK_IMAGE_LAYOUT_UNDEFINED };

        VkMemoryPropertyFlags memory_properties {
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        };
        const void* memory_allocate_next { nullptr };

        bool create_view { true };
        const void* image_view_next { nullptr };
        VkImageViewCreateFlags image_view_flags { 0 };
        VkImageViewType image_view_type { VK_IMAGE_VIEW_TYPE_2D };
        /// VK_FORMAT_UNDEFINED 表示沿用 format。
        VkFormat image_view_format { VK_FORMAT_UNDEFINED };
        VkComponentMapping image_view_components {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        };
        /// aspectMask 为 0 时由 format 自动推断 color/depth/stencil。
        VkImageSubresourceRange image_view_subresource_range { 0, 0, 1, 0, 1 };

        mutable alib6::ErrorWrapper ew {};
    };

    /// 为 swapchain 拥有的 VkImage 创建由 AVE 管理的 ImageView 包装。
    struct AVE_API CreateSwapchainImageInfo {
        std::shared_ptr<Swapchain> swapchain;
        VkImage image { VK_NULL_HANDLE };
        const void* image_view_next { nullptr };
        VkImageViewCreateFlags image_view_flags { 0 };
        VkImageViewType image_view_type { VK_IMAGE_VIEW_TYPE_2D };
        VkComponentMapping image_view_components {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        };
        VkImageSubresourceRange image_view_subresource_range {
            VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
        };
        mutable alib6::ErrorWrapper ew {};
    };

    class AVE_API Image final {
    private:
        std::shared_ptr<Device> device;
        std::shared_ptr<Swapchain> swapchain;
        VkImage image { VK_NULL_HANDLE };
        VkDeviceMemory memory { VK_NULL_HANDLE };
        VkImageView image_view { VK_NULL_HANDLE };
        VkFormat format { VK_FORMAT_UNDEFINED };
        VkExtent3D extent {};
        VkImageUsageFlags usage { 0 };
        VkImageAspectFlags aspect_mask { 0 };

        Image() = default;
        [[nodiscard]] bool initialize(CreateImageInfo ci);
        [[nodiscard]] bool initialize_swapchain_image(
            CreateSwapchainImageInfo ci
        );

    public:
        ~Image();
        Image(const Image&) = delete;
        Image& operator=(const Image&) = delete;
        Image(Image&&) = delete;
        Image& operator=(Image&&) = delete;

        [[nodiscard]] static std::shared_ptr<Image> create(CreateImageInfo ci);
        [[nodiscard]] static std::shared_ptr<Image> create_swapchain_image(
            CreateSwapchainImageInfo ci
        );
        void destroy() noexcept;

        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept;
        [[nodiscard]] const std::shared_ptr<Swapchain>&
        get_swapchain() const noexcept;
        [[nodiscard]] VkImage get_system_handle() const noexcept;
        [[nodiscard]] VkDeviceMemory get_memory() const noexcept;
        [[nodiscard]] VkImageView get_image_view() const noexcept;
        [[nodiscard]] VkFormat get_format() const noexcept;
        [[nodiscard]] VkExtent3D get_extent() const noexcept;
        [[nodiscard]] VkImageUsageFlags get_usage() const noexcept;
        [[nodiscard]] VkImageAspectFlags get_aspect_mask() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;
    };

    class AVE_API WithImagesInput {
    private:
        std::shared_ptr<Device> device;
        std::shared_ptr<Swapchain> swapchain;

    public:
        WithImagesInput(
            std::shared_ptr<Device> target_device,
            std::shared_ptr<Swapchain> target_swapchain
        );
        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept;
        [[nodiscard]] const std::shared_ptr<Swapchain>& get_swapchain() const noexcept;
        [[nodiscard]] VkExtent2D get_extent() const noexcept;
        [[nodiscard]] alib6::u32 get_swapchain_image_count() const noexcept;
        [[nodiscard]] VkSurfaceFormatKHR get_surface_format() const noexcept;
    };

    struct AVE_API CreateImagesInfo {
        std::shared_ptr<Device> device;
        const void* swapchain_image_view_next { nullptr };
        VkImageViewCreateFlags swapchain_image_view_flags { 0 };
        VkImageViewType swapchain_image_view_type { VK_IMAGE_VIEW_TYPE_2D };
        VkComponentMapping swapchain_image_view_components {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        };
        VkImageSubresourceRange swapchain_image_view_subresource_range {
            VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
        };
        /// swapchain images 之外，由 AVE 分配显存并拥有的图像。
        std::vector<CreateImageInfo> images;
        mutable alib6::ErrorWrapper ew {};
    };

    /// 默认为每个 swapchain image 创建同尺寸的 depth/stencil attachment。
    /// 配置回调可先调用本函数，再替换或追加任意图像请求。
    AVE_API void default_configure_images(
        WithImagesInput& input,
        CreateImagesInfo& ci
    );
}
