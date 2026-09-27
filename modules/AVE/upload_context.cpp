module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>
#include <alib6/debug.h>

module ave.render;

import std;
import alib6;
import ave.ecode;
import :upload_context;

namespace ave {

UploadTicket::State::~State() {
    if(!device) return;
    const auto d = device->get_system_handle();
    const auto alloc = device->get_instance()->get_vk_allocator();
    if(fence != VK_NULL_HANDLE) {
        // Wait for fence to ensure GPU work is finished before destroying resources
        vkWaitForFences(d, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(d, fence, alloc);
    }
    if(command_buffer != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
        if(pool_mutex) {
            std::lock_guard lock(*pool_mutex);
            vkFreeCommandBuffers(d, pool, 1, &command_buffer);
        } else {
            vkFreeCommandBuffers(d, pool, 1, &command_buffer);
        }
    }
}

bool UploadTicket::wait(uint64_t timeout_ns, alib6::ErrorWrapper ew) const {
    if(!state || state->fence == VK_NULL_HANDLE || !state->device) {
        ew.report(ave_vk_draw_frame, "Cannot wait on an empty or invalid UploadTicket.");
        return false;
    }
    const auto d = state->device->get_system_handle();
    const auto code = vkWaitForFences(d, 1, &state->fence, VK_TRUE, timeout_ns);
    if(code != VK_SUCCESS && code != VK_TIMEOUT) {
        ew.report(ave_vk_draw_frame, "Failed waiting for UploadTicket fence ({}).", int(code));
        return false;
    }
    return code == VK_SUCCESS;
}

bool UploadTicket::is_ready() const noexcept {
    if(!state || state->fence == VK_NULL_HANDLE || !state->device) return false;
    const auto code = vkGetFenceStatus(state->device->get_system_handle(), state->fence);
    return code == VK_SUCCESS;
}

struct UploadContext::Impl {
    std::shared_ptr<Device> device;
    VkQueue queue { VK_NULL_HANDLE };
    alib6::u32 queue_family { 0 };
    VkCommandPool pool { VK_NULL_HANDLE };
    std::shared_ptr<std::mutex> mutex;

    ~Impl() {
        if(device && pool != VK_NULL_HANDLE) {
            if(queue != VK_NULL_HANDLE) {
                vkQueueWaitIdle(queue);
            }
            vkDestroyCommandPool(device->get_system_handle(), pool,
                                 device->get_instance()->get_vk_allocator());
        }
    }
};

UploadContext::UploadContext(CreateUploadContextInfo ci) {    if(!ci.device || !ci.device->get_system_handle()) {
        ci.ew.report(ave_vk_create_command_pool, "UploadContext requires a valid Device.");
        return;
    }
    VkQueue q = ci.queue;
    if(q == VK_NULL_HANDLE) {
        q = ci.device->get_queue(ci.queue_family);
        if(q == VK_NULL_HANDLE) {
            ci.ew.report(ave_vk_create_command_pool,
                         "UploadContext cannot acquire queue for family {}.", ci.queue_family);
            return;
        }
    }

    VkCommandPoolCreateInfo pci {};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = ci.queue_family;

    VkCommandPool pool = VK_NULL_HANDLE;
    const auto code = vkCreateCommandPool(
        ci.device->get_system_handle(), &pci,
        ci.device->get_instance()->get_vk_allocator(), &pool);
    if(code != VK_SUCCESS) {
        ci.ew.report(ave_vk_create_command_pool,
                     "Failed to create CommandPool for UploadContext ({}).", int(code));
        return;
    }

    impl = std::make_shared<Impl>();
    impl->device = ci.device;
    impl->queue = q;
    impl->queue_family = ci.queue_family;
    impl->pool = pool;
    impl->mutex = std::make_shared<std::mutex>();
}

std::shared_ptr<UploadContext> UploadContext::create(CreateUploadContextInfo ci) {
    auto ctx = std::shared_ptr<UploadContext>(new UploadContext(ci));
    if(!*ctx) return nullptr;
    return ctx;
}

const std::shared_ptr<Device>& UploadContext::get_device() const noexcept {
    static const std::shared_ptr<Device> empty;
    return impl ? impl->device : empty;
}

VkQueue UploadContext::get_queue() const noexcept {
    return impl ? impl->queue : VK_NULL_HANDLE;
}

alib6::u32 UploadContext::get_queue_family() const noexcept {
    return impl ? impl->queue_family : 0;
}

UploadTicket UploadContext::submit_copy(
    VkBuffer src, VkDeviceSize src_offset,
    VkBuffer dst, VkDeviceSize dst_offset,
    VkDeviceSize size,
    std::shared_ptr<const void> session_lifetime,
    alib6::ErrorWrapper ew
) {
    if(!*this || size == 0) {
        ew.report(ave_vk_draw_frame, "Invalid UploadContext or zero size in submit_copy.");
        return {};
    }

    const VkBufferCopy region {
        .srcOffset = src_offset,
        .dstOffset = dst_offset,
        .size = size
    };

    return submit_recording(
        [&region, src, dst](VkCommandBuffer cb) {
            vkCmdCopyBuffer(cb, src, dst, 1, &region);
        },
        std::move(session_lifetime),
        ew
    );
}

UploadTicket UploadContext::submit_copy_to_image(UploadToImageInfo ci) {
    if(!*this || ci.src == VK_NULL_HANDLE || ci.dst == VK_NULL_HANDLE ||
       ci.extent.width == 0 || ci.extent.height == 0 || ci.extent.depth == 0) {
        ci.ew.report(ave_vk_draw_frame, "Invalid UploadContext or UploadToImageInfo in submit_copy_to_image.");
        return {};
    }

    const VkBufferImageCopy region {
        .bufferOffset = ci.src_offset,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = ci.subresource,
        .imageOffset = { 0, 0, 0 },
        .imageExtent = ci.extent
    };

    VkImageMemoryBarrier to_transfer {};
    to_transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_transfer.srcAccessMask = 0;
    to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_transfer.oldLayout = ci.initial_layout;
    to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.image = ci.dst;
    // VkImageSubresourceLayers::layerCount 对于复制区域而言是 mip 层数。
    to_transfer.subresourceRange = VkImageSubresourceRange{
        ci.subresource.aspectMask,
        ci.subresource.mipLevel, ci.subresource.layerCount,
        ci.subresource.baseArrayLayer, 1
    };

    VkImageMemoryBarrier to_final {};
    to_final.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_final.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_final.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    to_final.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_final.newLayout = ci.final_layout;
    to_final.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_final.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_final.image = ci.dst;
    to_final.subresourceRange = to_transfer.subresourceRange;

    return submit_recording(
        [&](VkCommandBuffer cb) {
            vkCmdPipelineBarrier(
                cb,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &to_transfer);
            vkCmdCopyBufferToImage(
                cb, ci.src, ci.dst,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
            vkCmdPipelineBarrier(
                cb,
                VK_PIPELINE_STAGE_TRANSFER_BIT, ci.final_stage,
                0, 0, nullptr, 0, nullptr, 1, &to_final);
        },
        std::move(ci.session_lifetime),
        ci.ew
    );
}

UploadTicket UploadContext::submit_recording(
    const std::function<void(VkCommandBuffer)>& record,
    std::shared_ptr<const void> session_lifetime,
    alib6::ErrorWrapper ew
) {
    if(!*this) {
        ew.report(ave_vk_draw_frame, "Invalid UploadContext in submit_recording.");
        return {};
    }

    const auto d = impl->device->get_system_handle();
    const auto alloc = impl->device->get_instance()->get_vk_allocator();

    std::lock_guard lock(*impl->mutex);

    VkCommandBufferAllocateInfo ai {};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = impl->pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    VkCommandBuffer cb = VK_NULL_HANDLE;
    auto code = vkAllocateCommandBuffers(d, &ai, &cb);
    if(code != VK_SUCCESS) {
        ew.report(ave_vk_allocate_command_buffers,
                  "Failed to allocate CommandBuffer for copy ({}).", int(code));
        return {};
    }

    VkCommandBufferBeginInfo bi {};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    code = vkBeginCommandBuffer(cb, &bi);
    if(code != VK_SUCCESS) {
        vkFreeCommandBuffers(d, impl->pool, 1, &cb);
        ew.report(ave_vk_draw_frame, "Failed to begin copy CommandBuffer ({}).", int(code));
        return {};
    }

    record(cb);

    code = vkEndCommandBuffer(cb);
    if(code != VK_SUCCESS) {
        vkFreeCommandBuffers(d, impl->pool, 1, &cb);
        ew.report(ave_vk_draw_frame, "Failed to end copy CommandBuffer ({}).", int(code));
        return {};
    }

    VkFenceCreateInfo fci {};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    code = vkCreateFence(d, &fci, alloc, &fence);
    if(code != VK_SUCCESS) {
        vkFreeCommandBuffers(d, impl->pool, 1, &cb);
        ew.report(ave_vk_draw_frame, "Failed to create Fence for copy ({}).", int(code));
        return {};
    }

    VkSubmitInfo si {};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;

    code = vkQueueSubmit(impl->queue, 1, &si, fence);
    if(code != VK_SUCCESS) {
        vkDestroyFence(d, fence, alloc);
        vkFreeCommandBuffers(d, impl->pool, 1, &cb);
        ew.report(ave_vk_draw_frame, "Failed to submit copy to queue ({}).", int(code));
        return {};
    }

    auto ticket_state = std::make_shared<UploadTicket::State>();
    ticket_state->device = impl->device;
    ticket_state->pool = impl->pool;
    ticket_state->command_buffer = cb;
    ticket_state->fence = fence;
    ticket_state->session_lifetime = std::move(session_lifetime);
    ticket_state->pool_mutex = impl->mutex;

    return UploadTicket(std::move(ticket_state));
}

UploadContext::operator bool() const noexcept {
    return impl && impl->pool != VK_NULL_HANDLE && impl->queue != VK_NULL_HANDLE;
}

} // namespace ave
