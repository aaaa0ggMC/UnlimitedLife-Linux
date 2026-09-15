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

struct TestUbo {
    uint32_t a { 0 };
    uint32_t b { 0 };
    uint32_t c { 0 };
    uint32_t d { 0 };
};

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

    // Test slice_n on Buffer
    auto slices = buffer->template slice_n<uint32_t>(3, 64, 0);
    check(slices.size() == 3, "buffer slice_n count");
    check(slices.get_stride() == 64, "buffer slice_n stride");
    check(slices.get_element_size() == sizeof(uint32_t), "buffer slice_n element size");
    check(slices[0].get_offset() == 0 && slices[1].get_offset() == 64 && slices[2].get_offset() == 128, "buffer slice_n offsets");
    check(slices.upload_all(uint32_t{42}, error), "buffer slice_n upload_all");
    {
        auto map0 = slices[0].map({.ew = error});
        auto map1 = slices[1].map({.ew = error});
        auto map2 = slices[2].map({.ew = error});
        check(map0.invalidate(0, VK_WHOLE_SIZE, error), "invalidate slice 0");
        check(map1.invalidate(0, VK_WHOLE_SIZE, error), "invalidate slice 1");
        check(map2.invalidate(0, VK_WHOLE_SIZE, error), "invalidate slice 2");
        check(*reinterpret_cast<const uint32_t*>(map0.raw_data()) == 42, "slice 0 readback");
        check(*reinterpret_cast<const uint32_t*>(map1.raw_data()) == 42, "slice 1 readback");
        check(*reinterpret_cast<const uint32_t*>(map2.raw_data()) == 42, "slice 2 readback");
    }
    check(slices[1].upload(uint32_t{999}, VkDeviceSize{0}, error), "slices[1] upload");
    {
        auto map0 = slices[0].map({.ew = error});
        auto map1 = slices[1].map({.ew = error});
        check(map0.invalidate(0, VK_WHOLE_SIZE, error), "invalidate slice 0");
        check(map1.invalidate(0, VK_WHOLE_SIZE, error), "invalidate slice 1");
        check(*reinterpret_cast<const uint32_t*>(map0.raw_data()) == 42, "slice 0 unchanged");
        check(*reinterpret_cast<const uint32_t*>(map1.raw_data()) == 999, "slice 1 updated");
    }
    auto oob_slices = buffer->template slice_n<uint32_t>(10, 64, 0);
    check(!oob_slices && oob_slices.empty(), "buffer slice_n oob rejection");

    // Test slice_n on Slice
    Slice parent_slice(buffer, 0, 192);
    auto sub_slices = parent_slice.template slice_n<uint32_t>(3, 64, 0);
    check(sub_slices.size() == 3, "slice slice_n count");
    check(sub_slices[0].get_offset() == 0 && sub_slices[1].get_offset() == 64 && sub_slices[2].get_offset() == 128, "slice slice_n offsets");

    // Test reflection-based partial updates (upload<&MemberPtr> and upload_range)
    {
        auto ubo_slices = buffer->template slice_n<TestUbo>(3, 64, 0);
        check(ubo_slices.size() == 3, "ubo_slices count");
        check(ubo_slices[0].get_size() == sizeof(TestUbo), "ubo_slices element size");

        TestUbo init_data { 10, 20, 30, 40 };
        check(ubo_slices[0].upload(init_data, VkDeviceSize{0}, error), "upload initial struct");

        // 1. Upload single member by value
        check(ubo_slices[0].template upload<&TestUbo::b>(uint32_t{222}, VkDeviceSize{0}, error), "upload member b");

        // 2. Upload single member by full struct instance
        TestUbo update_c {};
        update_c.c = 333;
        check(ubo_slices[0].template upload<&TestUbo::c>(update_c, VkDeviceSize{0}, error), "upload member c via struct");

        // 3. Upload range of members [b, c]
        TestUbo range_update {};
        range_update.b = 888;
        range_update.c = 999;
        check((ubo_slices[0].template upload_range<&TestUbo::b, &TestUbo::c>(range_update, VkDeviceSize{0}, error)), "upload_range b to c");

        {
            auto map0 = ubo_slices[0].map({.ew = error});
            check(map0.invalidate(0, VK_WHOLE_SIZE, error), "invalidate ubo_slices 0");
            auto* readback = reinterpret_cast<const TestUbo*>(map0.raw_data());
            check(readback->a == 10, "a untouched");
            check(readback->b == 888, "b updated via range");
            check(readback->c == 999, "c updated via range");
            check(readback->d == 40, "d untouched");
        }

        // 4. Test upload_all<&MemberPtr> on BasicBufferSlices
        check(ubo_slices.template upload_all<TestUbo>(TestUbo{ 1, 2, 3, 4 }, VkDeviceSize{0}, error), "upload_all initial TestUbo");
        check(ubo_slices.template upload_all<&TestUbo::b>(uint32_t{777}, VkDeviceSize{0}, error), "upload_all member b");
        for (std::size_t i = 0; i < ubo_slices.size(); ++i) {
            auto m = ubo_slices[i].map({.ew = error});
            check(m.invalidate(0, VK_WHOLE_SIZE, error), "invalidate slice");
            auto* readback = reinterpret_cast<const TestUbo*>(m.raw_data());
            check(readback->a == 1, "slice a untouched");
            check(readback->b == 777, "slice b broadcast updated");
            check(readback->c == 3, "slice c untouched");
            check(readback->d == 4, "slice d untouched");
        }
    }

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

    // Test alloc_n on VMABuffer
    ave::VMABuffer ubo_arena({.allocator = allocator, .size = 8 * atom,
        .host_access = ave::BufferHostAccess::RandomAccess, .ew = error});
    auto ubo_slices = ubo_arena.alloc_n(3, uint32_t{1234}, {.alignment = 2 * atom, .ew = error});
    check(bool(ubo_slices) && ubo_slices.size() == 3, "alloc_n with initial data");
    check(ubo_slices.get_stride() == 2 * atom, "alloc_n stride");
    check(ubo_slices[0].get_offset() % (2 * atom) == 0, "alloc_n alignment");
    check(ubo_slices[1].get_offset() == ubo_slices[0].get_offset() + 2 * atom, "alloc_n offset spacing");
    {
        auto m0 = ubo_slices[0].map({.ew = error});
        auto m2 = ubo_slices[2].map({.ew = error});
        check(*reinterpret_cast<const uint32_t*>(m0.raw_data()) == 1234, "alloc_n m0 readback");
        check(*reinterpret_cast<const uint32_t*>(m2.raw_data()) == 1234, "alloc_n m2 readback");
    }
    // Test shared region lifetime across all slices
    auto s0 = ubo_slices[0];
    auto s1 = ubo_slices[1];
    auto s2 = ubo_slices[2];
    ubo_slices.clear();
    check(!ubo_arena.alloc_bytes(8 * atom, {.ew = error}), "shared region_lifetime prevents reuse");
    s0.reset();
    s1.reset();
    check(!ubo_arena.alloc_bytes(8 * atom, {.ew = error}), "partial release still prevents reuse");
    s2.reset();
    auto full_reuse = ubo_arena.alloc_bytes(8 * atom, {.ew = error});
    check(bool(full_reuse), "all slices released triggers region reuse");
}

