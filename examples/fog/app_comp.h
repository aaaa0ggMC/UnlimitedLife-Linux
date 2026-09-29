#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>
#include <string>

import ave;
import alib6;
import std;

namespace fog {

using namespace alib6::ecs;

/// @brief 脏标记
struct DirtyMarker {
    bool dirty { true };

    void dm_mark() noexcept { dirty = true; }
    void dm_clear() noexcept { dirty = false; }
    [[nodiscard]] bool dm_check() const noexcept { return dirty; }
};

/**
 * @brief 变换组件：position / scale / rotation（四元数）
 * model_matrix 懒惰求值，仅脏时重建（AGE comps::Transform 的裁剪版）
 */
struct Transform : DirtyMarker {
    glm::vec3 position { 0.0f };
    glm::vec3 scale { 1.0f };
    glm::quat rotation { 1.0f, 0.0f, 0.0f, 0.0f };

    glm::mat4 model_matrix { 1.0f };
    glm::vec3 axis_up { 0.0f, 1.0f, 0.0f };
    glm::vec3 axis_left { 1.0f, 0.0f, 0.0f };
    glm::vec3 axis_forward { 0.0f, 0.0f, 1.0f };

    Transform() { reset(); }

    void reset() {
        position = glm::vec3(0.0f);
        scale = glm::vec3(1.0f);
        rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        model_matrix = glm::mat4(1.0f);
        axis_up = glm::vec3(0.0f, 1.0f, 0.0f);
        axis_left = glm::vec3(1.0f, 0.0f, 0.0f);
        axis_forward = glm::vec3(0.0f, 0.0f, 1.0f);
        dm_mark();
    }

    void translate(const glm::vec3& delta) {
        if(delta == glm::vec3(0.0f)) return;
        position += delta;
        dm_mark();
    }

    void rotate(const glm::vec3& axis, float rad) {
        if(axis == glm::vec3(0.0f)) return;
        rotation = glm::normalize(glm::angleAxis(rad, glm::normalize(axis)) * rotation);
        dm_mark();
    }

    /// 由偏航角(yaw)+俯仰角(pitch)（度）设置旋转（相机用；R = Ry(yaw)·Rx(pitch)）
    void set_yaw_pitch(float yaw_deg, float pitch_deg) {
        rotation = glm::angleAxis(glm::radians(yaw_deg), glm::vec3(0.0f, 1.0f, 0.0f))
                 * glm::angleAxis(glm::radians(pitch_deg), glm::vec3(1.0f, 0.0f, 0.0f));
        dm_mark();
    }

    const glm::mat4& build_model_matrix() {
        if(!dm_check()) return model_matrix;
        dm_clear();
        model_matrix = glm::translate(glm::mat4(1.0f), position)
                     * glm::mat4_cast(rotation)
                     * glm::scale(glm::mat4(1.0f), scale);
        axis_up      = glm::vec3(0.0f, 1.0f, 0.0f) * rotation;
        axis_left    = glm::vec3(1.0f, 0.0f, 0.0f) * rotation;
        axis_forward = glm::vec3(0.0f, 0.0f, 1.0f) * rotation;
        return model_matrix;
    }

    /// 相机观察方向（看向本地 -Z）
    [[nodiscard]] glm::vec3 view_direction() const {
        return -axis_forward;
    }
};

/// @brief 调试标签
struct Tag {
    std::string tag;
    explicit Tag(std::string_view set_tag) : tag(set_tag) {}
};

/// @brief 相机视角控制状态（挂在相机实体上，由输入系统驱动）
struct CameraTilt {
    float yaw_deg { 0.0f };    ///< 偏航，0 即看向 -Z
    float pitch_deg { -12.0f }; ///< 俯仰，负数俯视
    float sensitivity { 0.13f }; ///< 度/像素

    void add_mouse_delta(double dx, double dy) {
        yaw_deg   += static_cast<float>(dx) * sensitivity;
        pitch_deg -= static_cast<float>(dy) * sensitivity;
        pitch_deg = std::clamp(pitch_deg, -89.0f, 89.0f);
    }
};

/// @brief 全局线性雾参数（配置值仅作种子，此后以组件为唯一运行期状态）
struct FogParams : DirtyMarker {
    glm::vec3 color { 0.55f, 0.68f, 0.78f };
    float start { 5.0f };
    float end { 25.0f };

    FogParams(glm::vec3 i_color, float i_start, float i_end)
    : color(i_color), start(i_start), end(i_end) {}
};

/// @brief 可渲染物：GPU 侧几何切片 + 描述符集（不含任何 AVE 资源所有权）
struct Renderable {
    ave::VMABufferSlice vertices;
    ave::VMABufferSlice indices;
    VkDescriptorSet descriptor_set { VK_NULL_HANDLE };
    std::uint32_t index_count { 0 };
    bool ground { false };
};

/**
 * @brief 相机实体包装（AGE age::world::Camera 的最小版）
 * EntityWrapper 长期持有；组件 ref 在构造时缓存，访问器直接返回引用
 */
struct CameraEntity : public EntityWrapper {
    ref_t<Transform> transform_ref {};
    ref_t<CameraTilt> tilt_ref {};

    explicit CameraEntity(EntityManager& manager)
    : EntityWrapper(manager) {
        add<Tag>("camera");
        transform_ref = add<Transform>();
        tilt_ref = add<CameraTilt>();
        transform_ref->translate(glm::vec3(0.0f, 1.6f, 10.0f));
        transform_ref->set_yaw_pitch(tilt_ref->yaw_deg, tilt_ref->pitch_deg);
    }

    [[nodiscard]] Transform& transform() { return transform_ref.get(); }
    [[nodiscard]] CameraTilt& tilt() { return tilt_ref.get(); }
};

/// @brief 全局雾实体包装（长期持有）
struct FogEntity : public EntityWrapper {
    ref_t<FogParams> params_ref {};

    FogEntity(EntityManager& manager, glm::vec3 color, float start, float end)
    : EntityWrapper(manager) {
        add<Tag>("fog");
        params_ref = add<FogParams>(color, start, end);
    }

    [[nodiscard]] FogParams& params() { return params_ref.get(); }
};

/// @brief 场景物体实体包装（Transform + Renderable）
struct ObjectEntity : public EntityWrapper {
    ref_t<Transform> transform_ref {};
    ref_t<Renderable> renderable_ref {};

    ObjectEntity(
        EntityManager& manager,
        std::string_view tag,
        const Renderable& renderable,
        glm::vec3 position,
        glm::vec3 axis,
        float angle
    ) : EntityWrapper(manager) {
        add<Tag>(tag);
        transform_ref = add<Transform>();
        renderable_ref = add<Renderable>(renderable);
        transform_ref->translate(position);
        if(angle != 0.0f) {
            transform_ref->rotate(axis, glm::radians(angle));
        }
    }

    [[nodiscard]] Transform& transform() { return transform_ref.get(); }
    [[nodiscard]] Renderable& renderable() { return renderable_ref.get(); }
};

} // namespace fog
