// VMA implementation lives in exactly one ordinary translation unit.
// Neither this header nor VMA's types are part of AVE's public module interface.
#define VMA_IMPLEMENTATION
#ifdef __ANDROID__
// Android's API-24 loader exports Vulkan 1.0. Resolve newer/extension entrypoints
// from the actual instance/device instead of importing them at ELF load time.
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#endif
#include <vk_mem_alloc.h>
