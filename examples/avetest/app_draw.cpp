#include "app.h"

namespace avetest {

void App::render_frame(RenderCache rc, const PushConstant& pc) {
    auto graphics = renderer->acquire_context();
    graphics.begin();
    graphics.bind_pipeline(rc.pipeline);
    graphics.bind_descriptor_set(0, descriptor_set);
    graphics.push_constant(pc);
    graphics.bind_vertex_buffer(vertices_data);
    graphics.bind_index_buffer<std::uint16_t>(indices_data);
    graphics.draw_indexed(rc.index_count);
    graphics.end();
}

} // namespace avetest
