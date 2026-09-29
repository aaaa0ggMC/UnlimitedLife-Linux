#include <chrono>

#include "app.h"

using enum alib6::Severity;

auto App::run() -> int {
    ave::misc::FPSDetective detective;
    detective.start();

    lg(Info) << "Controls: WASD/QE move, mouse look, Shift sprint, "
             << "wheel/R/T adjust fog end, G/F adjust fog start, ESC quit."
             << std::endl;

    auto last = std::chrono::steady_clock::now();
    while (!window->should_close()) {
        // 1. 事件：poll 驱动 GLFW 回调（写入 Input），new_frame 推进按键生命周期并计算鼠标增量
        ave::Window::poll_events();
        input.new_frame();
        window->process_events(); // 防抖后的 AfterWindowFramebufferResize → swapchain 重建

        if(input.is_key_down(ave::KeyCode::Escape)) break;

        const auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        dt = std::min(dt, 0.1f);

        // 2. 更新相机与运行期雾参数
        update_camera(dt);

        const float yaw_r = glm::radians(cam_yaw);
        const float pitch_r = glm::radians(cam_pitch);
        const glm::vec3 front(
            std::cos(yaw_r) * std::cos(pitch_r),
            std::sin(pitch_r),
            std::sin(yaw_r) * std::cos(pitch_r)
        );

        const auto [fb_w, fb_h] = window->get_framebuffer_size();
        const float aspect = (fb_h > 0)
            ? static_cast<float>(fb_w) / static_cast<float>(fb_h)
            : 1.0f;

        // 观察/透视矩阵（GLM_FORCE_DEPTH_ZERO_TO_ONE → Vulkan [0,1] 深度）
        glm::mat4 view = glm::lookAt(
            cam_pos,
            cam_pos + front,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 300.0f);
        proj[1][1] *= -1.0f; // 适配 Vulkan 裁剪空间 Y 轴朝下

        // 3. 上传相机 + 全局雾参数 UBO
        const CameraFogUbo ubo {
            .view = view,
            .proj = proj,
            .fog_color = glm::vec4(fog_color, 1.0f),
            .fog_params = glm::vec4(fog_start, fog_end, 0.0f, 0.0f),
        };
        ubo_buffer->upload(ubo);

        // 4. 录制并提交一帧（清屏色 = 雾色，无几何处也"雾茫茫"）
        auto graphics = renderer->acquire_context();
        if(graphics) {
            VkClearColorValue clear {};
            clear.float32[0] = fog_color.r;
            clear.float32[1] = fog_color.g;
            clear.float32[2] = fog_color.b;
            clear.float32[3] = 1.0f;

            graphics.begin(clear);
            graphics.bind_pipeline(*pipeline);
            for(const auto& obj : objects) {
                draw_object(graphics, obj);
            }
            graphics.end();
        } else if(graphics.is_out_of_date()) {
            // 窗口最小化等场景：库内直接返回无效上下文，此处按需重建交换链
            ave::recreate_swapchain_from_window(context, *renderer, *window);
        }

        detective.next_frame();
    }

    lg << "\n" << detective.table() << std::endl;
    return 0;
}

auto App::update_camera(float dt) -> void {
    // ---- 鼠标视角（rad/px → deg 累积） ----
    constexpr float mouse_sens = 0.0022f;
    auto [mdx, mdy] = input.get_mouse_delta();
    cam_yaw   += static_cast<float>(mdx) * glm::degrees(mouse_sens);
    cam_pitch -= static_cast<float>(mdy) * glm::degrees(mouse_sens);
    cam_pitch = std::clamp(cam_pitch, -89.0f, 89.0f);

    const float yaw_r = glm::radians(cam_yaw);
    const float pitch_r = glm::radians(cam_pitch);
    const glm::vec3 front(
        std::cos(yaw_r) * std::cos(pitch_r),
        std::sin(pitch_r),
        std::sin(yaw_r) * std::cos(pitch_r)
    );
    const glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

    // ---- WASD/QE 平移 ----
    glm::vec3 move(0.0f);
    if(input.is_key_down(ave::KeyCode::W)) move += front;
    if(input.is_key_down(ave::KeyCode::S)) move -= front;
    if(input.is_key_down(ave::KeyCode::D)) move += right;
    if(input.is_key_down(ave::KeyCode::A)) move -= right;
    if(input.is_key_down(ave::KeyCode::E) || input.is_key_down(ave::KeyCode::Space)) move.y += 1.0f;
    if(input.is_key_down(ave::KeyCode::Q)) move.y -= 1.0f;
    if(glm::dot(move, move) > 0.0f) {
        float speed = 6.0f;
        if(input.has_shift()) speed *= 3.0f;
        cam_pos += glm::normalize(move) * speed * dt;
    }

    // ---- 全局线性雾参数实时调节 ----
    const double sy = input.get_mouse_scroll().second;
    if(sy != 0.0) {
        fog_end = std::clamp(fog_end + static_cast<float>(sy), fog_start + 0.5f, 200.0f);
    }
    if(input.is_key_down(ave::KeyCode::R)) fog_end += dt * 5.0f;
    if(input.is_key_down(ave::KeyCode::T)) fog_end -= dt * 5.0f;
    if(input.is_key_down(ave::KeyCode::G)) fog_start += dt * 5.0f;
    if(input.is_key_down(ave::KeyCode::F)) fog_start -= dt * 5.0f;
    fog_end = std::max(fog_start + 0.5f, fog_end);
    fog_start = std::clamp(fog_start, 0.0f, fog_end - 0.5f);
}

auto App::draw_object(ave::GraphicsContext& graphics, const Object& obj) -> void {
    graphics.bind_descriptor_set(0, obj.ground ? descriptor_ground : descriptor_cubes);
    graphics.push_constant(PushConstant{ .model = obj.model });

    if(obj.ground) {
        graphics.bind_vertex_buffer(ground_vertices);
        graphics.bind_index_buffer<std::uint16_t>(ground_indices);
        graphics.draw_indexed(6);
    } else {
        graphics.bind_vertex_buffer(cube_vertices);
        graphics.bind_index_buffer<std::uint16_t>(cube_indices);
        graphics.draw_indexed(36);
    }
}
