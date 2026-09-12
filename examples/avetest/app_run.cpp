#include "app.h"

namespace avetest {

void App::run() {
    RenderCache rc {
        .pipeline = *pipeline,
        .buffer = *vertex_buffer
    };

    while (!window->should_close()) {
        window->poll_events();
        render_frame(rc);
    }
}

} // namespace avetest
