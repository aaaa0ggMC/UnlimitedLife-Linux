#ifdef __ANDROID__
#include <AVE/env/android_window.h>
#include <android/native_window.h>
#include <vulkan/vulkan_android.h>
#include <atomic>

namespace ave::android {
    namespace {
        thread_local ANativeWindow* native_window = nullptr;
        thread_local void (*event_pump)() = nullptr;
        std::atomic_bool closing {false};
    }
    void set_native_window(ANativeWindow* window) noexcept { native_window = window; }
    ANativeWindow* get_native_window() noexcept { return native_window; }
    void set_event_pump(void (*pump)()) noexcept { event_pump = pump; }
    void pump_events() { if (event_pump) event_pump(); }
    void request_close() noexcept { closing.store(true); }
    void reset_close() noexcept { closing.store(false); }
    bool close_requested() noexcept { return closing.load(); }
}

namespace ave {
    namespace { thread_local Window* active_window = nullptr; }
    bool Window::create(const CreateWindowInfo& ci) {
        if (window || active_window) {
            ci.ew.report(ave_already_created, "Android window already attached.");
            return false;
        }
        window = android::get_native_window();
        if (!window) {
            ci.ew.report(ave_vk_create_surface, "No SurfaceView ANativeWindow bound to this thread.");
            return false;
        }
        ANativeWindow_acquire(window);
        last_size = get_framebuffer_size();
        cursor_mode = ci.cursor_mode;
        debounce = ci.resize_debounce_time;
        close_delivered = false;
        active_window = this;
        return true;
    }
    void Window::destroy() noexcept {
        if (active_window == this) active_window = nullptr;
        if (window) ANativeWindow_release(window);
        window = nullptr;
        input = nullptr;
    }
    std::pair<int, int> Window::get_framebuffer_size() const {
        if (!window) return {0, 0};
        return {ANativeWindow_getWidth(window), ANativeWindow_getHeight(window)};
    }
    void Window::set_size(int width, int height) {
        if (window) ANativeWindow_setBuffersGeometry(window, width, height, 0);
    }
    void Window::poll_events() {
        android::pump_events();
        if (active_window) active_window->process_events();
    }
    void Window::process_events() {
        if (should_close()) {
            if (!close_delivered) {
                close_delivered = true;
                if (input) input->reset_state();
                WindowFocusEvent focus(false);
                dispatch_event(focus);
                WindowCloseEvent close;
                dispatch_event(close);
            }
            return;
        }
        const auto size = get_framebuffer_size();
        if (size == last_size) return;
        last_size = size;
        WindowResizeEvent resize(size.first, size.second);
        dispatch_event(resize);
        WindowFramebufferResizeEvent framebuffer(size.first, size.second);
        dispatch_event(framebuffer);
        // SurfaceView already coalesces layout. Rebuild before rendering its new extent.
        if (size.first > 0 && size.second > 0) {
            AfterWindowFramebufferResizeEvent after(size.first, size.second);
            dispatch_event(after);
        }
    }
    VkSurfaceKHR Window::create_vulkan_surface(const std::shared_ptr<Instance>& instance,
                                               ErrorWrapper ew) const {
        if (!window || !instance || !instance->get_system_handle()) {
            ew.report(ave_vk_create_surface, "Android surface requires an instance and native window.");
            return VK_NULL_HANDLE;
        }
        const auto create_surface = reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
            vkGetInstanceProcAddr(instance->get_system_handle(), "vkCreateAndroidSurfaceKHR"));
        if (!create_surface) {
            ew.report(ave_vk_create_surface, "VK_KHR_android_surface is unavailable.");
            return VK_NULL_HANDLE;
        }
        VkAndroidSurfaceCreateInfoKHR info {};
        info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        info.window = window;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        const auto result = create_surface(instance->get_system_handle(), &info,
                                            instance->get_vk_allocator(), &surface);
        if (result != VK_SUCCESS) {
            ew.report(ave_vk_create_surface, "Cannot create Android Vulkan surface ({}).", int(result));
            return VK_NULL_HANDLE;
        }
        return surface;
    }
}
#endif
