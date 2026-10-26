#include "app.h"

namespace avetest {

void App::render_frame(const PushConstant& pc) {
    auto graphics = renderer->acquire_context();
    graphics.begin();
    graphics.bind_pipeline(*pipeline);
    graphics.bind_descriptor_set(0, descriptor_set);
    graphics.push_constant(pc);
    graphics.bind_vertex_buffer(vertices_data);
    graphics.bind_index_buffer<std::uint16_t>(indices_data);
    graphics.draw_indexed(index_count);
    graphics.end();
}

} // namespace avetest
