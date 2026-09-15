#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

import ave;
import alib6;

static void check(bool value, const char* message) {
    if(!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

// Exercise non-coherent tracking and retry deterministically, even on coherent-only GPUs.
struct TrackingPolicy {
    using CreateInfo = ave::CreateBufferInfo;
    struct State {
        ave::detail::BufferProperties info;
        std::vector<uint8_t> bytes;
        std::vector<std::pair<VkDeviceSize, VkDeviceSize>> flushed;
        bool fail { false };
        int mappings { 0 };
    };
    static inline std::weak_ptr<State> current;
    static std::shared_ptr<State> create(CreateInfo ci) {
        auto s = std::make_shared<State>();
        s->info.device = ci.device;
        s->info.size = s->info.allocation_size = ci.size;
        s->info.memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        s->bytes.resize(ci.size);
        current = s;
        return s;
    }
    static const ave::detail::BufferProperties& properties(const State& s) { return s.info; }
    static VkResult map(State& s, void** p) { ++s.mappings; *p = s.bytes.data(); return VK_SUCCESS; }
    static void unmap(State& s) noexcept { --s.mappings; }
    static VkResult flush(State& s, VkDeviceSize offset, VkDeviceSize size) {
        if(s.fail) return VK_ERROR_OUT_OF_HOST_MEMORY;
        s.flushed.emplace_back(offset, size);
        return VK_SUCCESS;
    }
    static VkResult invalidate(State&, VkDeviceSize, VkDeviceSize) { return VK_SUCCESS; }
};

static void exercise_dirty_tracking(const std::shared_ptr<ave::Device>& device) {
    const auto atom = device->get_non_coherent_atom_size();
    ave::BasicBuffer<TrackingPolicy> buffer({.device = device, .size = 8 * atom});
    auto state = TrackingPolicy::current.lock();
    alib6::Error error;
    {
        auto mapped = buffer.map({.chunk_multiply = 1, .offset = 3, .size = 5 * atom});
        check(bool(mapped), "tracking mapping");
        mapped[0] = 1;
        mapped[atom] = 2;
        mapped[4 * atom] = 3;
        check(mapped.is_dirty(), "non-coherent dirty marking");
        check(!mapped.invalidate(0, VK_WHOLE_SIZE, error), "invalidate rejects dirty writes");
        state->fail = true;
        check(!mapped.flush(error) && mapped.is_dirty(), "flush failure retains dirty ranges");
        state->fail = false;
        check(mapped.flush(error) && !mapped.is_dirty(), "flush retry clears dirty ranges");
        check(state->flushed.size() == 2, "adjacent dirty chunks merge");
        check(state->flushed[0] == std::pair<VkDeviceSize, VkDeviceSize>{3, 2 * atom},
              "flush offset is allocation-local");
        check(state->flushed[1] == std::pair<VkDeviceSize, VkDeviceSize>{3 + 4 * atom, atom},
              "separated dirty chunk stays separate");
        static_cast<uint8_t*>(mapped.raw_data())[2] = 8;
        state->fail = true;
        mapped.upload_raw(2, 1);
        check(mapped.is_dirty(), "raw flush failure retains dirty tracking");
        state->fail = false;
        check(mapped.flush(error), "raw flush retry");
        mapped[0] = 4; // destructor auto-flush
    }
    check(state->mappings == 0 && state->flushed.size() == 4, "RAII flush and balanced unmap");
    state.reset();
    buffer.destroy();
    check(TrackingPolicy::current.expired(), "tracking allocation released");
}

template<class Policy>
static void exercise(typename Policy::CreateInfo ci) {
    using Buffer = ave::BasicBuffer<Policy>;
    using Slice = ave::BasicBufferSlice<Policy>;
    alib6::Error error;
    ci.ew = error;
    ci.size = 257;
    auto buffer = Buffer::create_shared(ci);
    check(bool(buffer), "create");
    check(buffer->get_size() == 257, "logical size");
    std::array<uint8_t, 7> input {1, 2, 3, 4, 5, 6, 7};
    check(buffer->upload(input, 3, error), "unaligned upload");
    {
        auto a = buffer->map({.offset = 3, .size = 7, .ew = error});
        auto b = buffer->map({.offset = 256, .size = 1, .ew = error});
        check(bool(a) && bool(b), "simultaneous subrange maps");
        check(a.invalidate(0, VK_WHOLE_SIZE, error), "invalidate");
        check(static_cast<const uint8_t*>(a.raw_data())[6] == 7, "readback");
        b[0] = 91;
        check(b.flush(error), "tail flush");
        a = std::move(b);
        check(!b && bool(a), "mapping move assignment");
        check(static_cast<const uint8_t*>(a.raw_data())[0] == 91, "tail readback");
    }
    Slice slice(buffer, 1, 15);
    auto sub = slice.sub_slice(2, 7);
    check(sub.get_offset() == 3 && sub.get_size() == 7, "nested slice offsets");
    check(sub.upload(input, 0, error), "slice upload");
    check(!sub.map({.offset = 6, .size = 2, .ew = error}), "slice bounds");
    check(!buffer->map({.offset = 1, .size = std::numeric_limits<VkDeviceSize>::max() - 1,
                        .ew = error}), "mapping overflow rejection");
    auto mapping = buffer->map({.offset = 3, .size = 7, .ew = error});
    check(bool(mapping), "lifetime mapping");
    buffer->destroy();
    check(!*buffer, "destroy clears handle");
    mapping[1] = 99;
    check(mapping.flush(error), "mapping survives buffer destroy");
    check(buffer->create(ci), "recreate while old mapping survives");
    check(!mapping.write(input, 1, error), "write bounds");
}

// Instantiating these calls verifies both backends integrate with drawing APIs.
static void exercise_regions(const std::shared_ptr<ave::VMAAllocator>& allocator) {
    alib6::Error error;
    const auto atom = allocator->get_device()->get_non_coherent_atom_size();
    ave::VMABuffer arena({.allocator = allocator, .size = 4 * atom,
        .host_access = ave::BufferHostAccess::RandomAccess, .ew = error});
    std::vector<uint8_t> data(atom, 73);
    auto first = arena.alloc(data, {.ew = error});
    auto second = arena.alloc_bytes(atom, {.ew = error});
    check(bool(first) && bool(second), "automatic regions");
    check(first.get_offset() != second.get_offset(), "regions do not overlap");
    auto copy = first;
    auto sub = first.sub_slice(0, 1);
    auto mapping = sub.map({.ew = error});
    check(bool(mapping), "allocated slice mapping");
    check(static_cast<const uint8_t*>(mapping.raw_data())[0] == 73, "alloc uploads data");
    const auto original_offset = first.get_offset();
    first.reset();
    first.reset();
    check(!first && !first.get_buffer() && first.get_system_handle() == VK_NULL_HANDLE &&
          first.get_offset() == 0 && first.get_size() == 0, "reset leaves empty slice and is idempotent");
    check(bool(copy) && bool(sub), "reset preserves other owners");
    copy.reset();
    sub.reset();
    auto moved = std::move(mapping);
    auto remainder = arena.alloc_bytes(2 * atom, {.ew = error});
    check(bool(remainder), "remaining capacity");
    check(!arena.alloc_bytes(1, {.ew = error}), "mapping prevents region reuse");
    moved = {};
    auto reused = arena.alloc_bytes(atom, {.ew = error});
    check(bool(reused) && reused.get_offset() == original_offset, "released region reused");
    second.reset();
    remainder.reset();
    reused.reset();
    check(!arena.alloc_bytes(1, {.alignment = 3, .ew = error}), "invalid region alignment");
    check(!arena.alloc_bytes(0, {.ew = error}), "empty allocation rejected");
    auto aligned = arena.alloc_bytes(atom, {.alignment = 2 * atom, .ew = error});
    check(bool(aligned) && aligned.get_offset() % (2 * atom) == 0, "explicit region alignment");
    aligned = {};
    auto full = arena.alloc_bytes(4 * atom, {.ew = error});
    check(bool(full), "all free regions coalesce");
    arena.destroy();
    check(full.upload(data, 0, error), "allocated slice survives arena destroy");

    ave::VMABuffer gpu({.allocator = allocator, .size = 4 * atom,
        .host_access = ave::BufferHostAccess::DeviceOnly, .ew = error});
    check(!gpu.alloc(data, {.ew = error}), "device-only alloc(data) rejects upload");
    auto gpu_region = gpu.alloc_bytes(4 * atom, {.ew = error});
    check(bool(gpu_region), "failed upload returns region to allocator");
    ave::VMABuffer tiny({.allocator = allocator, .size = 1, .ew = error});
    auto byte = tiny.alloc(uint8_t{9}, {.ew = error});
    check(bool(byte) && byte.get_size() == 1, "partial final atom remains allocatable");
}

template<class Policy>
void compile_draw_calls(ave::GraphicsContext& context, const ave::BasicBuffer<Policy>& buffer,
                        const ave::BasicBufferSlice<Policy>& slice) {
    context.bind_vertex_buffer(buffer);
    context.bind_vertex_buffer(slice);
    context.bind_index_buffer(buffer);
    context.bind_index_buffer(slice);
    context.bind_indice_buffer(buffer);
    context.bind_indice_buffer(slice);
    context.draw_indirect(buffer);
    context.draw_indexed_indirect(buffer);
}
template void compile_draw_calls(ave::GraphicsContext&, const ave::Buffer&, const ave::BufferSlice&);
template void compile_draw_calls(ave::GraphicsContext&, const ave::VMABuffer&, const ave::VMABufferSlice&);

int main() {
    ave::Context context;
    alib6::Error error;
    auto instance = std::make_shared<ave::Instance>();
    check(instance->create({.ctx = context, .ew = error}), "instance");
    uint32_t count = 0;
    if(vkEnumeratePhysicalDevices(instance->get_system_handle(), &count, nullptr) != VK_SUCCESS || !count) {
        std::puts("SKIP: no Vulkan physical device");
        return 77;
    }
    std::vector<VkPhysicalDevice> physical(count);
    check(vkEnumeratePhysicalDevices(instance->get_system_handle(), &count, physical.data()) == VK_SUCCESS,
          "enumerate physical devices");
    ave::CreateDeviceInfo dci {.instance = instance, .physical_device = physical[0], .ew = error};
    dci.request_queue(0);
    auto device = ave::Device::create(dci);
    check(bool(device), "device");
    exercise_dirty_tracking(device);
    exercise<ave::NativeMemoryPolicy>({.device = device});
    auto allocator = ave::VMAAllocator::create_shared({.device = device, .ew = error});
    check(bool(allocator), "allocator");
    exercise_regions(allocator);
    exercise<ave::VmaMemoryPolicy>({.allocator = allocator,
        .host_access = ave::BufferHostAccess::RandomAccess});
    exercise<ave::VmaMemoryPolicy>({.allocator = allocator,
        .host_access = ave::BufferHostAccess::RandomAccess, .persistent_mapping = true});
    auto gpu = ave::VMABuffer::create_shared({.allocator = allocator, .size = 64,
        .host_access = ave::BufferHostAccess::DeviceOnly, .ew = error});
    check(bool(gpu), "device-only allocation");
    check(!gpu->map({.ew = error}), "device-only rejects mapping even on UMA");
    auto held = ave::VMABuffer::create_shared({.allocator = allocator, .size = 64, .ew = error});
    check(bool(held), "allocator lifetime buffer");
    auto mapped = held->map({.ew = error});
    check(bool(mapped), "allocator lifetime map");
    held.reset();
    gpu.reset();
    allocator.reset();
    mapped[0] = 42;
    check(mapped.flush(error), "mapping keeps allocator alive");
    std::puts("PASS: native/VMA buffers, slices, mappings, bounds and lifetimes");
}
