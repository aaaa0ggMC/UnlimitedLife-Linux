#pragma once

#ifdef __ANDROID__
#include <AVE/config.h>
#include <utility>
struct ANativeWindow;

namespace ave::android {
    // One Android render session per process. Bind/pump only on its render thread.
    // The host retains the supplied window until the render thread has joined.
    AVE_API void set_native_window(ANativeWindow* window) noexcept;
    AVE_API ANativeWindow* get_native_window() noexcept;
    // SurfaceView consumer extent, not the buffer geometry changed by Vulkan WSI.
    // Set before rendering starts and on surfaceChanged; callable from the UI thread.
    AVE_API void set_window_size(int width, int height) noexcept;
    AVE_API std::pair<int, int> get_window_size() noexcept;
    AVE_API void set_event_pump(void (*pump)()) noexcept;
    AVE_API void pump_events();
    // Thread-safe lifecycle requests; reset only after the previous thread joined.
    AVE_API void request_close() noexcept;
    AVE_API void reset_close() noexcept;
    AVE_API bool close_requested() noexcept;
}
#endif
