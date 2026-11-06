#include "../app_comp.h"
#include <cstdio>
#include <cstdlib>

static void check(bool value, const char* message) {
    if(!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static void check_axes(float yaw, float pitch) {
    fog::Transform transform;
    transform.set_yaw_pitch(yaw, pitch);
    const auto& model = transform.build_model_matrix();
    check(glm::length(transform.left() - glm::vec3(model[0])) < 1e-5f, "right axis must match rendered camera rotation");
    check(glm::length(transform.up() - glm::vec3(model[1])) < 1e-5f, "up axis must match rendered camera rotation");
    check(glm::length(transform.view_direction() + glm::vec3(model[2])) < 1e-5f, "W direction must match rendered camera forward");
    const glm::vec3 in_view = glm::vec3(glm::inverse(model) * glm::vec4(transform.view_direction(), 0.0f));
    check(glm::length(in_view - glm::vec3(0.0f, 0.0f, -1.0f)) < 1e-5f, "W must move along view-space -Z");
}

int main() {
    for(float yaw : {0.0f, 45.0f, -45.0f, 90.0f, -90.0f, 180.0f}) {
        for(float pitch : {0.0f, -12.0f, 30.0f, -30.0f, 89.0f, -89.0f}) check_axes(yaw, pitch);
    }
    fog::CameraTilt tilt;
    check(std::abs(tilt.sensitivity - 0.065f) < 1e-6f, "default sensitivity must be halved");
    tilt.yaw_deg = 0.0f;
    tilt.pitch_deg = 0.0f;
    tilt.add_mouse_delta(100.0, -100.0);
    check(std::abs(tilt.yaw_deg + 6.5f) < 1e-5f && std::abs(tilt.pitch_deg - 6.5f) < 1e-5f,
          "100 pixels must rotate 6.5 degrees");
    check(tilt.yaw_deg < 0.0f && tilt.pitch_deg > 0.0f, "right/up mouse input must decrease yaw/increase pitch");
    fog::Transform camera;
    camera.set_yaw_pitch(tilt.yaw_deg, tilt.pitch_deg);
    check(camera.view_direction().x > 0.0f && camera.view_direction().y > 0.0f, "mouse right/up must turn the camera right/up");
    tilt.add_mouse_delta(0.0, -1e6);
    check(tilt.pitch_deg == 89.0f, "upper pitch clamp");
    tilt.add_mouse_delta(0.0, 1e6);
    check(tilt.pitch_deg == -89.0f, "lower pitch clamp");
    fog::CameraTilt initial;
    camera.set_yaw_pitch(initial.yaw_deg, initial.pitch_deg);
    check(camera.view_direction().y < 0.0f, "initial negative pitch must look downward");
    for(float yaw : {0.0f, 45.0f, 180.0f}) {
        for(float pitch : {-89.0f, 0.0f, 89.0f}) {
            camera.set_yaw_pitch(yaw, pitch);
            ave::Input input;
            input.get_key_info(ave::KeyCode::Space).status = ave::KeyState::Held;
            check(glm::length(fog::camera_move_delta(camera, input, 0.5f) - glm::vec3(0.0f, 3.0f, 0.0f)) < 1e-5f,
                  "Space must ascend along world Y at fixed speed");
            input.get_key_info(ave::KeyCode::LeftShift).status = ave::KeyState::Held;
            check(glm::length(fog::camera_move_delta(camera, input, 0.5f)) < 1e-5f, "Space and Shift must cancel");
            input.get_key_info(ave::KeyCode::Space).status = ave::KeyState::Released;
            check(glm::length(fog::camera_move_delta(camera, input, 0.5f) - glm::vec3(0.0f, -3.0f, 0.0f)) < 1e-5f,
                  "Shift must descend, never sprint or follow camera pitch");
            input.get_key_info(ave::KeyCode::W).status = ave::KeyState::Held;
            const auto combined = fog::camera_move_delta(camera, input, 0.5f);
            input.get_key_info(ave::KeyCode::LeftShift).status = ave::KeyState::Released;
            const auto forward = fog::camera_move_delta(camera, input, 0.5f);
            check(glm::length(combined - forward - glm::vec3(0.0f, -3.0f, 0.0f)) < 1e-5f,
                  "world descent must remain independent when combined with W");
            check(std::abs(glm::length(forward) - 3.0f) < 1e-5f, "W speed must remain unchanged");
            input.get_key_info(ave::KeyCode::W).status = ave::KeyState::Released;
            input.get_key_info(ave::KeyCode::RightShift).status = ave::KeyState::Held;
            check(glm::length(fog::camera_move_delta(camera, input, 0.5f) - glm::vec3(0.0f, -3.0f, 0.0f)) < 1e-5f,
                  "right Shift must also descend");
        }
    }
    fog::EntityManager manager;
    fog::ObjectEntity direct(manager, "direct", {}, glm::vec3(0.0f), glm::vec3(0.0f), 0.0f);
    fog::ObjectEntity batched(manager, "batched", {}, glm::vec3(0.0f), glm::vec3(0.0f), 0.0f, {}, true);
    check(direct.get<fog::DirectDraw>().has_value() && !direct.get<fog::StaticInstance>().has_value(), "new objects must default to direct rendering");
    check(batched.get<fog::StaticInstance>().has_value() && !batched.get<fog::DirectDraw>().has_value(), "batched objects must not also draw directly");

    //// 相机矩阵懒惰缓存：只在 Transform / 视口变化时重算 ////
    fog::CameraEntity cam(manager);
    cam.transform().set_yaw_pitch(35.0f, -20.0f);
    cam.transform().translate(glm::vec3(1.0f, 2.0f, 3.0f));

    const glm::mat4 view_cached = cam.view_matrix();
    const auto unchanged_version = cam.transform().version;
    cam.transform().set_yaw_pitch(35.0f, -20.0f);
    check(cam.transform().version == unchanged_version, "identical camera angles must not dirty the view cache");
    const glm::mat4 view_reference = glm::inverse(cam.transform().build_model_matrix());
    for(int c = 0; c < 4; ++c) {
        for(int r = 0; r < 4; ++r) {
            check(std::abs(view_cached[c][r] - view_reference[c][r]) < 1e-5f,
                  "cached view matrix must equal the inverse of the model matrix");
        }
    }
    cam.transform().translate(glm::vec3(0.0f, 1.0f, 0.0f));
    check(cam.view_matrix() != view_cached, "view matrix must refresh after the transform changes");
    check(cam.view_matrix() == cam.view_matrix(), "view matrix must stay stable while the transform is untouched");

    cam.projection().set_viewport(800.0f, 600.0f);
    const glm::mat4 proj_16_9 = cam.projection_matrix();
    cam.projection().set_viewport(800.0f, 600.0f);
    check(cam.projection_matrix() == proj_16_9, "projection matrix must be reused while the viewport is unchanged");
    cam.projection().set_viewport(1600.0f, 900.0f);
    check(cam.projection_matrix() != proj_16_9, "projection matrix must refresh when the aspect ratio changes");
    check(cam.projection_matrix()[1][1] < 0.0f, "projection must flip Y for Vulkan clip space");

    std::puts("FOG CAMERA TESTS PASSED");
}
