#include "app.h"

//// 相机系统：鼠标视角 + WASD/QE 平移 ////
auto App::update_camera(float dt) -> void {
    auto& tilt = camera.tilt();
    auto& transform = camera.transform();

    // 鼠标视角（度/像素累积到 CameraTilt，再同步为 Transform 朝向）
    const auto [mdx, mdy] = input.get_mouse_delta();
    tilt.add_mouse_delta(mdx, mdy);
    transform.set_yaw_pitch(tilt.yaw_deg, tilt.pitch_deg);
    // 刷新本地轴（观察方向为 -axis_forward，相机的 +X 为 axis_left）
    transform.build_model_matrix();

    const glm::vec3 view_dir = transform.view_direction();
    const glm::vec3 right = transform.axis_left;

    glm::vec3 move(0.0f);
    if(input.is_key_down(ave::KeyCode::W)) move += view_dir;
    if(input.is_key_down(ave::KeyCode::S)) move -= view_dir;
    if(input.is_key_down(ave::KeyCode::D)) move += right;
    if(input.is_key_down(ave::KeyCode::A)) move -= right;
    if(input.is_key_down(ave::KeyCode::E) || input.is_key_down(ave::KeyCode::Space)) move.y += 1.0f;
    if(input.is_key_down(ave::KeyCode::Q)) move.y -= 1.0f;
    if(glm::dot(move, move) > 0.0f) {
        float speed = 6.0f;
        if(input.has_shift()) speed *= 3.0f;
        transform.translate(glm::normalize(move) * speed * dt);
    }
}

//// 雾系统：按键/滚轮实时调节 FogParams ////
auto App::update_fog(float dt) -> void {
    auto& fog_params = fog.params();

    const double sy = input.get_mouse_scroll().second;
    if(sy != 0.0) {
        fog_params.end = std::clamp(
            fog_params.end + static_cast<float>(sy),
            fog_params.start + 0.5f, 200.0f
        );
    }
    if(input.is_key_down(ave::KeyCode::R)) fog_params.end += dt * 5.0f;
    if(input.is_key_down(ave::KeyCode::T)) fog_params.end -= dt * 5.0f;
    if(input.is_key_down(ave::KeyCode::G)) fog_params.start += dt * 5.0f;
    if(input.is_key_down(ave::KeyCode::F)) fog_params.start -= dt * 5.0f;
    fog_params.end = std::max(fog_params.start + 0.5f, fog_params.end);
    fog_params.start = std::clamp(fog_params.start, 0.0f, fog_params.end - 0.5f);
}

//// 渲染系统：view 查询 (Transform, Renderable) 并逐物体绘制 ////
auto App::render_scene() -> void {
    auto& fog_params = fog.params();

    const auto [fb_w, fb_h] = window->get_framebuffer_size();
    const float aspect = (fb_h > 0)
        ? static_cast<float>(fb_w) / static_cast<float>(fb_h)
        : 1.0f;

    // 观察/透视矩阵（GLM_FORCE_DEPTH_ZERO_TO_ONE → Vulkan [0,1] 深度）
    const glm::mat4 view = glm::inverse(camera.transform().build_model_matrix());
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 300.0f);
    proj[1][1] *= -1.0f; // 适配 Vulkan 裁剪空间 Y 轴朝下

    // 上传相机 + 全局雾参数 UBO
    const CameraFogUbo ubo {
        .view = view,
        .proj = proj,
        .fog_color = glm::vec4(fog_params.color, 1.0f),
        .fog_params = glm::vec4(fog_params.start, fog_params.end, 0.0f, 0.0f),
    };
    ubo_buffer->upload(ubo);

    auto graphics = renderer->acquire_context();
    if(!graphics) {
        // 窗口最小化等场景：库内直接返回无效上下文，此处按需重建交换链
        if(graphics.is_out_of_date()) {
            ave::recreate_swapchain_from_window(context, *renderer, *window);
        }
        return;
    }

    // 清屏色 = 雾色：没有几何的区域同样"雾茫茫"
    VkClearColorValue clear {};
    clear.float32[0] = fog_params.color.r;
    clear.float32[1] = fog_params.color.g;
    clear.float32[2] = fog_params.color.b;
    clear.float32[3] = 1.0f;

    graphics.begin(clear);
    graphics.bind_pipeline(*pipeline);
    em.view<fog::Transform, fog::Renderable>().for_each(
        [&](fog::Transform& transform, fog::Renderable& renderable) {
            graphics.bind_descriptor_set(0, renderable.descriptor_set);
            graphics.push_constant(PushConstant{ .model = transform.build_model_matrix() });
            graphics.bind_vertex_buffer(renderable.vertices);
            graphics.bind_index_buffer<std::uint32_t>(renderable.indices);
            graphics.draw_indexed(renderable.index_count);
        }
    );
    graphics.end();
}
