#pragma once

#ifdef __ANDROID__
#include <AVE/config.h>
struct ANativeWindow;

namespace ave::android {
    // One Android render session per process. Bind/pump only on its render thread.
    // The host retains the supplied window until the render thread has joined.
    AVE_API void set_native_window(ANativeWindow* window) noexcept;
    AVE_API ANativeWindow* get_native_window() noexcept;
    AVE_API void set_event_pump(void (*pump)()) noexcept;
    AVE_API void pump_events();
    // Thread-safe lifecycle requests; reset only after the previous thread joined.
    AVE_API void request_close() noexcept;
    AVE_API void reset_close() noexcept;
    AVE_API bool close_requested() noexcept;
}
#endif
