#pragma once

#include <AVE/alib.h>
#include <AVE/error_code.h>
#include <AVE/render/instance.h>

/**
 * @file surface.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Vulkan surface wrapper
 * @version 5.5
 * @date 2026-10-27
 *
 * @copyright Copyright (c) 2026
 *
 * @note 本模块不依赖窗口/GLFW：VkSurfaceKHR 由平台层（GLFW 窗口、Android ANativeWindow 等）
 * 创建后交给 Surface 接管，render 模块因此可以独立构建到其他平台。
 */
#include <AVE/config.h>



namespace ave {

    struct AVE_API CreateSurfaceInfo {
        std::shared_ptr<Instance> instance;
        /// 已由平台层创建的 surface；Surface 接管其所有权。
        VkSurfaceKHR surface { VK_NULL_HANDLE };
        mutable ave::ErrorWrapper ew {};
    };

    /**
     * @brief 平台无关的 VkSurfaceKHR 所有者。
     *
     * Surface 通过 shared_ptr 保证 Vulkan Instance 存活；调用方负责在销毁
     * Surface 之前保证原生窗口存活（本模块不再持有窗口引用）。
     */
    class AVE_API Surface final {
    private:
        std::shared_ptr<Instance> instance;
        VkSurfaceKHR surface { VK_NULL_HANDLE };

        explicit Surface() noexcept = default;

        bool initialize(CreateSurfaceInfo ci) {
            if(!ci.instance ||
               ci.instance->get_system_handle() == VK_NULL_HANDLE) {
                ci.ew.report(
                    ave_vk_create_surface,
                    "Cannot adopt a Vulkan surface without a valid instance."
                );
                return false;
            }

            if(ci.surface == VK_NULL_HANDLE) {
                ci.ew.report(
                    ave_vk_create_surface,
                    "Cannot adopt a null Vulkan surface handle."
                );
                return false;
            }

            instance = std::move(ci.instance);
            surface = ci.surface;
            return true;
        }

    public:
        ~Surface() { destroy(); }

        Surface(const Surface&) = delete;
        Surface& operator=(const Surface&) = delete;
        Surface(Surface&&) = delete;
        Surface& operator=(Surface&&) = delete;

        /// @brief 接管平台层创建的 surface（GLFW: Window::create_vulkan_surface）。
        [[nodiscard]] static std::shared_ptr<Surface> create(
            CreateSurfaceInfo ci
        ) {
            auto result = std::shared_ptr<Surface>(new Surface());
            if(!result->initialize(std::move(ci))) return {};
            return result;
        }

        void destroy() noexcept {
            if(surface != VK_NULL_HANDLE && instance) {
                vkDestroySurfaceKHR(
                    instance->get_system_handle(),
                    surface,
                    instance->get_vk_allocator()
                );
            }

            surface = VK_NULL_HANDLE;
            instance.reset();
        }

        [[nodiscard]] VkSurfaceKHR get_system_handle() const noexcept {
            return surface;
        }

        [[nodiscard]] const std::shared_ptr<Instance>& get_instance() const noexcept {
            return instance;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return surface != VK_NULL_HANDLE;
        }
    };
}
