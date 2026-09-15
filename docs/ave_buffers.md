# Buffer 与 VMABuffer

`import ave.render;`（或 `import ave;`）即可使用；调用方不需要包含 VMA 头文件。

```cpp
using Buffer        = BasicBuffer<NativeMemoryPolicy>;
using VMABuffer     = BasicBuffer<VmaMemoryPolicy>;
using BufferData    = BasicBufferData<NativeMemoryPolicy>;
using VMABufferData = BasicBufferData<VmaMemoryPolicy>;
using BufferSlice  = BasicBufferSlice<NativeMemoryPolicy>;
using VMABufferSlice = BasicBufferSlice<VmaMemoryPolicy>;
```

两种后端共享写入、脏区间跟踪、映射视图、切片和绘制接口。Policy 通过不完整的
`State` 隐藏实现，分配、映射和缓存操作在对应 `.cpp` 中完成，无虚函数分发。
`modules/AVE/vma_detail.cpp` 是唯一定义 `VMA_IMPLEMENTATION` 的翻译单元。
VMA 是 AVE 的私有构建依赖，公开参数使用 AVE 枚举与 Vulkan 基础类型。

## 创建和写入

```cpp
auto allocator = ave::VMAAllocator::create_shared({ .device = device });
if (!allocator) return;

auto vertices = ave::VMABuffer::create_shared({
    .allocator = allocator,
    .size = sizeof(vertex_data),
    .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    .host_access = ave::BufferHostAccess::SequentialWrite,
    .memory_preference = ave::BufferMemoryPreference::PreferDevice,
    .persistent_mapping = true
});
if (!vertices || !vertices->upload(vertex_data)) return;

ave::VMABufferSlice slice(vertices, 0, sizeof(vertex_data));
graphics.bind_vertex_buffer(slice);
```

`BufferHostAccess` 的含义：

- `SequentialWrite`：CPU 顺序写入，适合 staging 和上传；不要依赖读取性能。
- `RandomAccess`：CPU 读取或随机读写，适合 readback。
- `DeviceOnly`：不允许 `map()` / `upload()`，即使最终内存在 UMA 上恰好可见。

`BufferMemoryPreference` 是自动选址偏好，不保证具体 heap；需要强制属性时使用
`required_memory_properties`，软偏好使用 `preferred_memory_properties`。
`persistent_mapping` 要求 CPU host access；`dedicated_allocation` 强制独立分配。

Allocator 默认使用 Vulkan 1.0。可通过 `api_version` 指定 1.0–1.3，版本必须同时
被 Instance 与物理设备支持。只有 Device 已启用相应特性时才能设置
`buffer_device_address = true`；它不会替 Device 启用特性。

## 自动分配 Buffer 内的区域

`VMABuffer` 的 `alloc(data)` 用 VMA 虚拟分配器寻找空闲区域，上传数据并返回拥有区域的
`VMABufferSlice`；`alloc_bytes(bytes)` 只预留未初始化区域，也可用于 DeviceOnly Buffer。
整数传给 `alloc(123)` 表示上传整数 123，分配 123 字节应写 `alloc_bytes(123)`。

```cpp
ave::VMABuffer arena({
    .allocator = allocator,
    .size = 1024 * 1024,
    .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
});
alib6::Error error;
ave::VMABufferSlice vertices = arena.alloc(vertex_data, { .ew = error });
if (!vertices) return;
graphics.bind_vertex_buffer(vertices); // 自动使用区域的 VkBuffer 与 offset

auto reserved = arena.alloc_bytes(4096, { .alignment = 256, .ew = error });
```

`data` 支持 trivially-copyable 对象和连续 POD 区间（vector/span/array 等），不做对象序列化。
对齐参数必须是非零的 2 的幂；内部还按 nonCoherentAtomSize 对齐每个区域起点，以免
不同区域共享需要 flush 的 atom。切片的 size 始终是数据原始字节数；对齐可能留下空隙。
UBO/SSBO 等用途的额外 offset 对齐由调用方通过 `alignment` 指定。

区域由返回切片、其副本、`sub_slice()` 和通过切片创建的映射共同持有；最后一个持有者
释放后自动归还。无需手动 free，但必须保留切片直到使用该区域的 GPU 工作完成。
可用 `vertices.reset()` 显式释放当前切片的所有权（等价于 `vertices = {}`），调用后
切片为空、offset/size 为 0，可重复调用；其他副本、子切片和映射仍然有效。
`reset()` 不清零数据、不等待 GPU，也不强制回收仍被其他对象持有的区域。
原始 arena 销毁或 recreate 不会改变已分配切片引用的旧 Buffer。通过 `get_buffer()`
重新手工构造切片不会继承区域所有权，请用复制或 `sub_slice()`。

