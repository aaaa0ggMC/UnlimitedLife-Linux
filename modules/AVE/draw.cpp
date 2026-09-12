module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>
#include <alib6/debug.h>

module ave.render;

import std;
import alib6;
import ave.ecode;
import :render;
import :buffer;
import :buffer_slice;

namespace ave {
GraphicsContext::GraphicsContext(alib6::ErrorWrapper target_ew)
:ew(std::move(target_ew)){}

GraphicsContext::~GraphicsContext() {
    // end() is mandatory. An acquired image's binary semaphore cannot safely
    // be reused when the frame was abandoned, so Renderer remains locked.
}

GraphicsContext::GraphicsContext(GraphicsContext&& other) noexcept
:renderer(std::exchange(other.renderer, nullptr))
,ew(std::move(other.ew))
,result_code(other.result_code)
,acquire_result(other.acquire_result)
,image_index(other.image_index)
,frame_index(other.frame_index)
,frame_count(other.frame_count)
,device_owner(other.device_owner)
,device(other.device)
,graphics_queue(other.graphics_queue)
,present_queue(other.present_queue)
,swapchain(other.swapchain)
,render_pass(other.render_pass)
,framebuffer(other.framebuffer)
,command_buffer(other.command_buffer)
,image_available(other.image_available)
,render_finished(other.render_finished)
,in_flight(other.in_flight)
,extent(other.extent)
,default_clear_values(other.default_clear_values)
,images_in_flight(other.images_in_flight)
,dynamic_rendering(other.dynamic_rendering)
,pfn_cmd_begin_rendering(other.pfn_cmd_begin_rendering)
,pfn_cmd_end_rendering(other.pfn_cmd_end_rendering)
,swapchain_image(other.swapchain_image)
,swapchain_view(other.swapchain_view)
,depth_image(other.depth_image)
,depth_view(other.depth_view)
,depth_format(other.depth_format)
,depth_aspect(other.depth_aspect)
,recording(other.recording)
,finished(other.finished) {
    other.recording = false;
    other.finished = true;
}

bool Renderer::prepare_graphics_cache(alib6::ErrorWrapper& ew) {
    const bool dynamic_rendering = device && device->supports_dynamic_rendering();
    const bool valid_legacy = !dynamic_rendering && legacy_render &&
        legacy_render->get_framebuffers().size() == swapchain->get_image_count();
    const bool valid_dynamic = dynamic_rendering;
    const bool valid =
        device && swapchain && sync_objects &&
        command_pool && command_buffers &&
        command_pool->get_device() == device &&
        command_buffers->get_pool() == command_pool &&
        command_buffers->size() >= sync_objects->size() &&
        !sync_objects->get_frames().empty() &&
        sync_objects->size() >= swapchain->get_image_count() &&
        (valid_dynamic || valid_legacy);
    if(!valid) {
        ew.report(ave_vk_draw_frame,
            "Cannot prepare GraphicsContext cache from incomplete or mismatched Renderer resources.");
        return false;
    }

    graphics_cache.device_owner = device.get();
    graphics_cache.device = device->get_system_handle();
    graphics_cache.graphics_queue = device->get_queue(
        swapchain->get_graphics_queue_family()
    );
    graphics_cache.present_queue = device->get_queue(
        swapchain->get_present_queue_family()
    );
    graphics_cache.swapchain = swapchain->get_system_handle();
    graphics_cache.extent = swapchain->get_extent();
    graphics_cache.image_count = swapchain->get_image_count();
    graphics_cache.dynamic_rendering = dynamic_rendering;
    graphics_cache.pfn_cmd_begin_rendering = dynamic_rendering
        ? device->get_cmd_begin_rendering() : nullptr;
    graphics_cache.pfn_cmd_end_rendering = dynamic_rendering
        ? device->get_cmd_end_rendering() : nullptr;
    graphics_cache.frames = sync_objects->get_frames();
    graphics_cache.command_buffers = command_buffers->get_buffers();

    if(dynamic_rendering) {
        graphics_cache.render_pass = VK_NULL_HANDLE;
        graphics_cache.framebuffers.clear();
        graphics_cache.default_clear_values = default_clear_values;

        graphics_cache.swapchain_images.clear();
        graphics_cache.swapchain_views.clear();
        const auto handles = swapchain->enumerate_images(ew);
        if(!handles || handles->size() != swapchain->get_image_count()) {
            ew.report(ave_vk_draw_frame,
                "Cannot enumerate Swapchain images for dynamic rendering.");
            return false;
        }
        for(const auto handle : *handles) {
            const auto it = std::ranges::find_if(images, [&](const auto& img) {
                return img && img->get_swapchain() == swapchain &&
                    img->get_system_handle() == handle &&
                    img->get_image_view() != VK_NULL_HANDLE;
            });
            if(it == images.end()) {
                ew.report(ave_vk_draw_frame,
                    "A Swapchain image has no matching Image view wrapper.");
                return false;
            }
            graphics_cache.swapchain_images.push_back(handle);
            graphics_cache.swapchain_views.push_back((*it)->get_image_view());
        }

        graphics_cache.depth_image = VK_NULL_HANDLE;
        graphics_cache.depth_view = VK_NULL_HANDLE;
        graphics_cache.depth_format = VK_FORMAT_UNDEFINED;
        graphics_cache.depth_aspect = 0;
        for(const auto& img : images) {
            if(img && (img->get_usage() & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) &&
               img->get_image_view() != VK_NULL_HANDLE) {
                graphics_cache.depth_image = img->get_system_handle();
                graphics_cache.depth_view = img->get_image_view();
                graphics_cache.depth_format = img->get_format();
                graphics_cache.depth_aspect = img->get_aspect_mask();
                break;
            }
        }
    } else {
        graphics_cache.render_pass = legacy_render->get_render_pass();
        graphics_cache.framebuffers = legacy_render->get_framebuffers();
        graphics_cache.default_clear_values =
            legacy_render->get_default_clear_values();
        graphics_cache.swapchain_images.clear();
        graphics_cache.swapchain_views.clear();
        graphics_cache.depth_image = VK_NULL_HANDLE;
        graphics_cache.depth_view = VK_NULL_HANDLE;
        graphics_cache.depth_format = VK_FORMAT_UNDEFINED;
        graphics_cache.depth_aspect = 0;
    }

    graphics_cache.ready = true;
    images_in_flight.assign(graphics_cache.image_count, VK_NULL_HANDLE);
    current_frame = 0;
    return true;
}

bool Renderer::invalidate_graphics_cache() noexcept {
    if(context_acquired) return false;
    graphics_cache = {};
    images_in_flight.clear();
    current_frame = 0;
    return true;
}

GraphicsContext Renderer::acquire_context(alib6::ErrorWrapper ew) {
    auto failure = [&ew](VkResult code) {
        GraphicsContext context(std::move(ew));
        context.result_code = code;
        context.acquire_result = code;
        return context;
    };

    if(context_acquired) {
        ew.report(ave_vk_draw_frame,
            "The previous GraphicsContext must be ended before acquiring another frame.");
        return failure(VK_ERROR_INITIALIZATION_FAILED);
    }

    if(!graphics_cache.ready && !prepare_graphics_cache(ew)) {
        return failure(VK_ERROR_INITIALIZATION_FAILED);
    }

    const auto& cache = graphics_cache;
    const auto& frames = cache.frames;
    current_frame %= frames.size();
    const auto frame = frames[current_frame];
    const VkCommandBuffer command_buffer = cache.command_buffers[current_frame];

    VkResult code = vkWaitForFences(
        cache.device, 1, &frame.in_flight, VK_TRUE, UINT64_MAX
    );
    if(code != VK_SUCCESS) {
        ew.report(ave_vk_draw_frame,
            "Failed waiting for the current frame fence ({}).",
            static_cast<int>(code));
        return failure(code);
    }

    code = vkResetCommandBuffer(command_buffer, 0);
    if(code != VK_SUCCESS) {
        ew.report(ave_vk_draw_frame,
            "Failed to reset the frame CommandBuffer ({}).",
            static_cast<int>(code));
        return failure(code);
    }

    alib6::u32 image_index = 0;
    const VkResult acquire_code = vkAcquireNextImageKHR(
        cache.device,
        cache.swapchain,
        UINT64_MAX,
        frame.image_available,
        VK_NULL_HANDLE,
        &image_index
    );
    if(acquire_code == VK_ERROR_OUT_OF_DATE_KHR) return failure(acquire_code);
    if(acquire_code != VK_SUCCESS && acquire_code != VK_SUBOPTIMAL_KHR) {
        ew.report(ave_vk_draw_frame,
            "Failed to acquire the next Swapchain image ({}).",
            static_cast<int>(acquire_code));
        return failure(acquire_code);
    }

    if(images_in_flight[image_index] != VK_NULL_HANDLE &&
       images_in_flight[image_index] != frame.in_flight) {
        code = vkWaitForFences(
            cache.device, 1, &images_in_flight[image_index], VK_TRUE, UINT64_MAX
        );
        if(code != VK_SUCCESS) {
            ew.report(ave_vk_draw_frame,
                "Failed waiting for the acquired image fence ({}).",
                static_cast<int>(code));
            return failure(code);
        }
    }

    GraphicsContext context(std::move(ew));
    context.renderer = this;
    context.result_code = acquire_code;
    context.acquire_result = acquire_code;
    context.image_index = image_index;
    context.frame_index = current_frame;
    context.frame_count = frames.size();
    context.device_owner = cache.device_owner;
    context.device = cache.device;
    context.graphics_queue = cache.graphics_queue;
    context.present_queue = cache.present_queue;
    context.swapchain = cache.swapchain;
    context.render_pass = cache.render_pass;
    context.command_buffer = command_buffer;
    context.image_available = frame.image_available;
    context.render_finished = cache.frames[image_index].render_finished;
    context.in_flight = frame.in_flight;
    context.extent = cache.extent;
    context.default_clear_values = cache.default_clear_values;
    context.images_in_flight = images_in_flight.data();
    context.dynamic_rendering = cache.dynamic_rendering;
    if(cache.dynamic_rendering) {
        context.pfn_cmd_begin_rendering = cache.pfn_cmd_begin_rendering;
        context.pfn_cmd_end_rendering = cache.pfn_cmd_end_rendering;
        context.swapchain_image = cache.swapchain_images[image_index];
        context.swapchain_view = cache.swapchain_views[image_index];
        context.depth_image = cache.depth_image;
        context.depth_view = cache.depth_view;
        context.depth_format = cache.depth_format;
        context.depth_aspect = cache.depth_aspect;
    } else {
        context.framebuffer = cache.framebuffers[image_index];
    }
    context_acquired = true;
    return context;
}

void GraphicsContext::begin() {
    begin(default_clear_values);
}

void GraphicsContext::begin(VkClearColorValue clear_color) {
    std::array<VkClearValue, 2> stack_clears {};
    alib6::u32 count = 1;
    stack_clears[0].color = clear_color;
    if(default_clear_values.size() > 1) {
        stack_clears[1] = default_clear_values[1];
        count = 2;
    }
    begin(std::span<const VkClearValue>(stack_clears.data(), count));
}

void GraphicsContext::begin(std::span<const VkClearValue> clear_values) {
    if(!renderer) return;
    if(recording || finished) {
        result_code = VK_ERROR_INITIALIZATION_FAILED;
        ew.report(ave_vk_draw_frame,
            "GraphicsContext::begin called in an invalid state.");
        return;
    }

    VkCommandBufferBeginInfo begin_info {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result_code = vkBeginCommandBuffer(command_buffer, &begin_info);
    if(result_code != VK_SUCCESS) {
        ew.report(ave_vk_draw_frame,
            "Failed to begin the frame CommandBuffer ({}).",
            static_cast<int>(result_code));
        return;
    }

    if(dynamic_rendering) {
        VkImageMemoryBarrier color_barrier {};
        color_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        color_barrier.srcAccessMask = 0;
        color_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        color_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        color_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        color_barrier.image = swapchain_image;
        color_barrier.subresourceRange = {
            VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
        };

        VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dst_stages =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        alib6::u32 barrier_count = 1;
        std::array<VkImageMemoryBarrier, 2> barriers { color_barrier };

        if(depth_image != VK_NULL_HANDLE) {
            VkImageMemoryBarrier depth_barrier {};
            depth_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            depth_barrier.srcAccessMask = 0;
            depth_barrier.dstAccessMask =
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            depth_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depth_barrier.newLayout =
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depth_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depth_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depth_barrier.image = depth_image;
            depth_barrier.subresourceRange = { depth_aspect, 0, 1, 0, 1 };
            barriers[1] = depth_barrier;
            barrier_count = 2;
            dst_stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }

        vkCmdPipelineBarrier(
            command_buffer,
            src_stages,
            dst_stages,
            0,
            0, nullptr,
            0, nullptr,
            barrier_count, barriers.data()
        );

        VkRenderingAttachmentInfo color_attachment {};
        color_attachment.sType =
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_attachment.imageView = swapchain_view;
        color_attachment.imageLayout =
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.clearValue = !clear_values.empty()
            ? clear_values[0]
            : VkClearValue { .color = {{ 0.02f, 0.02f, 0.03f, 1.0f }} };

        VkRenderingAttachmentInfo depth_attachment {};
        if(depth_view != VK_NULL_HANDLE) {
            depth_attachment.sType =
                VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depth_attachment.imageView = depth_view;
            depth_attachment.imageLayout =
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            if(clear_values.size() > 1) {
                depth_attachment.clearValue = clear_values[1];
            } else {
                depth_attachment.clearValue.depthStencil = { 1.0f, 0 };
            }
        }

        VkRenderingInfo rendering_info {};
        rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering_info.renderArea.offset = { 0, 0 };
        rendering_info.renderArea.extent = extent;
        rendering_info.layerCount = 1;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments = &color_attachment;
        if(depth_view != VK_NULL_HANDLE) {
            rendering_info.pDepthAttachment = &depth_attachment;
            if(depth_aspect & VK_IMAGE_ASPECT_STENCIL_BIT) {
                rendering_info.pStencilAttachment = &depth_attachment;
            }
        }
        pfn_cmd_begin_rendering(command_buffer, &rendering_info);
    } else {
        VkRenderPassBeginInfo render_pass_info {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = render_pass;
        render_pass_info.framebuffer = framebuffer;
        render_pass_info.renderArea.offset = { 0, 0 };
        render_pass_info.renderArea.extent = extent;
        render_pass_info.clearValueCount = static_cast<alib6::u32>(
            clear_values.size()
        );
        render_pass_info.pClearValues = clear_values.data();
        vkCmdBeginRenderPass(
            command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE
        );
    }

    VkViewport viewport {};
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    VkRect2D scissor {{ 0, 0 }, extent};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    recording = true;
}

void GraphicsContext::bind_pipeline(const Pipeline& pipeline) {
    panic_debug(!recording || !pipeline.is_graphics() || pipeline.get_device().get() != device_owner,
        "Cannot bind this Pipeline to the current GraphicsContext.");
    panic_debug(
        dynamic_rendering != pipeline.is_dynamic(),
        "Pipeline mode does not match current GraphicsContext rendering mode."
    );
    pipeline.bind(command_buffer);
}

void GraphicsContext::bind_vertex_buffer(
    const Buffer& buffer,
    VkDeviceSize offset,
    alib6::u32 binding
) noexcept {
    panic_debug(!recording, "Cannot bind vertex buffer outside of an active recording scope.");
    panic_debug(
        (buffer.get_usage() & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) == 0,
        "Cannot bind a Buffer as vertex buffer without VK_BUFFER_USAGE_VERTEX_BUFFER_BIT."
    );
    if(!recording) return;
    VkBuffer handle = buffer.get_system_handle();
    if(handle != VK_NULL_HANDLE) {
        vkCmdBindVertexBuffers(command_buffer, binding, 1, &handle, &offset);
    }
}

void GraphicsContext::bind_vertex_buffer(
    const BufferSlice& slice,
    alib6::u32 binding
) noexcept {
    panic_debug(!recording, "Cannot bind vertex buffer outside of an active recording scope.");
    panic_debug(
        !slice.get_buffer() || (slice.get_buffer()->get_usage() & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) == 0,
        "Cannot bind a BufferSlice as vertex buffer without VK_BUFFER_USAGE_VERTEX_BUFFER_BIT."
    );
    if(!recording) return;
    VkBuffer handle = slice.get_system_handle();
    VkDeviceSize offset = slice.get_offset();
    if(handle != VK_NULL_HANDLE) {
        vkCmdBindVertexBuffers(command_buffer, binding, 1, &handle, &offset);
    }
}

void GraphicsContext::bind_vertex_buffers(
    alib6::u32 first_binding,
    std::span<const VkBuffer> buffers,
    std::span<const VkDeviceSize> offsets
) noexcept {
    panic_debug(!recording, "Cannot bind vertex buffers outside of an active recording scope.");
    if(!recording || buffers.empty() || buffers.size() != offsets.size()) return;
    vkCmdBindVertexBuffers(
        command_buffer,
        first_binding,
        static_cast<uint32_t>(buffers.size()),
        buffers.data(),
        offsets.data()
    );
}

void GraphicsContext::bind_index_buffer(
    const Buffer& buffer,
    VkDeviceSize offset,
    VkIndexType index_type
) noexcept {
    panic_debug(!recording, "Cannot bind index buffer outside of an active recording scope.");
    panic_debug(
        (buffer.get_usage() & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) == 0,
        "Cannot bind a Buffer as index buffer without VK_BUFFER_USAGE_INDEX_BUFFER_BIT."
    );
    if(!recording) return;
    VkBuffer handle = buffer.get_system_handle();
    if(handle != VK_NULL_HANDLE) {
        vkCmdBindIndexBuffer(command_buffer, handle, offset, index_type);
    }
}

void GraphicsContext::bind_index_buffer(
    const BufferSlice& slice,
    VkIndexType index_type
) noexcept {
    panic_debug(!recording, "Cannot bind index buffer outside of an active recording scope.");
    panic_debug(
        !slice.get_buffer() || (slice.get_buffer()->get_usage() & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) == 0,
        "Cannot bind a BufferSlice as index buffer without VK_BUFFER_USAGE_INDEX_BUFFER_BIT."
    );
    if(!recording) return;
    VkBuffer handle = slice.get_system_handle();
    VkDeviceSize offset = slice.get_offset();
    if(handle != VK_NULL_HANDLE) {
        vkCmdBindIndexBuffer(command_buffer, handle, offset, index_type);
    }
}

void GraphicsContext::draw(
    alib6::u32 vertex_count,
    alib6::u32 instance_count,
    alib6::u32 first_vertex,
    alib6::u32 first_instance
) noexcept {
    if(!recording) return;
    vkCmdDraw(
        command_buffer,
        vertex_count,
        instance_count,
        first_vertex,
        first_instance
    );
}

void GraphicsContext::draw_indexed(
    alib6::u32 index_count,
    alib6::u32 instance_count,
    alib6::u32 first_index,
    alib6::i32 vertex_offset,
    alib6::u32 first_instance
) noexcept {
    if(!recording) return;
    vkCmdDrawIndexed(
        command_buffer,
        index_count,
        instance_count,
        first_index,
        vertex_offset,
        first_instance
    );
}

void GraphicsContext::draw_indirect(
    const Buffer& buffer,
    VkDeviceSize offset,
    alib6::u32 draw_count,
    alib6::u32 stride
) noexcept {
    panic_debug(!recording, "Cannot draw indirect outside of an active recording scope.");
    panic_debug(
        (buffer.get_usage() & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) == 0,
        "Cannot use a Buffer for indirect draw without VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT."
    );
    if(!recording) return;
    VkBuffer handle = buffer.get_system_handle();
    if(handle != VK_NULL_HANDLE) {
        vkCmdDrawIndirect(command_buffer, handle, offset, draw_count, stride);
    }
}

void GraphicsContext::draw_indexed_indirect(
    const Buffer& buffer,
    VkDeviceSize offset,
    alib6::u32 draw_count,
    alib6::u32 stride
) noexcept {
    panic_debug(!recording, "Cannot draw indexed indirect outside of an active recording scope.");
    panic_debug(
        (buffer.get_usage() & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) == 0,
        "Cannot use a Buffer for indexed indirect draw without VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT."
    );
    if(!recording) return;
    VkBuffer handle = buffer.get_system_handle();
    if(handle != VK_NULL_HANDLE) {
        vkCmdDrawIndexedIndirect(command_buffer, handle, offset, draw_count, stride);
    }
}

void GraphicsContext::end() {
    if(!renderer) return;
    if(!recording || finished) {
        result_code = VK_ERROR_INITIALIZATION_FAILED;
        ew.report(ave_vk_draw_frame,
            "GraphicsContext::end called before a successful begin().");
        return;
    }

    if(dynamic_rendering) {
        pfn_cmd_end_rendering(command_buffer);

        VkImageMemoryBarrier barrier {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = swapchain_image;
        barrier.subresourceRange = {
            VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
        };

        vkCmdPipelineBarrier(
            command_buffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
    } else {
        vkCmdEndRenderPass(command_buffer);
    }
    recording = false;
    result_code = vkEndCommandBuffer(command_buffer);
    if(result_code != VK_SUCCESS) {
        ew.report(ave_vk_draw_frame,
            "Failed to finish the frame CommandBuffer ({}).",
            static_cast<int>(result_code));
        return;
    }

    result_code = vkResetFences(device, 1, &in_flight);
    if(result_code != VK_SUCCESS) {
        ew.report(ave_vk_draw_frame,
            "Failed to reset the current frame fence ({}).",
            static_cast<int>(result_code));
        return;
    }
    if(images_in_flight) {
        images_in_flight[image_index] = in_flight;
    }

    const VkPipelineStageFlags wait_stage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_available;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_finished;
    result_code = vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight);
    if(result_code != VK_SUCCESS) {
        ew.report(ave_vk_draw_frame,
            "Failed to submit the frame CommandBuffer ({}).",
            static_cast<int>(result_code));
        return;
    }

    VkPresentInfoKHR present_info {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_finished;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain;
    present_info.pImageIndices = &image_index;
    const VkResult present_code = vkQueuePresentKHR(present_queue, &present_info);
    if(present_code != VK_SUCCESS && present_code != VK_SUBOPTIMAL_KHR &&
       present_code != VK_ERROR_OUT_OF_DATE_KHR) {
        result_code = present_code;
        ew.report(ave_vk_draw_frame,
            "Failed to present the rendered Swapchain image ({}).",
            static_cast<int>(present_code));
        return;
    }

    renderer->current_frame = (frame_index + 1) % frame_count;
    renderer->context_acquired = false;
    renderer = nullptr;
    finished = true;
    result_code = present_code != VK_SUCCESS ? present_code : acquire_result;
}

alib6::u32 GraphicsContext::get_image_index() const noexcept { return image_index; }
VkExtent2D GraphicsContext::get_extent() const noexcept { return extent; }
VkCommandBuffer GraphicsContext::get_command_buffer() const noexcept {
    return command_buffer;
}
}
