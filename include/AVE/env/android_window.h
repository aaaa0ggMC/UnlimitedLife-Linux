#pragma once

#include <AVE/context.h>
#include <AVE/error_code.h>
#include <AVE/render/instance.h>
#include <AVE/window/input.h>
#include <AVE/window/cursor.h>
#include <AVE/env/android.h>

namespace ave {
    // SurfaceView owns layout, title, visibility and focus on Android.
    enum class WindowStyle : ave::u32 {
        FollowGLFW = 1u << 31, Resizable = 1u, Visible = 2u, Decorated = 4u,
        Focused = 8u, AutoIconify = 16u, Floating = 32u, Maximized = 64u,
        CenterCursor = 128u, TransparentFramebuffer = 256u, FocusOnShow = 512u,
        ScaleToMonitor = 1024u, ScaleFramebuffer = 2048u
    };
    inline constexpr WindowStyle operator|(WindowStyle a, WindowStyle b) {
        return static_cast<WindowStyle>(static_cast<u32>(a) | static_cast<u32>(b));
    }
    inline constexpr WindowStyle operator&(WindowStyle a, WindowStyle b) {
        return static_cast<WindowStyle>(static_cast<u32>(a) & static_cast<u32>(b));
    }
    inline constexpr bool has_style(WindowStyle a, WindowStyle b) {
        return static_cast<u32>(a & b) != 0;
    }
    inline constexpr WindowStyle window_style_normal = WindowStyle::Visible;

    struct AVE_API CreateWindowInfo {
        Context& ctx;
        std::string title = "Hello from AVE";
        u32 width = 1920, height = 1080;
        WindowStyle style = window_style_normal;
        void* monitor = nullptr; // Not supported on Android.
        std::optional<std::pair<int, int>> position;
        double resize_debounce_time = 0.5;
        CursorMode cursor_mode = CursorMode::Normal;
        mutable ErrorWrapper ew = {};
    };
    template<typename EventT> struct Listener {
        usize index = 0;
        static constexpr EventType type = EventT::get_static_type();
    };

    class AVE_API Window {
        ANativeWindow* window = nullptr;
        using Storage = alib5::storage::FreelistLinearStorage<std::function<void(Event&)>>;
        std::array<Storage, static_cast<size_t>(EventType::Count)> listeners;
        std::function<void(Event&)> callback;
        Input* input = nullptr;
        CursorMode cursor_mode = CursorMode::Normal;
        std::pair<int, int> last_size {};
        double debounce = 0.5;
        bool close_delivered = false;
    public:
        Window() = default;
        explicit Window(const CreateWindowInfo& ci) { create(ci); }
        ~Window() { destroy(); }
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&) = delete;
        Window& operator=(Window&&) = delete;
        bool create(const CreateWindowInfo& ci);
        void destroy() noexcept;
        explicit operator bool() const noexcept { return window != nullptr; }
        bool should_close() const noexcept { return !window || android::close_requested(); }
        void* get_native_handle() const noexcept { return window; }
        ANativeWindow* get_system_handle() const noexcept { return window; }
        static void poll_events();
        void process_events();
        VkSurfaceKHR create_vulkan_surface(const std::shared_ptr<Instance>& instance,
                                           ErrorWrapper ew = {}) const;
        std::pair<int, int> get_framebuffer_size() const;
        std::pair<int, int> get_size() const { return get_framebuffer_size(); }
        std::pair<float, float> get_content_scale() const { return {1.0f, 1.0f}; }
        // Changes buffer geometry, not SurfaceView layout. 0,0 restores native size.
        void set_size(int width, int height);
        void set_position(int, int) {} // Activity owns layout.
        void set_title(std::string_view) {} // Activity owns title.
        void set_resize_debounce_time(double seconds) noexcept { debounce = seconds; }
        double get_resize_debounce_time() const noexcept { return debounce; }
        void set_event_callback(std::function<void(Event&)> cb) { callback = std::move(cb); }
        void bind_input(Input& value) noexcept { input = &value; }
        void unbind_input() noexcept { input = nullptr; }
        Input* get_bound_input() const noexcept { return input; }
        CursorMode get_cursor_mode() const noexcept { return cursor_mode; }
        // Touch has no cursor to hide/lock; retain intent for common camera code.
        void set_cursor_mode(CursorMode mode) noexcept {
            if (mode == cursor_mode) return;
            cursor_mode = mode;
            if (input) input->notify_cursor_mode_changed();
        }
        void reapply_cursor_mode() noexcept {}
        template<typename EventT, typename Func> Listener<EventT> on(Func&& cb) {
            constexpr auto idx = static_cast<size_t>(EventT::get_static_type());
            static_assert(idx < static_cast<size_t>(EventType::Count));
            bool fresh = false;
            usize slot = 0;
            listeners[idx].try_next_with_index(fresh, slot,
                [fn = std::forward<Func>(cb)](Event& e) { fn(static_cast<EventT&>(e)); });
            return {slot};
        }
        template<typename EventT> bool delete_listener(Listener<EventT> listener) {
            auto& storage = listeners[static_cast<size_t>(EventT::get_static_type())];
            if (!storage.is_occupied(listener.index)) return false;
            storage.remove(listener.index);
            return true;
        }
        template<typename EventT> size_t get_listener_count() const noexcept {
            return ave::freelist_occupied_count(listeners[static_cast<size_t>(EventT::get_static_type())]);
        }
        template<typename EventT> bool has_listener() const noexcept {
            return get_listener_count<EventT>() != 0;
        }
        void dispatch_event(Event& event) {
            if (input) input->on_event(event);
            if (callback) callback(event);
            const auto idx = static_cast<size_t>(event.get_type());
            if (idx >= listeners.size()) return;
            listeners[idx].for_each([&](auto& fn) { if (!event.handled) fn(event); });
        }
    };
}
