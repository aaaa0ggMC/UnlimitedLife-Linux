module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

module ave.render;

import std;
import alib6;
import ave.ecode;
import :sync_objects;
import :legacy_render;

namespace ave {

WithSyncObjectsInput::WithSyncObjectsInput(
    std::shared_ptr<Device> target_device,
    std::shared_ptr<Swapchain> target_swapchain
)
:device(std::move(target_device))
,swapchain(std::move(target_swapchain)){}

const std::shared_ptr<Device>& WithSyncObjectsInput::get_device() const noexcept {
    return device;
}

const std::shared_ptr<Swapchain>&
WithSyncObjectsInput::get_swapchain() const noexcept {
    return swapchain;
}

alib6::u32 WithSyncObjectsInput::get_swapchain_image_count() const noexcept {
    return swapchain
        ? swapchain->get_image_count()
        : 0;
}

alib6::u32 default_select_sync_objects_count(WithSyncObjectsInput& input) {
    return input.get_swapchain_image_count();
}

bool SyncObjects::initialize(CreateSyncObjectsInfo ci) {
    if(!ci.device || ci.device->get_system_handle() == VK_NULL_HANDLE ||
       ci.count == 0) {
        ci.ew.report(ave_vk_create_sync_objects,
            "Cannot create synchronization objects without a valid Device and non-zero count.");
        return false;
    }

    device = ci.device;
    frames.reserve(ci.count);
    VkSemaphoreCreateInfo image_available_info {};
    image_available_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    image_available_info.flags = ci.image_available_flags;
    VkSemaphoreCreateInfo render_finished_info {};
    render_finished_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    render_finished_info.flags = ci.render_finished_flags;
    VkFenceCreateInfo fence_info {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = ci.fence_flags;

    for(alib6::u32 i = 0; i < ci.count; ++i) {
        FrameSyncObjects frame;
        const auto handle = device->get_system_handle();
        const auto allocator = device->get_instance()->get_vk_allocator();
        VkResult code = vkCreateSemaphore(
            handle, &image_available_info, allocator, &frame.image_available);
        if(code == VK_SUCCESS) {
            code = vkCreateSemaphore(
                handle, &render_finished_info, allocator, &frame.render_finished);
        }
        if(code == VK_SUCCESS) {
            code = vkCreateFence(handle, &fence_info, allocator, &frame.in_flight);
        }
        if(code != VK_SUCCESS) {
            if(frame.in_flight != VK_NULL_HANDLE) {
                vkDestroyFence(handle, frame.in_flight, allocator);
            }
            if(frame.render_finished != VK_NULL_HANDLE) {
                vkDestroySemaphore(handle, frame.render_finished, allocator);
            }
            if(frame.image_available != VK_NULL_HANDLE) {
                vkDestroySemaphore(handle, frame.image_available, allocator);
            }
            ci.ew.report(ave_vk_create_sync_objects,
                "Failed to create synchronization object set {} ({}).",
                i, static_cast<int>(code));
            destroy();
            return false;
        }
        frames.push_back(frame);
    }
    return true;
}

SyncObjects::~SyncObjects() { destroy(); }

std::shared_ptr<SyncObjects> SyncObjects::create(CreateSyncObjectsInfo ci) {
    auto result = std::shared_ptr<SyncObjects>(new SyncObjects());
    if(!result->initialize(std::move(ci))) return {};
    return result;
}

void SyncObjects::destroy() noexcept {
    if(device && device->get_system_handle() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device->get_system_handle());
        const auto handle = device->get_system_handle();
        const auto allocator = device->get_instance()->get_vk_allocator();
        for(const auto& frame : frames) {
            if(frame.in_flight != VK_NULL_HANDLE) {
                vkDestroyFence(handle, frame.in_flight, allocator);
            }
            if(frame.render_finished != VK_NULL_HANDLE) {
                vkDestroySemaphore(handle, frame.render_finished, allocator);
            }
            if(frame.image_available != VK_NULL_HANDLE) {
                vkDestroySemaphore(handle, frame.image_available, allocator);
            }
        }
    }
    frames.clear();
    device.reset();
}

const std::shared_ptr<Device>& SyncObjects::get_device() const noexcept {
    return device;
}

const std::vector<FrameSyncObjects>& SyncObjects::get_frames() const noexcept {
    return frames;
}

alib6::u32 SyncObjects::size() const noexcept {
    return static_cast<alib6::u32>(frames.size());
}

SyncObjects::operator bool() const noexcept {
    return device && !frames.empty();
}

WithLegacyRenderInput::WithLegacyRenderInput(
    std::shared_ptr<Device> target_device,
    std::shared_ptr<Swapchain> target_swapchain,
    std::vector<std::shared_ptr<Image>> target_images
)
:device(std::move(target_device))
,swapchain(std::move(target_swapchain))
,images(std::move(target_images)){
    const auto handles = swapchain ? swapchain->enumerate_images() : std::nullopt;
    if(handles) {
        swapchain_image_views.reserve(handles->size());
        for(const auto handle : *handles) {
            const auto found = std::ranges::find_if(images, [&](const auto& image) {
                return image && image->get_swapchain() == swapchain &&
                    image->get_system_handle() == handle &&
                    image->get_image_view() != VK_NULL_HANDLE;
            });
            if(found == images.end()) {
                swapchain_image_views.clear();
                break;
            }
            swapchain_image_views.push_back((*found)->get_image_view());
        }
    }
}

const std::shared_ptr<Device>& WithLegacyRenderInput::get_device() const noexcept {
    return device;
}

const std::shared_ptr<Swapchain>&
WithLegacyRenderInput::get_swapchain() const noexcept {
    return swapchain;
}

VkSurfaceFormatKHR WithLegacyRenderInput::get_surface_format() const noexcept {
    return swapchain ? swapchain->get_surface_format() : VkSurfaceFormatKHR {};
}

VkExtent2D WithLegacyRenderInput::get_extent() const noexcept {
    return swapchain ? swapchain->get_extent() : VkExtent2D {};
}

const std::vector<VkImageView>&
WithLegacyRenderInput::get_image_views() const noexcept {
    return swapchain_image_views;
}

const std::vector<std::shared_ptr<Image>>&
WithLegacyRenderInput::get_images() const noexcept {
    return images;
}

void default_configure_legacy_render(
    WithLegacyRenderInput& input,
    CreateLegacyRenderInfo& ci
) {
    ci.swapchain = input.get_swapchain();
    ci.image_dependencies = input.get_images();
    ci.render_pass_flags = 0;
    ci.attachments = {{
        .flags = 0,
        .format = input.get_surface_format().format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    }};

    LegacySubpassInfo subpass;
    subpass.bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.color_attachments.push_back({
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    });

    const auto target_extent = input.get_extent();
    std::vector<std::shared_ptr<Image>> depth_images;
    for(const auto& image : input.get_images()) {
        if(!image || !*image || image->get_image_view() == VK_NULL_HANDLE ||
           (image->get_usage() & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
            continue;
        const auto image_extent = image->get_extent();
        if(image_extent.width == target_extent.width &&
           image_extent.height == target_extent.height) {
            depth_images.push_back(image);
        }
    }
    const bool use_depth =
        depth_images.size() == input.get_image_views().size() &&
        !depth_images.empty() &&
        std::ranges::all_of(depth_images, [&](const auto& image) {
            return image->get_format() == depth_images.front()->get_format();
        });
    if(use_depth) {
        const auto aspect = depth_images.front()->get_aspect_mask();
        const bool has_stencil = (aspect & VK_IMAGE_ASPECT_STENCIL_BIT) != 0;
        ci.attachments.push_back({
            .flags = 0,
            .format = depth_images.front()->get_format(),
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = has_stencil
                ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        });
        subpass.depth_stencil_attachment = VkAttachmentReference {
            .attachment = 1,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };
    }
    ci.subpasses.clear();
    ci.subpasses.push_back(std::move(subpass));
    VkPipelineStageFlags attachment_stages =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkAccessFlags attachment_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    if(use_depth) {
        attachment_stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        attachment_access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    ci.dependencies = {{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = attachment_stages,
        .dstStageMask = attachment_stages,
        .srcAccessMask = 0,
        .dstAccessMask = attachment_access,
        .dependencyFlags = 0
    }};

    VkClearValue color_clear {};
    color_clear.color = {{ 0.02f, 0.02f, 0.03f, 1.0f }};
    ci.default_clear_values = { color_clear };
    if(use_depth) {
        VkClearValue depth_clear {};
        depth_clear.depthStencil = { 1.0f, 0 };
        ci.default_clear_values.push_back(depth_clear);
    }

    ci.framebuffer_flags = 0;
    ci.framebuffer_attachments.clear();
    ci.framebuffer_attachments.reserve(input.get_image_views().size());
    for(std::size_t i = 0; i < input.get_image_views().size(); ++i) {
        ci.framebuffer_attachments.push_back({ input.get_image_views()[i] });
        if(use_depth) {
            ci.framebuffer_attachments.back().push_back(
                depth_images[i]->get_image_view());
        }
    }
    ci.framebuffer_extent = input.get_extent();
    ci.framebuffer_layers = 1;
}

bool LegacyRender::initialize(
    CreateLegacyRenderInfo ci,
    LegacyRenderCreateStatus* status
) {
    if(status) *status = {};
    if(!ci.swapchain || !*ci.swapchain ||
       ci.attachments.empty() || ci.subpasses.empty() ||
       ci.framebuffer_attachments.empty() ||
       ci.framebuffer_attachments.size() != ci.swapchain->get_image_count() ||
       ci.framebuffer_extent.width == 0 || ci.framebuffer_extent.height == 0 ||
       ci.framebuffer_layers == 0) {
        ci.ew.report(ave_vk_create_render_pass,
            "CreateLegacyRenderInfo is incomplete or incompatible with its Swapchain.");
        return false;
    }
    if(!std::ranges::all_of(ci.image_dependencies, [&](const auto& image) {
           return image && *image && image->get_device() == ci.swapchain->get_device();
       })) {
        ci.ew.report(ave_vk_create_render_pass,
            "A LegacyRender Image dependency is invalid or belongs to another Device.");
        return false;
    }

    std::vector<VkSubpassDescription> subpass_infos;
    subpass_infos.reserve(ci.subpasses.size());
    for(const auto& source : ci.subpasses) {
        if(!source.resolve_attachments.empty() &&
           source.resolve_attachments.size() != source.color_attachments.size()) {
            ci.ew.report(ave_vk_create_render_pass,
                "A Legacy subpass must provide one resolve attachment per color attachment.");
            return false;
        }
        VkSubpassDescription value {};
        value.flags = source.flags;
        value.pipelineBindPoint = source.bind_point;
        value.inputAttachmentCount = static_cast<alib6::u32>(
            source.input_attachments.size());
        value.pInputAttachments = source.input_attachments.data();
        value.colorAttachmentCount = static_cast<alib6::u32>(
            source.color_attachments.size());
        value.pColorAttachments = source.color_attachments.data();
        value.pResolveAttachments = source.resolve_attachments.empty()
            ? nullptr : source.resolve_attachments.data();
        value.pDepthStencilAttachment = source.depth_stencil_attachment
            ? std::addressof(*source.depth_stencil_attachment) : nullptr;
        value.preserveAttachmentCount = static_cast<alib6::u32>(
            source.preserve_attachments.size());
        value.pPreserveAttachments = source.preserve_attachments.data();
        subpass_infos.push_back(value);
    }

    VkRenderPassCreateInfo render_pass_info {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.flags = ci.render_pass_flags;
    render_pass_info.attachmentCount = static_cast<alib6::u32>(ci.attachments.size());
    render_pass_info.pAttachments = ci.attachments.data();
    render_pass_info.subpassCount = static_cast<alib6::u32>(subpass_infos.size());
    render_pass_info.pSubpasses = subpass_infos.data();
    render_pass_info.dependencyCount = static_cast<alib6::u32>(ci.dependencies.size());
    render_pass_info.pDependencies = ci.dependencies.data();

    swapchain = ci.swapchain;
    image_dependencies = std::move(ci.image_dependencies);
    const auto device = swapchain->get_device();
    const auto handle = device->get_system_handle();
    const auto allocator = device->get_instance()->get_vk_allocator();
    VkResult code = vkCreateRenderPass(
        handle, &render_pass_info, allocator, &render_pass);
    if(code != VK_SUCCESS) {
        ci.ew.report(ave_vk_create_render_pass,
            "Failed to create Vulkan RenderPass ({}).", static_cast<int>(code));
        swapchain.reset();
        return false;
    }
    if(status) status->render_pass_created = true;
    subpass_color_attachment_counts.reserve(ci.subpasses.size());
    subpass_uses_depth_stencil.reserve(ci.subpasses.size());
    for(const auto& subpass : ci.subpasses) {
        subpass_color_attachment_counts.push_back(
            static_cast<alib6::u32>(subpass.color_attachments.size())
        );
        subpass_uses_depth_stencil.push_back(
            subpass.depth_stencil_attachment.has_value());
    }
    default_clear_values = std::move(ci.default_clear_values);

    framebuffers.reserve(ci.framebuffer_attachments.size());
    for(alib6::u32 i = 0; i < ci.framebuffer_attachments.size(); ++i) {
        const auto& attachments = ci.framebuffer_attachments[i];
        if(attachments.size() != ci.attachments.size()) {
            ci.ew.report(ave_vk_create_framebuffer,
                "Framebuffer {} attachment count does not match the RenderPass.", i);
            destroy();
            return false;
        }
        VkFramebufferCreateInfo framebuffer_info {};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.flags = ci.framebuffer_flags;
        framebuffer_info.renderPass = render_pass;
        framebuffer_info.attachmentCount = static_cast<alib6::u32>(attachments.size());
        framebuffer_info.pAttachments = attachments.data();
        framebuffer_info.width = ci.framebuffer_extent.width;
        framebuffer_info.height = ci.framebuffer_extent.height;
        framebuffer_info.layers = ci.framebuffer_layers;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        code = vkCreateFramebuffer(handle, &framebuffer_info, allocator, &framebuffer);
        if(code != VK_SUCCESS) {
            ci.ew.report(ave_vk_create_framebuffer,
                "Failed to create Vulkan Framebuffer {} ({}).",
                i, static_cast<int>(code));
            destroy();
            return false;
        }
        framebuffers.push_back(framebuffer);
        if(status) ++status->framebuffers_created;
    }
    return true;
}

LegacyRender::~LegacyRender() { destroy(); }

std::shared_ptr<LegacyRender> LegacyRender::create(
    CreateLegacyRenderInfo ci,
    LegacyRenderCreateStatus* status
) {
    auto result = std::shared_ptr<LegacyRender>(new LegacyRender());
    if(!result->initialize(std::move(ci), status)) return {};
    return result;
}

void LegacyRender::destroy() noexcept {
    if(swapchain && swapchain->get_device()) {
        const auto device = swapchain->get_device();
        const auto handle = device->get_system_handle();
        if(handle != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(handle);
            const auto allocator = device->get_instance()->get_vk_allocator();
            for(const auto framebuffer : framebuffers) {
                if(framebuffer != VK_NULL_HANDLE) {
                    vkDestroyFramebuffer(handle, framebuffer, allocator);
                }
            }
            if(render_pass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(handle, render_pass, allocator);
            }
        }
    }
    framebuffers.clear();
    subpass_color_attachment_counts.clear();
    subpass_uses_depth_stencil.clear();
    default_clear_values.clear();
    render_pass = VK_NULL_HANDLE;
    image_dependencies.clear();
    swapchain.reset();
}

const std::shared_ptr<Swapchain>& LegacyRender::get_swapchain() const noexcept {
    return swapchain;
}

const std::shared_ptr<Device>& LegacyRender::get_device() const noexcept {
    static const std::shared_ptr<Device> empty;
    return swapchain ? swapchain->get_device() : empty;
}

const std::vector<std::shared_ptr<Image>>&
LegacyRender::get_image_dependencies() const noexcept {
    return image_dependencies;
}

VkRenderPass LegacyRender::get_render_pass() const noexcept { return render_pass; }

const std::vector<VkFramebuffer>& LegacyRender::get_framebuffers() const noexcept {
    return framebuffers;
}

alib6::u32 LegacyRender::get_subpass_color_attachment_count(
    alib6::u32 subpass
) const noexcept {
    return subpass < subpass_color_attachment_counts.size()
        ? subpass_color_attachment_counts[subpass]
        : 0;
}

bool LegacyRender::subpass_has_depth_stencil(alib6::u32 subpass) const noexcept {
    return subpass < subpass_uses_depth_stencil.size()
        ? subpass_uses_depth_stencil[subpass] : false;
}

const std::vector<VkClearValue>&
LegacyRender::get_default_clear_values() const noexcept {
    return default_clear_values;
}

LegacyRender::operator bool() const noexcept {
    return render_pass != VK_NULL_HANDLE && !framebuffers.empty();
}

}
