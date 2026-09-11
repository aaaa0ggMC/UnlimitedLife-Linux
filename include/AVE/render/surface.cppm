/**
 * @file surface.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Vulkan surface wrapper
 * @version 5.0
 * @date 2026-09-11
 *
 * @copyright Copyright (c) 2026
 */
module;
#include <AVE/config.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

export module ave.render:surface;

import std;
import alib6;
import ave.ecode;
import ave.window;
import :instance;

export namespace ave {

    struct AVE_API CreateSurfaceInfo {
        mutable alib6::ErrorWrapper ew = {};
    };

    /**
     * @brief 包装由 GLFW 窗口创建的 VkSurfaceKHR。
     *
     * Surface 会通过 shared_ptr 保证 Vulkan Instance 存活，但持有的 Window&
     * 不具有所有权。用户必须保证 Window 的生命周期长于由它创建的所有 Surface
     * 及其 Swapchain，并且在这些对象销毁前不得调用 Window::destroy()。
     */
    class AVE_API Surface final {
    private:
        Window& window;
        std::shared_ptr<Instance> instance;
        VkSurfaceKHR surface { VK_NULL_HANDLE };

        explicit Surface(Window& target_window) noexcept
        :window(target_window){}

        bool initialize(
            std::shared_ptr<Instance> target_instance,
            const CreateSurfaceInfo& ci
        ) {
            if(!target_instance ||
               target_instance->get_system_handle() == VK_NULL_HANDLE) {
                ci.ew.report(
                    ave_vk_create_surface,
                    "Cannot create a Vulkan surface without a valid instance."
                );
                return false;
            }

            if(!window || window.get_system_handle() == nullptr) {
                ci.ew.report(
                    ave_vk_create_surface,
                    "Cannot create a Vulkan surface without a valid window."
                );
                return false;
            }

            instance = std::move(target_instance);
            const VkResult code = glfwCreateWindowSurface(
                instance->get_system_handle(),
                window.get_system_handle(),
                instance->get_vk_allocator(),
                &surface
            );

            if(code != VK_SUCCESS) {
                ci.ew.report(
                    ave_vk_create_surface,
                    "Failed to create Vulkan window surface ({}).",
                    static_cast<int>(code)
                );
                instance.reset();
                surface = VK_NULL_HANDLE;
                return false;
            }

            return true;
        }

    public:
        ~Surface() { destroy(); }

        Surface(const Surface&) = delete;
        Surface& operator=(const Surface&) = delete;
        Surface(Surface&&) = delete;
        Surface& operator=(Surface&&) = delete;

        [[nodiscard]] static std::shared_ptr<Surface> create(
            std::shared_ptr<Instance> instance,
            Window& window,
            const CreateSurfaceInfo& ci = {}
        ) {
            auto result = std::shared_ptr<Surface>(new Surface(window));
            if(!result->initialize(std::move(instance), ci)) return {};
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

        [[nodiscard]] Window& get_window() const noexcept {
            return window;
        }

        [[nodiscard]] const std::shared_ptr<Instance>& get_instance() const noexcept {
            return instance;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return surface != VK_NULL_HANDLE;
        }
    };
}
