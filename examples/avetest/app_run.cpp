#include "app.h"

namespace avetest {

namespace {
    double env_double(const char* name, double fallback) {
        const char* v = std::getenv(name);
        return (v && *v) ? std::atof(v) : fallback;
    }
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

    float angle = 0.0f;
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

        angle += 0.001f;

        const auto extent = window->get_framebuffer_size();
        const float aspect = static_cast<float>(extent.first)
                           / static_cast<float>(extent.second);

        // 模型矩阵：绕着斜对角轴 3D 复合旋转
        glm::mat4 model = glm::rotate(
            glm::mat4(1.0f),
            angle,
            glm::normalize(glm::vec3(1.0f, 1.2f, 0.5f))
        );

        // 观察矩阵：摄像机位于 (0.0, 1.8, 2.8) 俯视原点
        glm::mat4 view = glm::lookAt(
            glm::vec3(0.0f, 1.8f, 2.8f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

        // 透视投影矩阵 (因 GLM_FORCE_DEPTH_ZERO_TO_ONE，自动为 Vulkan [0, 1] 深度)
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
        proj[1][1] *= -1.0f; // 适配 Vulkan 裁剪空间 Y 轴朝下

        CameraUbo camera_ubo {
            .vp = proj * view
        };
        ubo_buffer->upload(camera_ubo);

        PushConstant pc {
            .model = model
        };

        render_frame(pc);
        if(measuring) detective.next_frame();
    }

    lg << "\n" << detective.table() << std::endl;
}

} // namespace avetest