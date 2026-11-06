/**
 * @file app_run.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 
 * @version 5.0
 * @date 2026-10-14
 * 
 * @copyright Copyright (c) 2026
 * @parblock audit history (AI 别写这里)
 * 1. OK 没怎么动过
 * 2. OK 没变
 * @endparblock
 * 
 */
#include "app.h"
#ifdef __ANDROID__
#include <android/log.h>
#endif

using enum alib5::Severity;

auto App::run() -> int {
    if(mascot.enabled) return run_mascot();

    // 桌面端外部控制通道：未设 FOG_CONTROL 时 start() 直接返回，行为不变
    control.start();
    if(!control.socket_path.empty()) {
        lg(Info) << "Control channel: " << control.socket_path
                 << " (commands: ping, quit, shot <path>, ui on|off, cam x y z [yaw [pitch]], preset <0..7>)"
                 << std::endl;
    }

    ave::misc::FPSDetective detective;
    detective.start();

    lg(Info) << "Controls: M lock cursor (camera only moves while locked), WASD move, Space/E world up, "
             << "Shift/Q world down, mouse look, B toggle specular, "
             << "G/F adjust optional global near cutoff, F1 toggle UI, ESC quit."
             << std::endl;
    alib5::RateLimiter world_tick(world_update_rate);
#ifdef __ANDROID__
    auto benchmark_start = std::chrono::steady_clock::now();
    std::uint64_t benchmark_frames = 0;
#endif
    while (!window->should_close() && !control.quit()) {
        // 每帧：平台事件先进输入状态机（放最前面，这一帧的边沿才不会被后面的推进吃掉）
        window->poll_events();
        // Android Surface 销毁/退出可能在 poll 中到达，不再访问失效的窗口绘制。
        if(window->should_close()) break;

        if(world_tick.until()) {
            window->process_events();

            if(input.is_key_just_pressed(ave::KeyCode::M)) {
                const bool lock = window->get_cursor_mode() != ave::CursorMode::Disabled;
                window->set_cursor_mode(lock ? ave::CursorMode::Disabled : ave::CursorMode::Normal);
                lg(Info) << "Cursor " << (lock ? "locked (mouse look)" : "released") << std::endl;
            }
            if(input.is_key_down(ave::KeyCode::Escape)) break;

            update_camera(world_update_dt);
            update_environment(world_update_dt);

            input.end_frame();
        }

        // 控制通道下发的界面显隐 / 相机位姿（等价于 F1 与手动摆机位）
        bool ui_request;
        if(control.consume_ui(ui_request)) {
            ui_visible = ui_request;
            lg(Info) << "Control channel UI " << (ui_visible ? "on" : "off") << std::endl;
        }
        std::string camera_spec;
        if(control.consume_camera(camera_spec)) {
            apply_control_camera(camera_spec);
        }
        int preset_request;
        if(control.consume_preset(preset_request)) {
            apply_scene_preset(preset_request);
            lg(Info) << "Control channel scene preset: " << preset_request << std::endl;
        }

        // ImGui 界面：每渲染帧起帧 + 构建（draw data 在 render_scene 内录进当前 pass）
        ui_frame();

        render_scene();
#ifdef __ANDROID__
        if(android_benchmark) {
            ++benchmark_frames;
            const auto now = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double>(now - benchmark_start).count();
            if(elapsed >= 2.0) {
                __android_log_print(ANDROID_LOG_INFO, "FogBenchmark",
                    "fps=%.3f frame_ms=%.3f frames=%llu | %s",
                    double(benchmark_frames) / elapsed, elapsed * 1000.0 / double(benchmark_frames),
                    static_cast<unsigned long long>(benchmark_frames), gpu_status.c_str());
                benchmark_frames = 0;
                benchmark_start = now;
            }
        }
#endif

        // 控制通道的截帧请求：Vulkan 调用只能在主线程的安全点执行
        std::string shot_path;
        if(control.consume_shot(shot_path)) {
            if(capture_swapchain_tga(shot_path)) {
                lg(Info) << "Control channel capture written: " << shot_path << std::endl;
            } else {
                lg(Error) << "Control channel capture failed: " << shot_path << std::endl;
            }
        }

        detective.next_frame();
    }

    control.stop();

    lg << "\n" << detective.table() << std::endl;
    return 0;
}
