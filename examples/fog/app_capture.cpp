/**
 * @file app_capture.cpp
 * @brief 答辩吉祥物截帧：固定机位渲染 dragon，透明背景，交换链图像读回 TGA。
 * @version 5.0
 * @date 2026-10-27
 */
#include "app.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using enum alib5::Severity;

namespace {

struct HostImage {
    VkBuffer buffer { VK_NULL_HANDLE };
    VkDeviceMemory memory { VK_NULL_HANDLE };
    void* mapped { nullptr };
    VkDeviceSize bytes { 0 };
};

[[nodiscard]] std::uint32_t find_memory_type(VkPhysicalDevice physical, std::uint32_t bits,
                                             VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem {};
    vkGetPhysicalDeviceMemoryProperties(physical, &mem);
    for(std::uint32_t i = 0; i < mem.memoryTypeCount; ++i) {
        if((bits & (1u << i)) && (mem.memoryTypes[i].propertyFlags & properties) == properties) return i;
    }
    return UINT32_MAX;
}

[[nodiscard]] bool create_host_image(VkDevice device, VkPhysicalDevice physical,
                                     VkDeviceSize bytes, HostImage& out) {
    VkBufferCreateInfo buffer_info {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bytes,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    if(vkCreateBuffer(device, &buffer_info, nullptr, &out.buffer) != VK_SUCCESS) return false;
    VkMemoryRequirements requirements {};
    vkGetBufferMemoryRequirements(device, out.buffer, &requirements);
    const std::uint32_t type = find_memory_type(physical, requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(type == UINT32_MAX) return false;
    VkMemoryAllocateInfo alloc_info {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = type,
    };
    if(vkAllocateMemory(device, &alloc_info, nullptr, &out.memory) != VK_SUCCESS) return false;
    if(vkBindBufferMemory(device, out.buffer, out.memory, 0) != VK_SUCCESS) return false;
    if(vkMapMemory(device, out.memory, 0, VK_WHOLE_SIZE, 0, &out.mapped) != VK_SUCCESS) return false;
    out.bytes = bytes;
    return true;
}

void destroy_host_image(VkDevice device, HostImage& image) {
    if(image.mapped) vkUnmapMemory(device, image.memory);
    if(image.buffer) vkDestroyBuffer(device, image.buffer, nullptr);
    if(image.memory) vkFreeMemory(device, image.memory, nullptr);
    image = {};
}

/// TGA 2.0：32 位 BGRA、top-left origin（描述符 0x28），PIL 可直接读。
[[nodiscard]] bool write_tga(const std::string& path, std::uint32_t width, std::uint32_t height,
                             const std::uint8_t* pixels, bool source_rgba) {
    std::ofstream file(path, std::ios::binary);
    if(!file) return false;
    std::uint8_t header[18] {
        0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        static_cast<std::uint8_t>(width & 0xff), static_cast<std::uint8_t>((width >> 8) & 0xff),
        static_cast<std::uint8_t>(height & 0xff), static_cast<std::uint8_t>((height >> 8) & 0xff),
        32, 0x28,
    };
    file.write(reinterpret_cast<const char*>(header), sizeof(header));
    if(source_rgba) {
        std::vector<std::uint8_t> row(width * 4);
        for(std::uint32_t y = 0; y < height; ++y) {
            const std::uint8_t* src = pixels + static_cast<std::size_t>(y) * width * 4;
            for(std::uint32_t x = 0; x < width; ++x) {
                row[x * 4 + 0] = src[x * 4 + 2];
                row[x * 4 + 1] = src[x * 4 + 1];
                row[x * 4 + 2] = src[x * 4 + 0];
                row[x * 4 + 3] = src[x * 4 + 3];
            }
            file.write(reinterpret_cast<const char*>(row.data()), static_cast<std::streamsize>(row.size()));
        }
    } else {
        file.write(reinterpret_cast<const char*>(pixels),
            static_cast<std::streamsize>(static_cast<std::size_t>(width) * height * 4));
    }
    return static_cast<bool>(file);
}

} // namespace

void App::setup_mascot_camera() {
    const auto env_float = [](const char* name, float fallback) {
        if(const char* v = std::getenv(name); v && *v) return std::strtof(v, nullptr);
        return fallback;
    };
    auto& transform = camera.transform();
    const float orbit = glm::radians(mascot.orbit_deg);
    const float elevation = glm::radians(mascot.elevation_deg);
    // eye - target：水平环绕 + 抬升；相机看向 target。
    const glm::vec3 offset {
        std::sin(orbit) * std::cos(elevation),
        std::sin(elevation),
        std::cos(orbit) * std::cos(elevation),
    };
    transform.position = mascot.target + offset * std::max(mascot.distance, 0.5f);
    const glm::vec3 view = -offset;
    const float pitch = glm::degrees(std::asin(std::clamp(view.y, -1.0f, 1.0f)));
    const float yaw = glm::degrees(std::atan2(-view.x, -view.z));
    camera.tilt().yaw_deg = yaw;
    camera.tilt().pitch_deg = pitch;
    transform.set_yaw_pitch(yaw, pitch);
    camera.projection().fov_rad = glm::radians(std::clamp(mascot.fov_deg, 12.0f, 90.0f));
    camera.projection().far_plane = 300.0f;

    // 主光相对相机固定：左上方偏后，避免环绕到逆光时整个龙变黑。
    auto& light = sunlight_ref.get();
    const glm::vec3 camera_right = glm::normalize(glm::cross(view, glm::vec3(0.0f, 1.0f, 0.0f)));
    const float side = std::clamp(env_float("FOG_MASCOT_LIGHT_SIDE", 0.85f), 0.0f, 1.0f);
    light.direction = glm::normalize(-view * (0.55f - 0.40f * side)
        + glm::vec3(0.0f, 0.82f - 0.20f * side, 0.0f) - camera_right * (0.35f + 0.55f * side));
    light.color = { 1.0f, 0.95f, 0.84f };
    light.ambient = std::clamp(env_float("FOG_MASCOT_AMBIENT", 0.02f), 0.0f, 1.0f);
    light.specular = true;

    // 参考 fog 场景的黄昏灯笼：左前暖橙、右前冷青、背后深蓝轮廓。
    // 三盏都不投阴影（局部雾只需要光的颜色/强度），阴影仍由太阳方向光承担。
    const glm::vec3 up { 0.0f, 1.0f, 0.0f };
    const glm::vec3 forward = -view;
    const glm::vec3 right = glm::normalize(camera_right);
    local_lights.clear();
    local_lights.push_back(LocalLight{
        .tag = "mascot-warm",
        .position = mascot.target - right * 3.4f + forward * 2.7f - up * 0.15f,
        .color = { 1.0f, 0.42f, 0.10f },
        .intensity = 58.0f,
        .range = 10.0f,
        .radius = 0.055f,
        .enabled = true,
        .cast_shadow = false,
    });
    local_lights.push_back(LocalLight{
        .tag = "mascot-cool",
        .position = mascot.target + right * 3.0f + forward * 1.8f + up * 0.55f,
        .color = { 0.22f, 0.78f, 1.0f },
        .intensity = 14.0f,
        .range = 10.0f,
        .radius = 0.055f,
        .enabled = true,
        .cast_shadow = false,
    });
    local_lights.push_back(LocalLight{
        .tag = "mascot-back",
        .position = mascot.target - forward * 4.0f + right * 1.5f + up * 2.1f,
        .color = { 0.25f, 0.45f, 1.0f },
        .intensity = 22.0f,
        .range = 11.0f,
        .radius = 0.08f,
        .enabled = true,
        .cast_shadow = false,
    });
    local_lights_enabled = true;
    local_light_shadows = false;

    fog_phase = 0.0f;
    animation_paused = true;
    volume.temporal = true;
    volume.jitter = true;
    volume.shadows = true;
    volume.surface_shadow_radius = 2;
    volume.sun_extent = 5.2f;
    volume.debug = int(env_float("FOG_MASCOT_DEBUG", 0.0f));
    volume.exposure_ev = env_float("FOG_MASCOT_EXPOSURE", -0.25f);
    // 只留全局高度雾：龙腹更浓、背部更薄，噪声让雾在龙体上呈团状，而不是整块灰纱。
    volume.weather = 0;
    applied_weather = 0;
    cfg.fog.density = std::clamp(env_float("FOG_MASCOT_FOG", 0.0f), 0.0f, 0.5f);
    cfg.fog.mist_density = std::clamp(env_float("FOG_MASCOT_MIST", 0.0f), 0.0f, 0.5f);
    cfg.fog.height_falloff = 0.25f;
    cfg.fog.ground_y = -0.5f;
    cfg.fog.noise_amount = 1.0f;
    cfg.fog.noise_scale = 0.16f;
    cfg.fog.anim_amplitude = 0.30f;
    fog.params().color = { 0.64f, 0.78f, 0.94f };
    fog.params().start = 0.0f;
}

bool App::capture_swapchain_tga(const std::string& path) {
    if(!renderer || !renderer->swapchain || !renderer->device || renderer->images.empty()) return false;

    const VkDevice device = renderer->device->get_system_handle();
    const VkPhysicalDevice physical = renderer->device->get_physical_device();
    const VkSwapchainKHR swapchain = renderer->swapchain->get_system_handle();
    const VkExtent2D extent = renderer->swapchain->get_extent();
    const VkFormat format = renderer->swapchain->get_surface_format().format;
    if(extent.width == 0 || extent.height == 0) return false;
    if((renderer->swapchain->get_image_usage() & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0) {
        lg(Error) << "mascot capture: swapchain lacks TRANSFER_SRC usage." << std::endl;
        return false;
    }

    // 先 acquire：present 之后交换链图像归 presentation engine 所有，不能直接 transition。
    // 场景每帧都在重画，所有图像都是最终画面，所以任意返回的一张都可用。
    VkSemaphore acquired = VK_NULL_HANDLE;
    VkSemaphoreCreateInfo semaphore_info { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    if(vkCreateSemaphore(device, &semaphore_info, nullptr, &acquired) != VK_SUCCESS) return false;
    std::uint32_t image_index = 0;
    const VkResult acquire = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, acquired,
        VK_NULL_HANDLE, &image_index);
    if(acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
        lg(Error) << "mascot capture: acquire failed (" << static_cast<int>(acquire) << ")." << std::endl;
        vkDestroySemaphore(device, acquired, nullptr);
        return false;
    }
    const VkImage image = renderer->images.at(image_index)->get_system_handle();

    HostImage host;
    if(!create_host_image(device, physical, VkDeviceSize(extent.width) * extent.height * 4, host)) {
        lg(Error) << "mascot capture: host buffer allocation failed." << std::endl;
        vkDestroySemaphore(device, acquired, nullptr);
        destroy_host_image(device, host);
        return false;
    }

    const std::uint32_t family = renderer->swapchain->get_graphics_queue_family();
    const VkQueue queue = renderer->device->get_queue(family);
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo pool_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = family,
    };
    VkCommandBuffer commands = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    if(vkCreateCommandPool(device, &pool_info, nullptr, &pool) != VK_SUCCESS) {
        destroy_host_image(device, host);
        vkDestroySemaphore(device, acquired, nullptr);
        return false;
    }
    VkCommandBufferAllocateInfo allocate_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    vkAllocateCommandBuffers(device, &allocate_info, &commands);
    VkFenceCreateInfo fence_info { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    vkCreateFence(device, &fence_info, nullptr, &fence);

    VkCommandBufferBeginInfo begin_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(commands, &begin_info);

    const VkImageSubresourceRange range { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    VkImageMemoryBarrier to_transfer {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = range,
    };
    vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_transfer);

    VkBufferImageCopy region {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageOffset { 0, 0, 0 },
        .imageExtent { extent.width, extent.height, 1 },
    };
    vkCmdCopyImageToBuffer(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        host.buffer, 1, &region);

    VkImageMemoryBarrier to_present = to_transfer;
    to_present.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    to_present.dstAccessMask = 0;
    to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_present);
    vkEndCommandBuffer(commands);

    // 拷贝完成用信号量通知，再把图像 present 回交换链：这里 acquire 来的图像如果只读不还，
    // 交换链可用图像会越来越少，连续截帧最终让主循环 acquire 失败
    // （mascot 只截一次，所以这个泄漏一直没暴露）。
    VkSemaphore rendered = VK_NULL_HANDLE;
    {
        VkSemaphoreCreateInfo info { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        if(vkCreateSemaphore(device, &info, nullptr, &rendered) != VK_SUCCESS) {
            vkDestroyFence(device, fence, nullptr);
            vkDestroyCommandPool(device, pool, nullptr);
            destroy_host_image(device, host);
            vkDestroySemaphore(device, acquired, nullptr);
            return false;
        }
    }

    const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit_info {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &acquired,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &commands,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &rendered,
    };
    bool ok = vkQueueSubmit(queue, 1, &submit_info, fence) == VK_SUCCESS
        && vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS;

    // 读回已经完成，把图像还回交换链（present 等拷贝做完）
    VkPresentInfoKHR present_info {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &rendered,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &image_index,
    };
    const VkResult presented = vkQueuePresentKHR(queue, &present_info);
    if(presented != VK_SUCCESS && presented != VK_SUBOPTIMAL_KHR) {
        lg(Error) << "mascot capture: present back failed (" << static_cast<int>(presented) << ")." << std::endl;
        ok = false;
    }
    // present 还在队列里，等它做完再销毁信号量
    vkQueueWaitIdle(queue);

    if(ok) {
        const bool bgra = format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_B8G8R8A8_UNORM
            || format == VK_FORMAT_B8G8R8A8_SNORM;
        const bool rgba = format == VK_FORMAT_R8G8B8A8_SRGB || format == VK_FORMAT_R8G8B8A8_UNORM
            || format == VK_FORMAT_R8G8B8A8_SNORM || format == VK_FORMAT_A8B8G8R8_SRGB_PACK32
            || format == VK_FORMAT_A8B8G8R8_UNORM_PACK32;
        if(!bgra && !rgba) {
            lg(Error) << "mascot capture: unsupported swapchain format "
                      << static_cast<int>(format) << "." << std::endl;
            ok = false;
        } else {
            ok = write_tga(path, extent.width, extent.height,
                static_cast<const std::uint8_t*>(host.mapped), rgba);
        }
    }

    vkDestroyFence(device, fence, nullptr);
    vkDestroyCommandPool(device, pool, nullptr);
    destroy_host_image(device, host);
    vkDestroySemaphore(device, rendered, nullptr);
    vkDestroySemaphore(device, acquired, nullptr);
    return ok;
}

auto App::run_mascot() -> int {
    if(!window || !renderer) return 1;
    setup_mascot_camera();

    const int frames = std::max(mascot.frames, 3);
    for(int frame = 0; frame < frames; ++frame) {
        window->poll_events();
        if(window->should_close()) break;
        render_scene();
    }
    if(renderer->wait_idle() != VK_SUCCESS) {
        lg(Error) << "mascot capture: device wait idle failed." << std::endl;
        return 1;
    }
    if(!capture_swapchain_tga(mascot.output)) {
        lg(Error) << "mascot capture failed: " << mascot.output << std::endl;
        return 1;
    }
    lg(Info) << "mascot capture written: " << mascot.output << std::endl;
    return 0;
}
