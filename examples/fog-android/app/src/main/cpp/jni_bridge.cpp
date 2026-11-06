#include <jni.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <AVE/env/android.h>
#include <imgui.h>
#include "app.h"
#include <deque>
#include <filesystem>
#include <mutex>
#include <thread>
#include <cmath>

namespace {
    constexpr const char* tag = "FogAndroid";
    std::thread render_thread;
    std::mutex touch_mutex, error_mutex;
    struct Touch { int action; float x, y; };
    std::deque<Touch> touches;
    std::string render_error;

    /// 启动界面选择的 Froxel 精度；必须在 App::setup() 之前写入 App::volume，
    /// 因为 setup_ubo() 据此分配 froxel / injection / history SSBO。
    /// 取值范围与桌面端 VolumeSettings 一致（pixel_size 4..32，depth_slices 16..96）。
    struct FroxelRequest {
        int pixel_size { 8 };
        int depth_slices { 64 };
        int light_steps { 4 };
    };

    void report_error(std::string message) {
        __android_log_print(ANDROID_LOG_ERROR, tag, "%s", message.c_str());
        std::lock_guard lock(error_mutex);
        render_error = std::move(message);
    }

    void pump_touch() {
        std::deque<Touch> pending;
        {
            std::lock_guard lock(touch_mutex);
            pending.swap(touches);
        }
        if (!ImGui::GetCurrentContext()) return;
        auto& io = ImGui::GetIO();
        for (const auto& touch : pending) {
            io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
            if (touch.action != 3) io.AddMousePosEvent(touch.x, touch.y);
            if (touch.action == 0) io.AddMouseButtonEvent(0, true);
            if (touch.action == 1 || touch.action == 3) io.AddMouseButtonEvent(0, false);
        }
    }

    // Called only on the Activity's UI thread. Never wait with a queue mutex held.
    void stop() {
        ave::android::request_close();
        if (render_thread.joinable()) render_thread.join();
        std::lock_guard lock(touch_mutex);
        touches.clear();
    }