static void exercise_staging_upload(const std::shared_ptr<ave::Device>& device,
                                    const std::shared_ptr<ave::VMAAllocator>& allocator) {
    alib6::Error error;
    auto upload_ctx = ave::UploadContext::create({
        .device = device,
        .queue_family = 0,
        .ew = error
    });
    check(bool(upload_ctx), "create UploadContext");

    // 1. DeviceOnly GPU buffer
    ave::VMABuffer gpu({
        .allocator = allocator,
        .size = 256,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .host_access = ave::BufferHostAccess::DeviceOnly,
        .ew = error
    });
    check(bool(gpu), "create DeviceOnly buffer");

    // 2. Staging buffer (SequentialWrite)
    ave::VMABuffer staging({
        .allocator = allocator,
        .size = 256,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .host_access = ave::BufferHostAccess::SequentialWrite,
        .ew = error
    });
    check(bool(staging), "create staging buffer");

    // Pre-validation tests:
    // Missing upload_context with staging must fail
    alib6::Error missing_ctx_err;
    auto bad_map = gpu.map({.staging = &staging, .upload_context = nullptr, .ew = missing_ctx_err});
    check(!bad_map, "staging map without upload_context rejected");
    check(missing_ctx_err.has_error(), "missing upload_context reported");
    check(gpu.get_stagedby() == nullptr, "stagedby not locked on pre-validation failure");

    // Staging buffer too small must fail
    ave::VMABuffer tiny_staging({
        .allocator = allocator,
        .size = 16,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .host_access = ave::BufferHostAccess::SequentialWrite,
        .ew = error
    });
    alib6::Error small_err;
    auto small_map = gpu.map({.offset = 0, .size = 32, .staging = &tiny_staging, .upload_context = upload_ctx.get(), .ew = small_err});
    check(!small_map, "staging capacity too small rejected");
    check(small_err.has_error(), "small staging capacity reported");
    check(gpu.get_stagedby() == nullptr, "stagedby not locked on capacity failure");

    // 3. Staging map, write, submit and wait
    std::vector<uint8_t> test_data = { 10, 20, 30, 40, 50, 60, 70, 80 };
    {
        auto mapped = gpu.map({
            .offset = 16,
            .size = test_data.size(),
            .staging = &staging,
            .upload_context = upload_ctx.get(),
            .ew = error
        });
        check(bool(mapped), "staging map succeeded");
        check(gpu.get_stagedby() != nullptr, "stagedby locked during session");

        // Concurrent map on the same buffer must be rejected!
        alib6::Error conflict_err;
        auto conflict_map = gpu.map({
            .offset = 0,
            .size = 8,
            .staging = &staging,
            .upload_context = upload_ctx.get(),
            .ew = conflict_err
        });
        check(!conflict_map, "concurrent staging map rejected by stagedby");
        check(conflict_err.has_error(), "conflict error reported in ew");

        // Write and submit
        check(mapped.write(test_data), "write to staging mapping");
        auto ticket = mapped.submit(error);
        check(bool(ticket), "submit returned ticket");
        check(ticket.wait(10'000'000'000ULL, error), "ticket wait");
        check(ticket.is_ready(), "ticket is ready after wait");
    }
    // After session and ticket destructed, stagedby must be released
    check(gpu.get_stagedby() == nullptr, "stagedby released after ticket and session finished");

    // 4. Readback verification from DeviceOnly buffer using upload_ctx submit_copy
    ave::VMABuffer readback({
        .allocator = allocator,
        .size = 256,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .host_access = ave::BufferHostAccess::RandomAccess,
        .ew = error
    });
    auto rb_ticket = upload_ctx->submit_copy(
        gpu.get_system_handle(), 16,
        readback.get_system_handle(), 16,
        test_data.size(),
        nullptr,
        error
    );
    check(bool(rb_ticket), "readback submit copy");
    check(rb_ticket.wait(10'000'000'000ULL, error), "readback wait");
    {
        auto rb_map = readback.map({.offset = 16, .size = test_data.size(), .ew = error});
        check(bool(rb_map), "map readback buffer");
        check(rb_map.invalidate(0, VK_WHOLE_SIZE, error), "invalidate readback");
        for(size_t i = 0; i < test_data.size(); ++i) {
            check(rb_map[i] == test_data[i], "readback byte matches uploaded staging data");
        }
    }

    // 5. Slice upload with staging
    auto slice = gpu.alloc_bytes(test_data.size(), {.ew = error});
    check(bool(slice), "alloc_bytes on DeviceOnly buffer");
    std::vector<uint8_t> slice_data = { 99, 88, 77, 66 };
    check(slice.upload(slice_data, 0, {
        .staging = &staging,
        .upload_context = upload_ctx.get(),
        .ew = error
    }), "slice.upload with staging");

    // Readback slice data
    auto rb_slice_ticket = upload_ctx->submit_copy(
        gpu.get_system_handle(), slice.get_offset(),
        readback.get_system_handle(), 0,
        slice_data.size(),
        nullptr,
        error
    );
    check(rb_slice_ticket.wait(10'000'000'000ULL, error), "readback slice wait");
    {
        auto rb_map = readback.map({.offset = 0, .size = slice_data.size(), .ew = error});
        check(rb_map.invalidate(0, VK_WHOLE_SIZE, error), "invalidate readback slice");
        for(size_t i = 0; i < slice_data.size(); ++i) {
            check(rb_map[i] == slice_data[i], "slice readback matches");
        }
    }
}

template<class Policy>
void compile_draw_calls(ave::GraphicsContext& context, const ave::BasicBuffer<Policy>& buffer,
                        const ave::BasicBufferSlice<Policy>& slice) {
    context.bind_vertex_buffer(buffer);
    context.bind_vertex_buffer(slice);
    context.bind_index_buffer_raw(buffer, VK_INDEX_TYPE_UINT32);
    context.bind_index_buffer_raw(slice, VK_INDEX_TYPE_UINT16);
    context.bind_index_buffer<uint32_t>(buffer);
    context.bind_index_buffer<uint16_t>(slice);
    context.bind_indice_buffer_raw(buffer, VK_INDEX_TYPE_UINT32);
    context.bind_indice_buffer_raw(slice, VK_INDEX_TYPE_UINT16);
    context.bind_indice_buffer<uint32_t>(buffer);
    context.bind_indice_buffer<uint16_t>(slice);

    VkDescriptorSet dummy_set = VK_NULL_HANDLE;
    uint32_t offset = 0;
    std::array<uint32_t, 1> offsets = { offset };
    std::array<VkDescriptorSet, 1> sets = { dummy_set };
    context.bind_descriptor_set(0, dummy_set);
    context.bind_descriptor_set(0, dummy_set, offset);
    context.bind_descriptor_set(0, dummy_set, offsets);
    context.bind_descriptor_sets(0, sets);
    context.bind_descriptor_sets(0, sets, offsets);
    context.set_dynamic_buffer_offset(0, offset);
    context.set_dynamic_buffer_offsets(0, offsets);

    context.draw_indirect(buffer);
    context.draw_indexed_indirect(buffer);
}
template void compile_draw_calls(ave::GraphicsContext&, const ave::Buffer&, const ave::BufferSlice&);
template void compile_draw_calls(ave::GraphicsContext&, const ave::VMABuffer&, const ave::VMABufferSlice&);

static void exercise_descriptor_binding_and_sets(const std::shared_ptr<ave::Device>& device) {
    // 1. Check DescriptorBinding fields and factories
    auto ubo_b = ave::DescriptorBinding::ubo(0);
    check(ubo_b.binding == 0, "ubo binding");
    check(ubo_b.descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, "ubo type");
    check(ubo_b.descriptor_count == 1, "ubo count");
    check(ubo_b.stage_flags == VK_SHADER_STAGE_ALL_GRAPHICS, "ubo stages");
    check(ubo_b.immutable_samplers == nullptr, "ubo immutable_samplers");
    check(ubo_b.binding_flags == 0, "ubo binding_flags");

    VkDescriptorSetLayoutBinding vk_b = ubo_b;
    check(vk_b.binding == 0 && vk_b.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, "vk_b conversion");

    auto dyn_ubo_b = ave::DescriptorBinding::dynamic_ubo(1, VK_SHADER_STAGE_VERTEX_BIT, 2, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
    check(dyn_ubo_b.binding == 1, "dynamic_ubo binding");
    check(dyn_ubo_b.descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, "dynamic_ubo type");
    check(dyn_ubo_b.descriptor_count == 2, "dynamic_ubo count");
    check(dyn_ubo_b.stage_flags == VK_SHADER_STAGE_VERTEX_BIT, "dynamic_ubo stages");
    check(dyn_ubo_b.binding_flags == VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, "dynamic_ubo binding_flags");

    auto ssbo_b = ave::DescriptorBinding::ssbo(2);
    check(ssbo_b.descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, "ssbo type");

    auto dyn_ssbo_b = ave::DescriptorBinding::dynamic_ssbo(3);
    check(dyn_ssbo_b.descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, "dynamic_ssbo type");

    // 2. Check DescriptorSetLayoutInfo constructors
    ave::DescriptorSetLayoutInfo set_info {
        {
            ave::DescriptorBinding::ubo(0),
            ave::DescriptorBinding::dynamic_ubo(1)
        },
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT
    };
    check(set_info.bindings.size() == 2, "set_info bindings size");
    check(set_info.flags == VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, "set_info flags");
    check(set_info.p_next == nullptr, "set_info p_next");

    // Custom pNext test
    VkBaseInStructure custom_next {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = nullptr
    };
    set_info.p_next = &custom_next;
    check(set_info.p_next == &custom_next, "set_info custom p_next");

    // 3. Test actual Vulkan descriptor set layout creation via device using DescriptorBinding
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.push_back(ubo_b);
    bindings.push_back(dyn_ubo_b);

    VkDescriptorSetLayoutCreateInfo layout_ci {};
    layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_ci.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_ci.pBindings = bindings.data();

    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(
        device->get_system_handle(),
        &layout_ci,
        device->get_instance()->get_vk_allocator(),
        &set_layout
    );
    check(res == VK_SUCCESS && set_layout != VK_NULL_HANDLE, "vkCreateDescriptorSetLayout via DescriptorBinding");
    vkDestroyDescriptorSetLayout(
        device->get_system_handle(),
        set_layout,
        device->get_instance()->get_vk_allocator()
    );
}

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
    exercise_descriptor_binding_and_sets(device);
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

    exercise_staging_upload(device, ave::VMAAllocator::create_shared({.device = device, .ew = error}));

    std::puts("PASS: native/VMA buffers, slices, mappings, bounds and lifetimes");
}
