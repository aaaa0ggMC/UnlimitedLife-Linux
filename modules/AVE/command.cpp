#include <AVE/config.h>
#include <vulkan/vulkan.h>
#include <AVE/alib.h>
#include <AVE/error_code.h>
#include <AVE/render/command.h>



namespace ave {
WithCommandPoolInput::WithCommandPoolInput(
    std::shared_ptr<Device> target_device,
    std::shared_ptr<Swapchain> target_swapchain
)
:device(std::move(target_device))
,swapchain(std::move(target_swapchain)){}
const std::shared_ptr<Device>& WithCommandPoolInput::get_device() const noexcept {
    return device;
}
const std::shared_ptr<Swapchain>& WithCommandPoolInput::get_swapchain() const noexcept {
    return swapchain;
}
ave::u32 WithCommandPoolInput::get_graphics_queue_family() const noexcept {
    return swapchain ? swapchain->get_graphics_queue_family() : 0;
}
void default_configure_command_pool(
    WithCommandPoolInput& input,
    CreateCommandPoolInfo& ci
) {
    ci.device = input.get_device();
    ci.queue_family = input.get_graphics_queue_family();
    ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
}

bool CommandPool::initialize(CreateCommandPoolInfo ci) {
    if(!ci.device || ci.device->get_system_handle() == VK_NULL_HANDLE ||
       !ci.device->has_queue(ci.queue_family)) {
        ci.ew.report(ave_vk_create_command_pool,
            "Cannot create CommandPool for a queue family not created by Device.");
        return false;
    }
    VkCommandPoolCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = ci.flags;
    info.queueFamilyIndex = ci.queue_family;
    device = ci.device;
    queue_family = ci.queue_family;
    const VkResult code = vkCreateCommandPool(
        device->get_system_handle(), &info,
        device->get_instance()->get_vk_allocator(), &pool);
    if(code != VK_SUCCESS) {
        ci.ew.report(ave_vk_create_command_pool,
            "Failed to create Vulkan CommandPool ({}).", static_cast<int>(code));
        device.reset();
        return false;
    }
    return true;
}

CommandPool::~CommandPool() { destroy(); }
std::shared_ptr<CommandPool> CommandPool::create(CreateCommandPoolInfo ci) {
    auto result = std::shared_ptr<CommandPool>(new CommandPool());
    if(!result->initialize(std::move(ci))) return {};
    return result;
}
void CommandPool::destroy() noexcept {
    if(pool != VK_NULL_HANDLE && device) {
        vkDeviceWaitIdle(device->get_system_handle());
        vkDestroyCommandPool(device->get_system_handle(), pool,
            device->get_instance()->get_vk_allocator());
    }
    pool = VK_NULL_HANDLE;
    queue_family = 0;
    device.reset();
}
const std::shared_ptr<Device>& CommandPool::get_device() const noexcept { return device; }
VkCommandPool CommandPool::get_system_handle() const noexcept { return pool; }
ave::u32 CommandPool::get_queue_family() const noexcept { return queue_family; }
CommandPool::operator bool() const noexcept { return pool != VK_NULL_HANDLE; }

WithCommandBuffersInput::WithCommandBuffersInput(
    std::shared_ptr<CommandPool> target_pool,
    std::shared_ptr<SyncObjects> target_sync_objects
)
:pool(std::move(target_pool))
,sync_objects(std::move(target_sync_objects)){}
const std::shared_ptr<CommandPool>& WithCommandBuffersInput::get_pool() const noexcept {
    return pool;
}
const std::shared_ptr<SyncObjects>& WithCommandBuffersInput::get_sync_objects() const noexcept {
    return sync_objects;
}
ave::u32 WithCommandBuffersInput::get_frame_count() const noexcept {
    return sync_objects ? sync_objects->size() : 0;
}
void default_configure_command_buffers(
    WithCommandBuffersInput& input,
    CreateCommandBuffersInfo& ci
) {
    ci.pool = input.get_pool();
    ci.count = input.get_frame_count();
    ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
}

bool CommandBuffers::initialize(CreateCommandBuffersInfo ci) {
    if(!ci.pool || !*ci.pool || ci.count == 0) {
        ci.ew.report(ave_vk_allocate_command_buffers,
            "Cannot allocate CommandBuffers without a valid CommandPool and non-zero count.");
        return false;
    }
    pool = ci.pool;
    buffers.resize(ci.count, VK_NULL_HANDLE);
    VkCommandBufferAllocateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = pool->get_system_handle();
    info.level = ci.level;
    info.commandBufferCount = ci.count;
    const VkResult code = vkAllocateCommandBuffers(
        pool->get_device()->get_system_handle(), &info, buffers.data());
    if(code != VK_SUCCESS) {
        ci.ew.report(ave_vk_allocate_command_buffers,
            "Failed to allocate Vulkan CommandBuffers ({}).", static_cast<int>(code));
        buffers.clear();
        pool.reset();
        return false;
    }
    return true;
}

CommandBuffers::~CommandBuffers() { destroy(); }
std::shared_ptr<CommandBuffers> CommandBuffers::create(CreateCommandBuffersInfo ci) {
    auto result = std::shared_ptr<CommandBuffers>(new CommandBuffers());
    if(!result->initialize(std::move(ci))) return {};
    return result;
}
void CommandBuffers::destroy() noexcept {
    if(pool && *pool && !buffers.empty()) {
        vkDeviceWaitIdle(pool->get_device()->get_system_handle());
        vkFreeCommandBuffers(
            pool->get_device()->get_system_handle(),
            pool->get_system_handle(),
            static_cast<ave::u32>(buffers.size()),
            buffers.data());
    }
    buffers.clear();
    pool.reset();
}
const std::shared_ptr<CommandPool>& CommandBuffers::get_pool() const noexcept { return pool; }
const std::vector<VkCommandBuffer>& CommandBuffers::get_buffers() const noexcept { return buffers; }
ave::u32 CommandBuffers::size() const noexcept {
    return static_cast<ave::u32>(buffers.size());
}
CommandBuffers::operator bool() const noexcept { return pool && !buffers.empty(); }

bool CommandBuffers::reset(ave::u32 index, VkCommandBufferResetFlags flags, ave::ErrorWrapper ew) const {
    if(index >= buffers.size() || buffers[index] == VK_NULL_HANDLE) {
        ew.report(ave_vk_allocate_command_buffers,
            "CommandBuffers::reset index {} is out of range.", index);
        return false;
    }
    const VkResult code = vkResetCommandBuffer(buffers[index], flags);
    if(code != VK_SUCCESS) {
        ew.report(ave_vk_allocate_command_buffers,
            "Failed to reset CommandBuffer {} ({}).", index, static_cast<int>(code));
        return false;
    }
    return true;
}

bool CommandBuffers::begin(ave::u32 index, VkCommandBufferUsageFlags flags, ave::ErrorWrapper ew) const {
    if(index >= buffers.size() || buffers[index] == VK_NULL_HANDLE) {
        ew.report(ave_vk_allocate_command_buffers,
            "CommandBuffers::begin index {} is out of range.", index);
        return false;
    }
    VkCommandBufferBeginInfo info {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags = flags;
    const VkResult code = vkBeginCommandBuffer(buffers[index], &info);
    if(code != VK_SUCCESS) {
        ew.report(ave_vk_allocate_command_buffers,
            "Failed to begin CommandBuffer {} ({}).", index, static_cast<int>(code));
        return false;
    }
    return true;
}

bool CommandBuffers::end(ave::u32 index, ave::ErrorWrapper ew) const {
    if(index >= buffers.size() || buffers[index] == VK_NULL_HANDLE) {
        ew.report(ave_vk_allocate_command_buffers,
            "CommandBuffers::end index {} is out of range.", index);
        return false;
    }
    const VkResult code = vkEndCommandBuffer(buffers[index]);
    if(code != VK_SUCCESS) {
        ew.report(ave_vk_allocate_command_buffers,
            "Failed to end CommandBuffer {} ({}).", index, static_cast<int>(code));
        return false;
    }
    return true;
}

bool CommandBuffers::submit_and_wait(ave::u32 index, VkQueue queue, ave::ErrorWrapper ew) const {
    if(index >= buffers.size() || buffers[index] == VK_NULL_HANDLE) {
        ew.report(ave_vk_allocate_command_buffers,
            "CommandBuffers::submit_and_wait index {} is out of range.", index);
        return false;
    }
    if(queue == VK_NULL_HANDLE) {
        ew.report(ave_vk_allocate_command_buffers,
            "Cannot submit CommandBuffer {} to a null queue.", index);
        return false;
    }
    VkSubmitInfo info {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &buffers[index];
    VkResult code = vkQueueSubmit(queue, 1, &info, VK_NULL_HANDLE);
    if(code != VK_SUCCESS) {
        ew.report(ave_vk_allocate_command_buffers,
            "Failed to submit CommandBuffer {} ({}).", index, static_cast<int>(code));
        return false;
    }
    code = vkQueueWaitIdle(queue);
    if(code != VK_SUCCESS) {
        ew.report(ave_vk_allocate_command_buffers,
            "Failed waiting for CommandBuffer {} ({}).", index, static_cast<int>(code));
        return false;
    }
    return true;
}

void memory_barrier(VkCommandBuffer command_buffer, const MemoryBarrier& barrier) noexcept {
    if(command_buffer == VK_NULL_HANDLE) return;
    VkMemoryBarrier value {};
    value.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    value.srcAccessMask = barrier.src_access;
    value.dstAccessMask = barrier.dst_access;
    vkCmdPipelineBarrier(command_buffer, barrier.src_stage, barrier.dst_stage,
        0, 1, &value, 0, nullptr, 0, nullptr);
}

void image_barriers(VkCommandBuffer command_buffer, std::span<const ImageBarrier> barriers) noexcept {
    if(command_buffer == VK_NULL_HANDLE || barriers.empty()) return;
    std::vector<VkImageMemoryBarrier> values;
    values.reserve(barriers.size());
    VkPipelineStageFlags src_stage = 0;
    VkPipelineStageFlags dst_stage = 0;
    for(const auto& barrier : barriers) {
        if(barrier.image == VK_NULL_HANDLE) continue;
        VkImageMemoryBarrier value {};
        value.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        value.srcAccessMask = barrier.src_access;
        value.dstAccessMask = barrier.dst_access;
        value.oldLayout = barrier.old_layout;
        value.newLayout = barrier.new_layout;
        value.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        value.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        value.image = barrier.image;
        value.subresourceRange = {
            barrier.aspect,
            barrier.base_mip,
            barrier.mip_count,
            barrier.base_layer,
            barrier.layer_count
        };
        values.push_back(value);
        src_stage |= barrier.src_stage;
        dst_stage |= barrier.dst_stage;
    }
    if(values.empty()) return;
    vkCmdPipelineBarrier(command_buffer, src_stage, dst_stage,
        0, 0, nullptr, 0, nullptr,
        static_cast<ave::u32>(values.size()), values.data());
}

void image_barrier(VkCommandBuffer command_buffer, const ImageBarrier& barrier) noexcept {
    image_barriers(command_buffer, std::span<const ImageBarrier>(&barrier, 1));
}
}
