#include "app.h"

namespace avetest {

void App::run() {
    RenderCache rc {
        .pipeline = *pipeline,
        .buffer = *vertex_buffer,
        .vertex_count = vertex_count
    };

    ave::misc::FPSDetective detective;
    detective.start();

    while (!window->should_close()) {
        window->poll_events();
        render_frame(rc);
        detective.next_frame();
    }

    lg << "\n" << detective.table() << std::endl;
}

} // namespace avetest