容量固定，不足或碎片导致无法满足请求时通过 `ew` 报错并返回空切片；不会隐式扩容或
整理已分配数据，也不会提交 GPU 复制。`alloc(data)` 上传失败会自动归还刚分配的区域。
开始使用区域分配后，不要再通过 arena 的裸 map/upload 或手工切片写入未预留的范围，
这些操作不会登记到区域分配器，可能覆盖自动分配的数据。

## 映射、缓存和同步

```cpp
auto mapping = vertices->map({ .offset = 3, .size = 64 });
if (!mapping) return;
if (!mapping.write(payload)) return;
if (!mapping.flush()) return;
```

所有 offset/size 均以字节计。Buffer 的 offset 相对于 Buffer，映射对象的写入及
`upload_raw()` / `invalidate()` offset 相对于当前映射，切片 offset 相对于切片。
`VK_WHOLE_SIZE` 表示当前逻辑视图的剩余长度，不暴露分配尾部作为可写业务范围。
非对齐映射由后端处理；Native 映射完整分配再偏移，VMA 使用 allocation-local 偏移。

`write()`、`memcpy()`、ByteProxy 和可写迭代器跟踪脏 chunk；通过 `raw_data()` 写入后
须调用 `mark_range_dirty()` 或 `upload_raw()`。`flush()` 合并连续脏区间，成功后清位；
失败保留脏标记。`upload()` 是兼容的无返回值 flush 包装，`upload_raw()` 强制刷新指定范围。
需要检查错误时使用 `flush(ErrorWrapper)`。

映射析构默认 best-effort flush 后 unmap；`cancel_auto_upload()` 仅关闭自动 flush。
`Buffer::upload()` / `BufferSlice::upload()` 显式检查 flush 返回值。
GPU 写入后的 CPU 读取应先由调用方完成 GPU 同步，再 `invalidate()`；有未 flush 的
脏标记时 invalidate 会拒绝执行。flush/invalidate 不是执行同步或 pipeline barrier。
同时存在的映射视图不提供 CPU 数据访问锁，重叠范围及 non-coherent atom 的访问需外部同步。

`upload()` 只支持可映射内存，不隐式提交 staging copy。Device-only 上传需由调用方
创建传输源 Buffer 并录制复制命令。

## 所有权与兼容性

Buffer 为 move-only，映射对象持有分配状态，VMA 分配状态持有 allocator，allocator
持有 Device。因此 `buffer.destroy()` 后已有映射仍有效，重新 create 不会改动旧映射。
手工构造的切片持有原 Buffer 包装对象；显式 destroy/recreate 会影响这类切片。
不要在仍有资源时手动调用 `Device::destroy()` 或 `Instance::destroy()`。

两种 Buffer 销毁都不隐式调用 `vkDeviceWaitIdle()`，GPU 资源最后一次引用释放前必须由
调用方等待相关 fence/timeline，或采用延迟销毁。这与旧 Native Buffer 的隐式等待不同。

`get_memory()` 是借用句柄。VMA 可能让多个 Buffer 共用同一个 `VkDeviceMemory`，
`get_memory_offset()` 返回当前分配在其中的偏移；不得通过这个句柄直接 free/map/unmap。
`get_allocation_size()` 是当前分配大小，`get_size()` 是逻辑 Buffer 大小；
`get_memory_properties()` 返回实际选中的内存类型属性。

Native 保留 `CreateBufferInfo` 字段和常用调用形式。Buffer/Data/Slice 改为类型别名，
已有二进制及手写 `class Buffer;` 前置声明需要更新并重新编译。不同 Policy 的
`shared_ptr<BasicBuffer<...>>` 不互相转换；GraphicsContext 的相关调用直接接收两种类型。

旧 `modules/AVE/buffer_slice.cpp` 的逻辑已迁入模板接口，不再需要单独的实现文件。

## 验证

```sh
xmake build ave_buffer_test
xmake run ave_buffer_test
```

测试无需窗口，检查 Native/VMA 写入、非对齐与尾部映射、多映射、移动赋值、嵌套切片、
越界拒绝、持续映射及 Buffer/allocator 生命周期，并实例化两种后端的绘制接口。
另用测试 Policy 确定性验证非一致内存的脏块合并、flush 失败重试和 RAII 自动刷新。
实际后端的缓存一致性路径取决于设备内存类型。需要 Vulkan ICD 和可用设备；无物理设备返回 77。
