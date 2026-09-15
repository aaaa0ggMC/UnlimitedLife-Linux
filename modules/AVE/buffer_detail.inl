// Private implementation helpers. No VMA declarations in the module interface.
namespace ave::detail {
template<class Info>
bool prepare_buffer(const Info& ci, const std::shared_ptr<Device>& device,
                    BufferProperties& properties, VkBufferCreateInfo& vk,
                    std::vector<alib6::u32>& families, bool pad_to_atom) {
    if(!device || device->get_system_handle() == VK_NULL_HANDLE) {
        ci.ew.report(ave_vk_create_buffer, "A valid Device is required.");
        return false;
    }
    const auto atom = device->get_non_coherent_atom_size();
    const auto max_size = std::numeric_limits<VkDeviceSize>::max();
    if(atom == 0 || (ci.atom_multiply && atom > max_size / ci.atom_multiply)) {
        ci.ew.report(ave_vk_create_buffer, "Invalid or overflowing atom size.");
        return false;
    }
    const auto logical = ci.atom_multiply ? atom * ci.atom_multiply : ci.size;
    if(logical == 0 || (pad_to_atom && logical > max_size - (atom - 1))) {
        ci.ew.report(ave_vk_create_buffer, "Invalid or overflowing Buffer size.");
        return false;
    }
    families = ci.queue_family_indices;
    std::ranges::sort(families);
    families.erase(std::unique(families.begin(), families.end()), families.end());
    if(ci.sharing_mode == VK_SHARING_MODE_CONCURRENT && families.size() < 2) {
        ci.ew.report(ave_vk_create_buffer, "Concurrent sharing requires two queue families.");
        return false;
    }
    properties.device = device;
    properties.size = logical;
    properties.usage = ci.usage;
    vk = {};
    vk.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vk.pNext = ci.next;
    vk.flags = ci.flags;
    vk.size = pad_to_atom ? ((logical + atom - 1) / atom) * atom : logical;
    vk.usage = ci.usage;
    vk.sharingMode = ci.sharing_mode;
    if(ci.sharing_mode == VK_SHARING_MODE_CONCURRENT) {
        vk.queueFamilyIndexCount = static_cast<uint32_t>(families.size());
        vk.pQueueFamilyIndices = families.data();
    }
    return true;
}
}
