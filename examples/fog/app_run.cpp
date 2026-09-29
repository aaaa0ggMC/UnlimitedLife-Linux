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
        input.new_frame();
        ave::Window::poll_events();
        window->process_events();

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

        update_camera(dt);
        update_fog(dt);
        
        render_scene();

        detective.next_frame();
    }

    lg << "\n" << detective.table() << std::endl;
    return 0;
}
