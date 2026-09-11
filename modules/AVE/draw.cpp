module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

module ave.render;

import std;
import alib6;
import ave.ecode;
import :render;

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
,recording(other.recording)
,finished(other.finished) {
    other.recording = false;
    other.finished = true;
}

bool Renderer::prepare_graphics_cache(alib6::ErrorWrapper& ew) {
    const bool valid =
        device && swapchain && sync_objects && legacy_render &&
        command_pool && command_buffers &&
        command_pool->get_device() == device &&
        command_buffers->get_pool() == command_pool &&
        command_buffers->size() >= sync_objects->size() &&
        !sync_objects->get_frames().empty() &&
        sync_objects->size() >= swapchain->get_images().size() &&
        legacy_render->get_framebuffers().size() == swapchain->get_images().size();
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
    graphics_cache.render_pass = legacy_render->get_render_pass();
    graphics_cache.extent = swapchain->get_extent();
    graphics_cache.image_count = swapchain->get_images().size();
    graphics_cache.frames = sync_objects->get_frames();
    graphics_cache.framebuffers = legacy_render->get_framebuffers();
    graphics_cache.command_buffers = command_buffers->get_buffers();
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
    context.framebuffer = cache.framebuffers[image_index];
    context.command_buffer = command_buffer;
    context.image_available = frame.image_available;
    context.render_finished = cache.frames[image_index].render_finished;
    context.in_flight = frame.in_flight;
    context.extent = cache.extent;
    context_acquired = true;
    return context;
}

void GraphicsContext::begin(VkClearColorValue clear_color) {
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

    VkClearValue clear_value {};
    clear_value.color = clear_color;
    VkRenderPassBeginInfo render_pass_info {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass;
    render_pass_info.framebuffer = framebuffer;
    render_pass_info.renderArea.offset = { 0, 0 };
    render_pass_info.renderArea.extent = extent;
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_value;
    vkCmdBeginRenderPass(
        command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE
    );

    VkViewport viewport {};
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    VkRect2D scissor {{ 0, 0 }, extent};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    recording = true;
}

void GraphicsContext::bind_pipeline(const LegacyPipeline& pipeline) {
    if(!renderer) return;
    if(!recording || pipeline.get_device().get() != device_owner) {
        result_code = VK_ERROR_INITIALIZATION_FAILED;
        ew.report(ave_vk_draw_frame,
            "Cannot bind this LegacyPipeline to the current GraphicsContext.");
        return;
    }
    vkCmdBindPipeline(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline.get_system_handle()
    );
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

void GraphicsContext::end() {
    if(!renderer) return;
    if(!recording || finished) {
        result_code = VK_ERROR_INITIALIZATION_FAILED;
        ew.report(ave_vk_draw_frame,
            "GraphicsContext::end called before a successful begin().");
        return;
    }

    vkCmdEndRenderPass(command_buffer);
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
    renderer->images_in_flight[image_index] = in_flight;

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
