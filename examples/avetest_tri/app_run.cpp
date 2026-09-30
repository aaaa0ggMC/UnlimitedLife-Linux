#include "app.h"

namespace avetest_tri {

namespace {
    double env_double(const char* name, double fallback) {
        const char* v = std::getenv(name);
        return (v && *v) ? std::atof(v) : fallback;
    }
}

void App::render_frame() {
    auto graphics = renderer->acquire_context();
    VkClearColorValue clear {};
    clear.float32[3] = 1.0f; // 黑色，与 LearnVulkan2 一致
    graphics.begin(clear);
    graphics.bind_pipeline(*pipeline);
    graphics.draw(3);
    graphics.end();
}

void App::run() {
    // 基准配置（规格见 e1/bench_spec.md）：AVE_BENCH=1；AVE_BENCH_SECONDS 默认 60；AVE_BENCH_WARMUP 默认 2
    const bool bench = std::getenv("AVE_BENCH") != nullptr;
    const double bench_seconds = env_double("AVE_BENCH_SECONDS", 60.0);
    const double bench_warmup = env_double("AVE_BENCH_WARMUP", 2.0);
    bool measuring = !bench;
    const auto bench_begin = std::chrono::steady_clock::now();

    ave::misc::FPSDetective detective;
    if(!bench) detective.start();

    while (!window->should_close()) {
        window->poll_events();
        window->process_events();

        if(bench) {
            const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - bench_begin).count();
            if(!measuring && t >= bench_warmup) {
                detective.reset();
                detective.start();
                measuring = true;
            }
            if(measuring && t >= bench_warmup + bench_seconds) break;
        }

        render_frame();
        if(measuring) detective.next_frame();
    }

    lg << "\n" << detective.table() << std::endl;
}

} // namespace avetest_tri
