/**
 * @file legacy_render.cppm
 * @brief VkRenderPass and Swapchain framebuffer rendering backend
 * @version 5.0
 * @date 2026-09-11
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.render:legacy_render;

import std;
import alib6;
import :device;
import :swapchain;
import :image;
import :pipeline_fwd;

export namespace ave {
    struct AVE_API LegacySubpassInfo {
        VkSubpassDescriptionFlags flags { 0 };
        VkPipelineBindPoint bind_point { VK_PIPELINE_BIND_POINT_GRAPHICS };
        std::vector<VkAttachmentReference> input_attachments;
        std::vector<VkAttachmentReference> color_attachments;
        std::vector<VkAttachmentReference> resolve_attachments;
        std::optional<VkAttachmentReference> depth_stencil_attachment;
        std::vector<alib6::u32> preserve_attachments;
    };

    class AVE_API WithLegacyRenderInput {
    private:
        std::shared_ptr<Device> device;
        std::shared_ptr<Swapchain> swapchain;
        std::vector<std::shared_ptr<Image>> images;
        std::vector<VkImageView> swapchain_image_views;

    public:
        WithLegacyRenderInput(
            std::shared_ptr<Device> target_device,
            std::shared_ptr<Swapchain> target_swapchain,
            std::vector<std::shared_ptr<Image>> target_images = {}
        );
        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept;
        [[nodiscard]] const std::shared_ptr<Swapchain>& get_swapchain() const noexcept;
        [[nodiscard]] VkSurfaceFormatKHR get_surface_format() const noexcept;
        [[nodiscard]] VkExtent2D get_extent() const noexcept;
        [[nodiscard]] const std::vector<VkImageView>& get_image_views() const noexcept;
        [[nodiscard]] const std::vector<std::shared_ptr<Image>>&
        get_images() const noexcept;
    };

    struct AVE_API CreateLegacyRenderInfo {
        std::shared_ptr<Swapchain> swapchain;
        VkRenderPassCreateFlags render_pass_flags { 0 };
        std::vector<VkAttachmentDescription> attachments;
        std::vector<LegacySubpassInfo> subpasses;
        std::vector<VkSubpassDependency> dependencies;
        std::vector<VkClearValue> default_clear_values;

        /// Framebuffer 引用到的自建 Image；LegacyRender 持有它们以保证生命周期。
        std::vector<std::shared_ptr<Image>> image_dependencies;

        VkFramebufferCreateFlags framebuffer_flags { 0 };
        std::vector<std::vector<VkImageView>> framebuffer_attachments;
        VkExtent2D framebuffer_extent {};
        alib6::u32 framebuffer_layers { 1 };
        mutable alib6::ErrorWrapper ew {};
    };

    AVE_API void default_configure_legacy_render(
        WithLegacyRenderInput& input,
        CreateLegacyRenderInfo& ci
    );

    using ConfigureLegacyRender = std::function<void(
        WithLegacyRenderInput&,
        CreateLegacyRenderInfo&
    )>;

    struct AVE_API LegacyRenderCreateStatus {
        bool render_pass_created { false };
        alib6::u32 framebuffers_created { 0 };
    };

    class AVE_API LegacyRender final
        : public std::enable_shared_from_this<LegacyRender>
    {
    private:
        std::shared_ptr<Swapchain> swapchain;
        std::vector<std::shared_ptr<Image>> image_dependencies;
        VkRenderPass render_pass { VK_NULL_HANDLE };
        std::vector<VkFramebuffer> framebuffers;
        std::vector<alib6::u32> subpass_color_attachment_counts;
        std::vector<bool> subpass_uses_depth_stencil;
        std::vector<VkClearValue> default_clear_values;

        LegacyRender() = default;
        [[nodiscard]] bool initialize(
            CreateLegacyRenderInfo ci,
            LegacyRenderCreateStatus* status
        );

    public:
        ~LegacyRender();
        LegacyRender(const LegacyRender&) = delete;
        LegacyRender& operator=(const LegacyRender&) = delete;
        LegacyRender(LegacyRender&&) = delete;
        LegacyRender& operator=(LegacyRender&&) = delete;

        [[nodiscard]] static std::shared_ptr<LegacyRender> create(
            CreateLegacyRenderInfo ci,
            LegacyRenderCreateStatus* status = nullptr
        );
        void destroy() noexcept;

        [[nodiscard]] const std::shared_ptr<Swapchain>& get_swapchain() const noexcept;
        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept;
        [[nodiscard]] const std::vector<std::shared_ptr<Image>>&
        get_image_dependencies() const noexcept;
        [[nodiscard]] VkRenderPass get_render_pass() const noexcept;
        [[nodiscard]] const std::vector<VkFramebuffer>& get_framebuffers() const noexcept;
        [[nodiscard]] alib6::u32 get_subpass_color_attachment_count(
            alib6::u32 subpass
        ) const noexcept;
        [[nodiscard]] bool subpass_has_depth_stencil(
            alib6::u32 subpass
        ) const noexcept;
        [[nodiscard]] const std::vector<VkClearValue>&
        get_default_clear_values() const noexcept;

        [[nodiscard]] std::shared_ptr<LegacyPipeline> create_graphics_pipeline(
            GraphicsShaderBytecode shaders,
            ConfigureLegacyPipeline configure = {}
        );

        [[nodiscard]] std::shared_ptr<LegacyPipeline> create_graphics_pipeline(
            ShaderBytecode vertex,
            ShaderBytecode fragment,
            ShaderBytecode geometry = {},
            TessellationShaderBytecode tessellation = {},
            ConfigureLegacyPipeline configure = {}
        );

        [[nodiscard]] std::shared_ptr<LegacyPipeline> create_graphics_pipeline(
            GraphicsShaderPaths shaders,
            ConfigureLegacyPipeline configure = {}
        );

        [[nodiscard]] std::shared_ptr<LegacyPipeline> create_graphics_pipeline(
            std::string_view vertex,
            std::string_view fragment,
            std::string_view geometry = {},
            TessellationShaderPaths tessellation = {},
            ConfigureLegacyPipeline configure = {}
        );

        [[nodiscard]] explicit operator bool() const noexcept;
    };
}
