#include <chrono>

#include "app.h"

using enum alib6::Severity;

auto App::run() -> int {
    ave::misc::FPSDetective detective;
    detective.start();

    lg(Info) << "Controls: WASD/QE move, mouse look, Shift sprint, "
             << "M lock/unlock cursor, wheel/R/T adjust fog end, G/F adjust fog start, ESC quit."
             << std::endl;

    auto last = std::chrono::steady_clock::now();
    while (!window->should_close()) {
        // 1. 事件：new_frame 先推进上一帧生命周期，再 poll 驱动 GLFW 回调写入 Input，
        //    保证 just_pressed / 本帧按键状态在随后的查询中可见。
        input.new_frame();
        ave::Window::poll_events();
        window->process_events(); // 防抖后的 AfterWindowFramebufferResize → swapchain 重建

        // M：锁定/释放光标（Disable 时隐藏并锁定，鼠标增量不越界；失焦会自动释放）
        if(input.is_key_just_pressed(ave::KeyCode::M)) {
            const bool lock = window->get_cursor_mode() != ave::CursorMode::Disabled;
            window->set_cursor_mode(lock ? ave::CursorMode::Disabled : ave::CursorMode::Normal);
            lg(Info) << "Cursor " << (lock ? "locked (mouse look)" : "released") << std::endl;
        }

        if(input.is_key_down(ave::KeyCode::Escape)) break;

        const auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        dt = std::min(dt, 0.1f);

        // 2. 系统驱动：相机 / 全局雾参数
        update_camera(dt);
        update_fog(dt);

        // 3. 渲染：view 查询 (Transform, Renderable) 逐物体绘制
        render_scene();

        detective.next_frame();
    }

    lg << "\n" << detective.table() << std::endl;
    return 0;
}
