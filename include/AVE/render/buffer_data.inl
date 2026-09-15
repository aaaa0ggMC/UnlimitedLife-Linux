// Included inside exported namespace ave by buffer.cppm.
template<class P>
BasicBufferData<P>::BasicBufferData(
    std::shared_ptr<typename P::State> resource, void* ptr,
    VkDeviceSize offset, VkDeviceSize size, VkDeviceSize capacity,
    VkDeviceSize alloc_size, VkDeviceSize c_size, bool coherent
) : state(std::move(resource)), mapped_ptr(ptr), map_offset(offset),
    map_size(size), mapped_capacity(capacity), allocation_size(alloc_size),
    chunk_size(c_size), chunk_count(size / c_size + (size % c_size != 0)),
    is_coherent(coherent), dirty_mask(chunk_count) {
    dirty_mask.ensure(chunk_count);
    dirty_mask.fill(false);
}

template<class P>
void BasicBufferData<P>::release() noexcept {
    if(mapped_ptr && state) {
        if(auto_upload) {
            // Destructors cannot report flush failures; use explicit flush() when needed.
            try {
                alib6::Error ignored;
                (void)flush(ignored);
            } catch(...) {}
        }
        P::unmap(*state);
    }
    mapped_ptr = nullptr;
    state.reset();
    region_lifetime.reset();
}

template<class P>
BasicBufferData<P>::~BasicBufferData() noexcept { release(); }

template<class P>
BasicBufferData<P>::BasicBufferData(BasicBufferData&& other) noexcept
    : state(std::move(other.state)),
      region_lifetime(std::move(other.region_lifetime)),
      mapped_ptr(std::exchange(other.mapped_ptr, nullptr)),
      map_offset(other.map_offset), map_size(other.map_size),
      mapped_capacity(other.mapped_capacity), allocation_size(other.allocation_size),
      chunk_size(other.chunk_size), chunk_count(other.chunk_count),
      is_coherent(other.is_coherent), auto_upload(other.auto_upload),
      dirty_mask(std::move(other.dirty_mask)) {}

template<class P>
BasicBufferData<P>& BasicBufferData<P>::operator=(BasicBufferData&& other) noexcept {
    if(this == &other) return *this;
    release();
    state = std::move(other.state);
    region_lifetime = std::move(other.region_lifetime);
    mapped_ptr = std::exchange(other.mapped_ptr, nullptr);
    map_offset = other.map_offset;
    map_size = other.map_size;
    mapped_capacity = other.mapped_capacity;
    allocation_size = other.allocation_size;
    chunk_size = other.chunk_size;
    chunk_count = other.chunk_count;
    is_coherent = other.is_coherent;
    auto_upload = other.auto_upload;
    dirty_mask = std::move(other.dirty_mask);
    return *this;
}

template<class P>
typename BasicBufferData<P>::ByteProxy&
BasicBufferData<P>::ByteProxy::operator=(uint8_t value) noexcept {
    *ptr = value;
    if(parent) parent->mark_range_dirty(offset, 1);
    return *this;
}

template<class P>
typename BasicBufferData<P>::ByteProxy&
BasicBufferData<P>::ByteProxy::operator=(const ByteProxy& other) noexcept {
    return *this = static_cast<uint8_t>(other);
}

template<class P>
void BasicBufferData<P>::mark_range_dirty(alib6::usize offset, alib6::usize count) noexcept {
    if(!mapped_ptr || is_coherent || count == 0 || offset >= map_size) return;
    const auto end = offset + std::min<VkDeviceSize>(count, map_size - offset);
    const auto last = end / chunk_size + (end % chunk_size != 0);
    for(auto i = offset / chunk_size; i < last; ++i) dirty_mask.set(i);
}

template<class P>
void BasicBufferData<P>::memcpy(alib6::usize offset, const void* src, alib6::usize bytes) {
    const bool invalid = offset > map_size || bytes > map_size - offset;
    panic_debug(invalid, "BufferData::memcpy out of bounds.");
    if(invalid || !mapped_ptr || !src || bytes == 0) return;
    std::memcpy(static_cast<uint8_t*>(mapped_ptr) + offset, src, bytes);
    mark_range_dirty(offset, bytes);
}

template<class P>
typename BasicBufferData<P>::ByteProxy BasicBufferData<P>::operator[](alib6::usize offset) {
    panic_debug(!mapped_ptr || offset >= map_size, "BufferData::operator[] out of range.");
    return ByteProxy(static_cast<uint8_t*>(mapped_ptr) + offset, this, offset);
}

template<class P>
uint8_t BasicBufferData<P>::operator[](alib6::usize offset) const noexcept {
    panic_debug(!mapped_ptr || offset >= map_size, "BufferData::operator[] out of range.");
    return static_cast<const uint8_t*>(mapped_ptr)[offset];
}

template<class P>
bool BasicBufferData<P>::flush(alib6::ErrorWrapper ew) {
    if(!mapped_ptr || !state) {
        ew.report(ave_vk_map_buffer, "Cannot flush an empty mapping.");
        return false;
    }
    if(is_coherent || dirty_mask.none()) return true;
    alib6::usize cursor = 0;
    while(cursor < chunk_count) {
        const auto first = dirty_mask.find_next_1(cursor);
        if(!first || *first >= chunk_count) break;
        const auto last = dirty_mask.find_next_0(*first, chunk_count).value_or(chunk_count);
        const VkDeviceSize begin = *first * chunk_size;
        const VkDeviceSize end = last == chunk_count ? map_size : last * chunk_size;
        const auto code = P::flush(*state, map_offset + begin, end - begin);
        if(code != VK_SUCCESS) {
            ew.report(ave_vk_map_buffer, "Failed to flush Buffer ({}).", int(code));
            return false; // Retain all dirty bits, including already flushed ranges, for retry.
        }
        cursor = last;
    }
    dirty_mask.fill(false);
    return true;
}

template<class P>
void BasicBufferData<P>::upload_raw(VkDeviceSize offset, VkDeviceSize size) {
    if(!mapped_ptr || !state || offset >= map_size) return;
    const auto bytes = size == VK_WHOLE_SIZE ? map_size - offset : size;
    if(bytes == 0 || bytes > map_size - offset) return;
    mark_range_dirty(static_cast<alib6::usize>(offset), static_cast<alib6::usize>(bytes));
    const auto code = P::flush(*state, map_offset + offset, bytes);
    // Preserve dirty tracking on failure; do not clear partially covered chunks.
    if(code == VK_SUCCESS && offset == 0 && bytes == map_size) dirty_mask.fill(false);
}

template<class P>
bool BasicBufferData<P>::invalidate(VkDeviceSize offset, VkDeviceSize size,
                                    alib6::ErrorWrapper ew) {
    if(!mapped_ptr || !state || offset >= map_size) {
        ew.report(ave_vk_map_buffer, "Invalid invalidate mapping or offset.");
        return false;
    }
    const auto bytes = size == VK_WHOLE_SIZE ? map_size - offset : size;
    if(bytes == 0 || bytes > map_size - offset || is_dirty()) {
        ew.report(ave_vk_map_buffer, "Invalid invalidate range or pending unflushed writes.");
        return false;
    }
    const auto code = P::invalidate(*state, map_offset + offset, bytes);
    if(code != VK_SUCCESS) {
        ew.report(ave_vk_map_buffer, "Failed to invalidate Buffer ({}).", int(code));
        return false;
    }
    return true;
}