    void render(ANativeWindow* native_window, std::string asset_root, float render_scale,
                FroxelRequest froxel, int diagnostics) noexcept {
        // fromSurface acquired the host reference; Window acquires its own reference.
        struct ReleaseWindow {
            ANativeWindow* window;
            ~ReleaseWindow() {
                ave::android::set_event_pump(nullptr);
                ave::android::set_native_window(nullptr);
                ANativeWindow_release(window);
            }
        } release {native_window};
        std::filesystem::path previous;
        try {
            previous = std::filesystem::current_path();
            std::filesystem::current_path(asset_root);
            ave::android::set_native_window(native_window);
            ave::android::set_event_pump(pump_touch);
            AppConfig config;
            const auto loaded = alib5::compact::load_config_from_file(config_file, config);
            if (!loaded.success) throw std::runtime_error("Cannot load packaged config.fog.json");
            // UI uses buffer pixels too: keep its physical size stable after upscaling.
            config.ui.scale *= render_scale;
            if (diagnostics & 16) config.shape.path.clear();
            if (!ave::android::close_requested()) {
                App app(config);
                // Froxel precision from the launcher picker; applied before setup_ubo()
                // allocates the froxel/injection/history buffers and rebuilds the grid.
                app.volume.pixel_size = std::clamp(froxel.pixel_size, 4, 32);
                app.volume.depth_slices = std::clamp(froxel.depth_slices, 16, 96);
                // 0 合法且有意义：lighting.glsl 的 medium_visibility 会整体跳过，
                // 与 app_ui.cpp 的滑杆范围 0..8 一致（原先这里误 clamp 成 1..16）。
                app.volume.light_steps = std::clamp(froxel.light_steps, 0, 8);
                // 表面阴影走最低 PCF（单 tap）：桌面是太阳 25 tap + 5 个投影点光各 25 tap
                // = 150 次阴影采样/fragment，是移动端 scene+UI 的主要开销。桌面保持 5×5。
                app.volume.surface_shadow_radius = 0;
                app.android_benchmark = (diagnostics & 1) != 0;
                app.android_no_volume = (diagnostics & 2) != 0;
                app.local_lights_enabled = (diagnostics & 4) == 0;
                app.volume.shadows = (diagnostics & 8) == 0;
                app.ui_visible = (diagnostics & 32) == 0;
                __android_log_print(ANDROID_LOG_INFO, "FogBenchmark",
                    "config flags=%d no_volume=%d lights=%d shadows=%d dragon=%d ui=%d pixel=%d depth=%d steps=%d",
                    diagnostics, app.android_no_volume, app.local_lights_enabled, app.volume.shadows,
                    !config.shape.path.empty(), app.ui_visible,
                    app.volume.pixel_size, app.volume.depth_slices, app.volume.light_steps);
                __android_log_print(ANDROID_LOG_INFO, tag,
                    "Froxel precision: pixel_size=%d depth_slices=%d light_steps=%d",
                    app.volume.pixel_size, app.volume.depth_slices, app.volume.light_steps);
                app.setup();
                if (diagnostics & 64) {
                    // Diagnostic control-field volume: the expensive local-fog case.
                    app.spawn_diagnostic_local_mist();
                    __android_log_print(ANDROID_LOG_INFO, "FogBenchmark",
                        "spawned diagnostic local mist");
                }
                // Pixel-based saved layouts from another resolution must not place
                // this session's controls outside a smaller render buffer.
                if (ImGui::GetCurrentContext()) ImGui::GetIO().IniFilename = nullptr;
                if (!ave::android::close_requested()) app.run();
                // App destructor waits for GPU and destroys UI before native window release.
            }
        } catch (const std::exception& error) {
            report_error(error.what());
        } catch (...) {
            report_error("Unknown native rendering failure");
        }
        if (!previous.empty()) {
            std::error_code error;
            std::filesystem::current_path(previous, error);
        }
    }
}

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM*, void*) { return JNI_VERSION_1_6; }

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_fogandroid_MainActivity_nativeStart(JNIEnv* env, jobject, jobject surface, jstring root,
                                                     jfloat render_scale, jint froxel_pixel_size,
                                                     jint froxel_depth_slices, jint froxel_light_steps,
                                                     jint diagnostic_flags) {
    stop();
    if (!std::isfinite(render_scale) || render_scale <= 0.0f || render_scale > 1.0f)
        return env->NewStringUTF("Invalid render resolution scale");
    const char* text = env->GetStringUTFChars(root, nullptr);
    if (!text) return nullptr; // JNI exception already pending.
    std::string asset_root(text);
    env->ReleaseStringUTFChars(root, text);
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) return env->NewStringUTF("Cannot acquire SurfaceView native window");
    {
        std::lock_guard lock(error_mutex);
        render_error.clear();
    }
    ave::android::reset_close();
    try {
        render_thread = std::thread(render, window, std::move(asset_root), render_scale,
                                    FroxelRequest { int(froxel_pixel_size), int(froxel_depth_slices),
                                                    int(froxel_light_steps) }, int(diagnostic_flags));
    } catch (const std::exception& error) {
        ANativeWindow_release(window);
        return env->NewStringUTF(error.what());
    }
    return env->NewStringUTF("");
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_fogandroid_MainActivity_nativeStop(JNIEnv*, jobject) { stop(); }

extern "C" JNIEXPORT void JNICALL
Java_com_example_fogandroid_MainActivity_nativeResize(JNIEnv*, jobject, jint width, jint height) {
    // Keep the consumer's logical extent independent of swapchain buffer geometry.
    ave::android::set_window_size(width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_fogandroid_MainActivity_nativeTouch(JNIEnv*, jobject, jint action, jfloat x, jfloat y) {
    std::lock_guard lock(touch_mutex);
    if (action == 2 && !touches.empty() && touches.back().action == 2) {
        touches.back() = {action, x, y};
        return;
    }
    if (touches.size() >= 256) {
        touches.clear();
        touches.push_back({3, 0, 0}); // Never leave a held mouse button after overflow.
    }
    touches.push_back({action, x, y});
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_fogandroid_MainActivity_nativeError(JNIEnv* env, jobject) {
    std::lock_guard lock(error_mutex);
    return env->NewStringUTF(render_error.c_str());
}
