#include "app.h"

namespace avetest {

void App::render_frame(RenderCache rc, const PushConstant& pc) {
    auto graphics = renderer->acquire_context();
    graphics.begin();
    graphics.bind_pipeline(rc.pipeline);
    graphics.push_constant(pc);
    graphics.bind_vertex_buffer(vertices_data);
    graphics.draw(rc.vertex_count);
    graphics.end();
}

} // namespace avetest
