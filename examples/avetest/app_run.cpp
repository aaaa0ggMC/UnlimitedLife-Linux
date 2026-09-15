#include "app.h"

namespace avetest {

void App::run() {
    RenderCache rc {
        .pipeline = *pipeline,
        .vertex_count = vertex_count
    };

    ave::misc::FPSDetective detective;
    detective.start();

    float angle = 0.0f;
    while (!window->should_close()) {
        window->poll_events();
        window->process_events();

        angle += 0.001f;

        const auto extent = window->get_framebuffer_size();
        float aspect = (extent.second > 0)
            ? (static_cast<float>(extent.first) / static_cast<float>(extent.second))
            : (800.0f / 600.0f);

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

        PushConstant pc {
            .mvp = proj * view * model
        };

        render_frame(rc, pc);
        detective.next_frame();
    }

    lg << "\n" << detective.table() << std::endl;
}

} // namespace avetest
