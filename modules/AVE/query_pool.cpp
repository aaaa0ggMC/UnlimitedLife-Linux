#include <AVE/config.h>
#include <vulkan/vulkan.h>
#include <AVE/alib.h>
#include <AVE/error_code.h>
#include <AVE/render/query_pool.h>



namespace ave {

bool TimestampQueryPool::initialize(CreateTimestampQueryPoolInfo ci) {
    if(!ci.device || ci.device->get_system_handle() == VK_NULL_HANDLE || ci.count == 0) {
        ci.ew.report(ave_vk_create_query_pool,
            "Cannot create TimestampQueryPool without a valid Device and non-zero count.");
        return false;
    }
    const auto& families = ci.device->get_physical_device_info().queue_family_properties;
    if(ci.queue_family >= families.size()) {
        ci.ew.report(ave_vk_create_query_pool,
            "TimestampQueryPool queue family {} was not reported by the physical device.",
            ci.queue_family);
        return false;
    }
    device = ci.device;
    count = ci.count;
    valid_bits = families[ci.queue_family].timestampValidBits;
    VkQueryPoolCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    info.queryCount = count;
    const VkResult code = vkCreateQueryPool(
        device->get_system_handle(), &info,
        device->get_instance()->get_vk_allocator(), &pool);
    if(code != VK_SUCCESS) {
        ci.ew.report(ave_vk_create_query_pool,
            "Failed to create Vulkan timestamp QueryPool ({}).", static_cast<int>(code));
        device.reset();
        count = 0;
        valid_bits = 0;
        return false;
    }
    return true;
}

TimestampQueryPool::~TimestampQueryPool() { destroy(); }

std::shared_ptr<TimestampQueryPool> TimestampQueryPool::create(CreateTimestampQueryPoolInfo ci) {
    auto result = std::shared_ptr<TimestampQueryPool>(new TimestampQueryPool());
    if(!result->initialize(std::move(ci))) return {};
    return result;
}

void TimestampQueryPool::destroy() noexcept {
    if(pool != VK_NULL_HANDLE && device) {
        vkDeviceWaitIdle(device->get_system_handle());
        vkDestroyQueryPool(device->get_system_handle(), pool,
            device->get_instance()->get_vk_allocator());
    }
    pool = VK_NULL_HANDLE;
    count = 0;
    valid_bits = 0;
    device.reset();
}

VkQueryPool TimestampQueryPool::get_system_handle() const noexcept { return pool; }
ave::u32 TimestampQueryPool::get_count() const noexcept { return count; }
ave::u32 TimestampQueryPool::get_valid_bits() const noexcept { return valid_bits; }
TimestampQueryPool::operator bool() const noexcept { return pool != VK_NULL_HANDLE; }

void TimestampQueryPool::reset(VkCommandBuffer command_buffer, ave::u32 first, ave::u32 reset_count) const noexcept {
    if(pool == VK_NULL_HANDLE || command_buffer == VK_NULL_HANDLE || reset_count == 0) return;
    if(first >= count || reset_count > count - first) return;
    vkCmdResetQueryPool(command_buffer, pool, first, reset_count);
}

void TimestampQueryPool::write(VkCommandBuffer command_buffer, VkPipelineStageFlagBits stage, ave::u32 index) const noexcept {
    if(pool == VK_NULL_HANDLE || command_buffer == VK_NULL_HANDLE || index >= count) return;
    vkCmdWriteTimestamp(command_buffer, stage, pool, index);
}

bool TimestampQueryPool::read(std::span<std::uint64_t> out, ave::u32 first, bool wait) const noexcept {
    if(pool == VK_NULL_HANDLE || device == nullptr || out.empty()) return false;
    if(first >= count || out.size() > count - first) return false;
    VkQueryResultFlags flags = VK_QUERY_RESULT_64_BIT;
    if(wait) flags |= VK_QUERY_RESULT_WAIT_BIT;
    return vkGetQueryPoolResults(
        device->get_system_handle(), pool, first, static_cast<ave::u32>(out.size()),
        out.size_bytes(), out.data(), sizeof(std::uint64_t), flags) == VK_SUCCESS;
}

}
