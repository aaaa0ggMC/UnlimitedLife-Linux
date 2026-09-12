/**
 * @file dynamic_render.cppm
 * @brief Dynamic rendering backend (VK_KHR_dynamic_rendering / Vulkan 1.3 core)
 * @version 5.0
 * @date 2026-09-12
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.render:dynamic_render;

import std;
import alib6;
import :device;
import :swapchain;
import :image;
import :legacy_render;
import :pipeline_fwd;

export namespace ave {
    class AVE_API WithDynamicRenderInput {
    private:
        std::shared_ptr<Device> device;
        std::shared_ptr<Swapchain> swapchain;
        std::vector<std::shared_ptr<Image>> images;

    public:
        WithDynamicRenderInput(
            std::shared_ptr<Device> target_device,
            std::shared_ptr<Swapchain> target_swapchain,
            std::vector<std::shared_ptr<Image>> target_images = {}
        );
        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept;
        [[nodiscard]] const std::shared_ptr<Swapchain>& get_swapchain() const noexcept;
        [[nodiscard]] VkSurfaceFormatKHR get_surface_format() const noexcept;
        [[nodiscard]] VkExtent2D get_extent() const noexcept;
        [[nodiscard]] const std::vector<std::shared_ptr<Image>>& get_images() const noexcept;
    };

    struct AVE_API CreateDynamicRenderInfo {
        std::shared_ptr<Swapchain> swapchain;
        std::vector<VkFormat> color_attachment_formats;
        VkFormat depth_attachment_format { VK_FORMAT_UNDEFINED };
        VkFormat stencil_attachment_format { VK_FORMAT_UNDEFINED };
        std::vector<VkClearValue> default_clear_values;
        std::vector<std::shared_ptr<Image>> image_dependencies;
        mutable alib6::ErrorWrapper ew {};
    };

    AVE_API void default_configure_dynamic_render(
        WithDynamicRenderInput& input,
        CreateDynamicRenderInfo& ci
    );

    using ConfigureDynamicRender = std::function<void(
        WithDynamicRenderInput&,
        CreateDynamicRenderInfo&
    )>;

    class AVE_API DynamicRender final
        : public std::enable_shared_from_this<DynamicRender>
    {
    private:
        std::shared_ptr<Swapchain> swapchain;
        std::vector<std::shared_ptr<Image>> image_dependencies;
        std::vector<VkFormat> color_attachment_formats;
        VkFormat depth_attachment_format { VK_FORMAT_UNDEFINED };
        VkFormat stencil_attachment_format { VK_FORMAT_UNDEFINED };
        std::vector<VkClearValue> default_clear_values;

        DynamicRender() = default;
        [[nodiscard]] bool initialize(CreateDynamicRenderInfo ci);

    public:
        ~DynamicRender();
        DynamicRender(const DynamicRender&) = delete;
        DynamicRender& operator=(const DynamicRender&) = delete;
        DynamicRender(DynamicRender&&) = delete;
        DynamicRender& operator=(DynamicRender&&) = delete;

        [[nodiscard]] static std::shared_ptr<DynamicRender> create(
            CreateDynamicRenderInfo ci
        );
        void destroy() noexcept;

        [[nodiscard]] const std::shared_ptr<Swapchain>& get_swapchain() const noexcept;
        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept;
        [[nodiscard]] const std::vector<std::shared_ptr<Image>>&
        get_image_dependencies() const noexcept;
        [[nodiscard]] const std::vector<VkFormat>&
        get_color_attachment_formats() const noexcept;
        [[nodiscard]] VkFormat get_depth_attachment_format() const noexcept;
        [[nodiscard]] VkFormat get_stencil_attachment_format() const noexcept;
        [[nodiscard]] const std::vector<VkClearValue>&
        get_default_clear_values() const noexcept;

        [[nodiscard]] std::shared_ptr<DynamicPipeline> create_graphics_pipeline(
            GraphicsShaderBytecode shaders,
            ConfigureDynamicPipeline configure = {}
        );

        [[nodiscard]] std::shared_ptr<DynamicPipeline> create_graphics_pipeline(
            ShaderBytecode vertex,
            ShaderBytecode fragment,
            ShaderBytecode geometry = {},
            TessellationShaderBytecode tessellation = {},
            ConfigureDynamicPipeline configure = {}
        );

        [[nodiscard]] std::shared_ptr<DynamicPipeline> create_graphics_pipeline(
            GraphicsShaderPaths shaders,
            ConfigureDynamicPipeline configure = {}
        );

        [[nodiscard]] std::shared_ptr<DynamicPipeline> create_graphics_pipeline(
            std::string_view vertex,
            std::string_view fragment,
            std::string_view geometry = {},
            TessellationShaderPaths tessellation = {},
            ConfigureDynamicPipeline configure = {}
        );

        [[nodiscard]] explicit operator bool() const noexcept;
    };
}
