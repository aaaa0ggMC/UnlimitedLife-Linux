#include "app.h"

namespace avetest {

void App::render_frame(RenderCache rc) {
    auto graphics = renderer->acquire_context();
    graphics.begin();
    graphics.bind_pipeline(rc.pipeline);
    graphics.bind_vertex_buffer(rc.buffer);
    graphics.draw(rc.vertex_count);
    graphics.end();
}

} // namespace avetest
