/**
 * @file image.cppm
 * @brief Vulkan image / image-view RAII wrappers
 * @version 5.0
 * @date 2026-09-27
 *
 * Image 为一等资源：既可直接创建（VMA 或原生显存后端二选一，见 CreateImageInfo），
 * 也支持包装 swapchain 持有的 VkImage；每个 Image 内置一个默认 ImageView 对象，
 * 用户亦可使用 ImageView::create 从 Image 派生任意视图（不同 format / mip / 层范围）。
 * ImageView 派生自 Image：视图可被任何消费 "Image + View" 的下游（framebuffer、
 * dynamic rendering attachments、Renderer::images 等）直接接管。
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.render:image;

import std;
import alib6;
import ave.ecode;
import :device;
import :swapchain;
import :buffer;
import :upload_context;

export namespace ave {

    /// @brief ImageView 创建配置：Image 的内置默认视图与用户自建视图共用。
    /// 所有零值字段（viewType 2D 除外）都会由 Image 属性自动适配。
    struct AVE_API ImageViewConfig {
        const void* next { nullptr };
        VkImageViewCreateFlags flags { 0 };
        /// 与 Image 类型/层数明显不匹配（如 1D/3D/Array）时会自动推断。
        VkImageViewType view_type { VK_IMAGE_VIEW_TYPE_2D };
        /// VK_FORMAT_UNDEFINED 表示沿用 Image 的 format（如 SRGB 变体视图需显式指定）。
        VkFormat format { VK_FORMAT_UNDEFINED };
        VkComponentMapping components {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        };
        /// aspectMask / levelCount / layerCount 为 0 时由 Image 属性补齐。
        VkImageSubresourceRange subresource_range { 0, 0, 1, 0, 1 };
    };

    class Image;
    class ImageView;

    /// @brief 从已有 Image 创建附加 ImageView 的配置。
    /// 注意：ImageView 不延长父 Image 的生命周期（与 Vulkan 中 VkImageView/VkImage
    /// 的生命周期约束一致），调用方须保证父 Image 存活。
    struct AVE_API CreateImageViewInfo {
        std::shared_ptr<Image> image;
        ImageViewConfig view {};
        mutable alib6::ErrorWrapper ew {};
    };

    /// @brief Image 创建配置（统一入口；内存后端按是否提供 allocator 二选一）
    struct AVE_API CreateImageInfo {
        /// VMA 后端：显存与可选的内部 staging 均来自该 allocator（此时 device 可为空，
        /// 自动从 allocator 推导）。
        std::shared_ptr<VMAAllocator> allocator;
        /// 原生后端：device 必填，显存由 vkAllocateMemory 手工分配。
        std::shared_ptr<Device> device;

        const void* next { nullptr };
        VkImageCreateFlags flags { 0 };
        VkImageType image_type { VK_IMAGE_TYPE_2D };
        /// create_from_file 时留 UNDEFINED 表示按文件内容推断。
        VkFormat format { VK_FORMAT_UNDEFINED };
        /// create_from_file 时置 { 0, 0, 0 } 表示按文件内容推断。
        VkExtent3D extent {};
        alib6::u32 mip_levels { 1 };
        alib6::u32 array_layers { 1 };
        VkSampleCountFlagBits samples { VK_SAMPLE_COUNT_1_BIT };
        VkImageTiling tiling { VK_IMAGE_TILING_OPTIMAL };
        /// create_from_file 时留 0 表示自动添加 TRANSFER_DST | SAMPLED。
        VkImageUsageFlags usage { 0 };
        VkSharingMode sharing_mode { VK_SHARING_MODE_EXCLUSIVE };
        std::vector<alib6::u32> queue_family_indices;
        VkImageLayout initial_layout { VK_IMAGE_LAYOUT_UNDEFINED };

        /// 以下字段仅 VMA 后端生效。
        VkMemoryPropertyFlags required_memory_properties { 0 };
        VkMemoryPropertyFlags preferred_memory_properties { 0 };
        bool dedicated_allocation { false };

        /// 以下字段仅原生后端生效。
        VkMemoryPropertyFlags memory_properties {
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        };
        const void* memory_allocate_next { nullptr };

        /// 内置默认 ImageView 配置；默认构造即“自动适配一个合适的视图”，
        /// 置 std::nullopt 则不创建视图。
        std::optional<ImageViewConfig> default_view { ImageViewConfig {} };

        mutable alib6::ErrorWrapper ew {};
    };

    /// @brief 包装 swapchain 拥有的 VkImage（不持有 image/memory，仅持有其视图）。
    struct AVE_API CreateSwapchainImageInfo {
        std::shared_ptr<Swapchain> swapchain;
        VkImage image { VK_NULL_HANDLE };
        /// 默认配置即 swapchain 颜色视图（format/aspect 自动适配）。
        ImageViewConfig view {};
        mutable alib6::ErrorWrapper ew {};
    };

    class AVE_API Image : public std::enable_shared_from_this<Image> {
    protected:
        std::shared_ptr<Device> device;
        std::shared_ptr<Swapchain> swapchain;
        std::shared_ptr<VMAAllocator> allocator;
        /// VMA 后端的 image/显存绑定（状态位于 buffer 子系统，不暴露 VMA 类型）。
        std::shared_ptr<VmaMemoryPolicy::ImageBinding> vma_binding;
        VkImage image { VK_NULL_HANDLE };
        VkDeviceMemory memory { VK_NULL_HANDLE };
        std::shared_ptr<ImageView> default_view;
        VkFormat format { VK_FORMAT_UNDEFINED };
        VkExtent3D extent {};
        alib6::u32 mip_levels { 1 };
        alib6::u32 array_layers { 1 };
        VkImageType image_type { VK_IMAGE_TYPE_2D };
        VkImageCreateFlags image_flags { 0 };
        VkImageUsageFlags usage { 0 };
        VkImageAspectFlags aspect_mask { 0 };
        /// 是否负责销毁 image / memory（原生路径为 true；VMA / swapchain / 视图均为 false）。
        bool owns_image { false };

        Image() = default;
        [[nodiscard]] bool initialize(CreateImageInfo ci);
        [[nodiscard]] bool initialize_swapchain_image(
            CreateSwapchainImageInfo ci
        );
        /// 用与用户一致的路径创建内置默认 ImageView。
        [[nodiscard]] bool initialize_default_view(
            const ImageViewConfig& config,
            alib6::ErrorWrapper& ew
        );
        /// 按本对象属性把 ImageViewConfig 的零值字段补齐为合适的默认视图参数。
        void resolve_self_view_config(ImageViewConfig& cfg) const noexcept;
        /// ImageView 构造时复制父 Image 的公共属性（不转移 image 所有权）。
        void adopt_parent(const Image& parent) noexcept;
        /// 通过 allocator/staging 上传 CPU 侧像素数据（create_from_file 使用）。
        [[nodiscard]] bool upload_pixels(
            const void* data,
            VkDeviceSize bytes,
            AllocateBufferInfo ai
        );

        friend class ImageView;

    public:
        virtual ~Image();
        Image(const Image&) = delete;
        Image& operator=(const Image&) = delete;
        Image(Image&&) = delete;
        Image& operator=(Image&&) = delete;

        [[nodiscard]] static std::shared_ptr<Image> create(CreateImageInfo ci);
        [[nodiscard]] static std::shared_ptr<Image> create_swapchain_image(
            CreateSwapchainImageInfo ci
        );
        /// 从图片文件创建 Image（stb 解码，自动 staging 上传）。
        /// - ci.format / ci.extent / ci.usage 留零值时按文件内容自动推断；
        /// - upload.map_info.staging 为空时内部临时 staging 兜底（需 ci.allocator），
        ///   亦可显式传入用户 buffer（要求 host_access 为 SequentialWrite）。
        /// - upload.map_info.upload_context 必填（提供单次命令与提交队列）。
        [[nodiscard]] static std::shared_ptr<Image> create_from_file(
            CreateImageInfo ci,
            const alib6::io::FileEntry& entry,
            AllocateBufferInfo upload = {}
        );

        virtual void destroy() noexcept;

        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept;
        [[nodiscard]] const std::shared_ptr<Swapchain>& get_swapchain() const noexcept;
        [[nodiscard]] const std::shared_ptr<VMAAllocator>& get_allocator() const noexcept;
        [[nodiscard]] VkImage get_system_handle() const noexcept;
        [[nodiscard]] VkDeviceMemory get_memory() const noexcept;
        [[nodiscard]] VkFormat get_format() const noexcept;
        [[nodiscard]] VkExtent3D get_extent() const noexcept;
        [[nodiscard]] VkImageUsageFlags get_usage() const noexcept;
        [[nodiscard]] VkImageAspectFlags get_aspect_mask() const noexcept;
        [[nodiscard]] alib6::u32 get_mip_levels() const noexcept;
        [[nodiscard]] alib6::u32 get_array_layers() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;

        /// ImageView 句柄（普通 Image 为内置默认视图；ImageView 为自身视图）。
        [[nodiscard]] virtual VkImageView get_image_view() const noexcept;
        /// 内置默认 ImageView 对象（未创建视图时为 nullptr）。
        [[nodiscard]] std::shared_ptr<ImageView> get_default_view() const noexcept;
    };

    /// @brief Image 的附加视图；派生自 Image，可直接当作 Image 使用。
    class AVE_API ImageView : public Image {
    private:
        std::weak_ptr<Image> parent;
        VkImageView view { VK_NULL_HANDLE };

        ImageView() = default;
        [[nodiscard]] bool initialize(CreateImageViewInfo ci);

    public:
        ~ImageView() override;
        [[nodiscard]] static std::shared_ptr<ImageView> create(
            CreateImageViewInfo ci
        );
        void destroy() noexcept override;
        [[nodiscard]] VkImageView get_image_view() const noexcept override;
        [[nodiscard]] const std::weak_ptr<Image>& get_parent() const noexcept;
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
        /// swapchain image 默认视图配置（默认即颜色视图）。
        ImageViewConfig swapchain_image_view {};
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
