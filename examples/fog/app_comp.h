/**
 * @file app_comp.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 组件
 * @version 5.0
 * @date 2026-10-14
 * 
 * @copyright Copyright (c) 2026
 * @parblock audit AI不能修改 
 * 1. OK 整体还行
 * @endparblock
 * 
 */
#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>
#include <string>
#include <vector>
#include <AVE/AVE.h>
#include <AVE/alib.h>
#include <alib5/aecs.h>


namespace fog {

using namespace alib5::ecs;

/// @brief 脏标记（含版本号；移植 AGE Utils.h DirtyMarker 的 dm_version，供下游 lazy 重算缓存）
struct DirtyMarker {
    bool dirty { true };
    std::uint64_t version { 1 };

    void dm_mark() noexcept { ++version; dirty = true; }
    void dm_clear() noexcept { dirty = false; }
    [[nodiscard]] bool dm_check() const noexcept { return dirty; }

    /// @brief 版本变化时更新 cached 并返回 true（调用方据此重算缓存）
    bool dm_version(std::uint64_t& cached) noexcept {
        if(cached == version) return false;
        cached = version;
        return true;
    }
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

    Transform() { reset(); }

    void reset() {
        position = glm::vec3(0.0f);
        scale = glm::vec3(1.0f);
        rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        model_matrix = glm::mat4(1.0f);
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
        const auto next = glm::angleAxis(glm::radians(yaw_deg), glm::vec3(0.0f, 1.0f, 0.0f))
                        * glm::angleAxis(glm::radians(pitch_deg), glm::vec3(1.0f, 0.0f, 0.0f));
        if(next == rotation) return;
        rotation = next;
        dm_mark();
    }

    const glm::mat4& build_model_matrix() {
        if(!dm_check()) return model_matrix;
        dm_clear();
        model_matrix = glm::translate(glm::mat4(1.0f), position)
                     * glm::mat4_cast(rotation)
                     * glm::scale(glm::mat4(1.0f), scale);
        return model_matrix;
    }

    //// 本地轴：直接由 rotation 求值，不再作为矩阵构建的副产物缓存。
    //// 注意必须用 quat * vec3；vec3 * quat 是逆旋转，会使相机移动方向与渲染朝向相反。
    [[nodiscard]] glm::vec3 up() const { return rotation * glm::vec3(0.0f, 1.0f, 0.0f); }
    [[nodiscard]] glm::vec3 left() const { return rotation * glm::vec3(1.0f, 0.0f, 0.0f); }
    [[nodiscard]] glm::vec3 forward() const { return rotation * glm::vec3(0.0f, 0.0f, 1.0f); }

    /// 相机观察方向（看向本地 -Z）
    [[nodiscard]] glm::vec3 view_direction() const { return -forward(); }
};

/// @brief 调试标签
struct Tag {
    std::string tag;
    explicit Tag(std::string_view set_tag) : tag(set_tag) {}
};

/// @brief 视图矩阵（对应 AGE comps::Viewer）：按 Transform 版本懒惰重建
struct Viewer {
    glm::mat4 view_matrix { 1.0f };
    std::uint64_t dm_transform { 0 };

    /// @note 相机 scale 恒为 1，视图矩阵 = model 的逆（AGE 那边写成 R·T(-p)，用的是 R 不是 R⁻¹）
    const glm::mat4& build_view_matrix(Transform& transform) {
        if(transform.dm_version(dm_transform)) {
            view_matrix = glm::inverse(transform.build_model_matrix());
        }
        return view_matrix;
    }
};

/// @brief 投影参数（对应 AGE comps::Projector）：懒惰重建，含 Vulkan 裁剪空间 Y 翻转
struct Projector : DirtyMarker {
    float fov_rad { glm::radians(45.0f) };
    float aspect { 1.0f };
    float near_plane { 0.1f };
    float far_plane { 300.0f };
    glm::mat4 proj_matrix { 1.0f };

    /// 宽高比没变就不标脏，可以每帧无脑调用
    void set_viewport(float width, float height) {
        const float next = (height > 0.0f) ? width / height : 1.0f;
        if(next == aspect) return;
        aspect = next;
        dm_mark();
    }

    const glm::mat4& build_projection_matrix() {
        if(!dm_check()) return proj_matrix;
        dm_clear();
        proj_matrix = glm::perspective(fov_rad, aspect, near_plane, far_plane);
        proj_matrix[1][1] *= -1.0f; // 适配 Vulkan 裁剪空间 Y 轴朝下
        return proj_matrix;
    }
};

/// @brief 相机视角控制状态（挂在相机实体上，由输入系统驱动）
struct CameraTilt {
    float yaw_deg { 30.0f };    ///< 偏航，0 即看向 -Z
    float pitch_deg { -22.0f }; ///< 俯仰，负数俯视
    float sensitivity { 0.065f }; ///< 度/像素，原默认值的 50%

    void add_mouse_delta(double dx, double dy) {
        // 原始鼠标坐标：+X 向右、+Y 向下。相机朝本地 -Z，
        // 正 yaw 向左、正 pitch 向上，因此两个屏幕增量均取反。
        yaw_deg   -= static_cast<float>(dx) * sensitivity;
        pitch_deg -= static_cast<float>(dy) * sensitivity;
        pitch_deg = std::clamp(pitch_deg, -89.0f, 89.0f);
    }
};

/// WASD 沿相机朝向；升降独立沿世界 Y，不受观察矩阵或斜向归一化影响。
inline glm::vec3 camera_move_delta(const Transform& transform, const ave::Input& input, float dt) {
    glm::vec3 move(0.0f);
    if(input.is_key_down(ave::KeyCode::W)) move += transform.view_direction();
    if(input.is_key_down(ave::KeyCode::S)) move -= transform.view_direction();
    if(input.is_key_down(ave::KeyCode::D)) move += transform.left();
    if(input.is_key_down(ave::KeyCode::A)) move -= transform.left();
    if(glm::dot(move, move) > 0.0f) move = glm::normalize(move);
    const bool up = input.is_key_down(ave::KeyCode::Space) || input.is_key_down(ave::KeyCode::E);
    const bool down = input.is_key_down(ave::KeyCode::LeftShift) || input.is_key_down(ave::KeyCode::RightShift)
        || input.is_key_down(ave::KeyCode::Q);
    move.y += static_cast<float>(up) - static_cast<float>(down);
    return move * (6.0f * dt);
}

/// @brief 全局线性雾参数（配置值仅作种子，此后以组件为唯一运行期状态）
struct FogParams : DirtyMarker {
    glm::vec3 color { 0.55f, 0.68f, 0.78f };
    float start { 5.0f };

    FogParams(glm::vec3 i_color, float i_start)
    : color(i_color), start(i_start) {}
};

/// Local-space bounds are [-0.5, 0.5]; Transform::scale is the full world-space size.
struct FogVolume {
    int shape { 0 }; // 0 box, 1 ellipsoid
    float density { 0.15f };
    glm::vec3 color { 0.75f, 0.82f, 0.90f };
    float albedo { 0.90f };
    float edge_fade { 0.20f }; // fraction of the normalized radius, not world units
    bool enabled { true };
};

/// Resource description only; App owns the actual atlas image and sampler.
struct FogDensityTexture {
    std::string path; // empty = uniform, builtin:cloud, or raw R8 volume
    std::array<int, 3> extent { 32, 32, 32 }; // raw file dimensions, X fastest
    glm::vec3 tiling { 1.0f };
    glm::vec3 offset { 0.0f };
    float strength { 1.0f };
    bool repeat { false };
};

inline constexpr unsigned max_fog_control_points = 32;

struct FogControlPoint {
    glm::vec3 position { 0.5f }; // normalized [0,1]^3
    float radius { 0.25f };
    float density { 1.0f };
    float softness { 0.75f }; // outer fraction of the radius
    glm::vec3 amplitude { 0.05f }; // bounded elliptical orbit around position
    float phase { 0.0f }; // radians
    int frequency { 1 }; // integer cycles per field period
    bool operator==(const FogControlPoint&) const = default;
};

/// Overrides FogDensityTexture when enabled; FogVolume still supplies bounds/material.
struct FogControlField {
    bool enabled { true };
    bool animated { false };
    float period { 12.0f };
    float noise_scale { 6.0f };
    float noise_strength { 0.65f };
    int octaves { 3 };
    int seed { 1 };
    std::vector<FogControlPoint> points { FogControlPoint{} };
    double cycle { 0.0 }; // runtime only, [0,1), advanced by the world tick
    bool operator==(const FogControlField&) const = default;
};

struct FogVolumeEntity : EntityWrapper {
    FogVolumeEntity(EntityManager& manager, std::string_view tag,
                    const FogVolume& medium, const FogDensityTexture& texture,
                    glm::vec3 position, glm::vec3 scale,
                    glm::vec3 axis = glm::vec3(0.0f, 1.0f, 0.0f), float angle = 0.0f)
    : EntityWrapper(manager) {
        add<Tag>(tag);
        auto transform = add<Transform>();
        transform->translate(position);
        transform->scale = scale;
        if(angle != 0.0f) transform->rotate(axis, glm::radians(angle));
        transform->dm_mark();
        add<FogVolume>(medium);
        if(!texture.path.empty()) add<FogDensityTexture>(texture);
    }
};

/// @brief 可渲染物：GPU 侧几何切片 + 渲染类别（不含任何 AVE 资源所有权）
struct Renderable {
    ave::VMABufferSlice vertices;
    ave::VMABufferSlice indices;
    std::uint32_t index_count { 0 };
    bool solid { false };
    /// false 时不进入 shadow pass（例如贴着火把光源的灯杆）。
    bool cast_shadow { true };
};

/// 天空不带 Transform：普通物体查询不会重复绘制它。
struct Skybox {
    float exposure { 1.0f };
    bool textured { false };
};

/// 单个太阳方向光，direction 指向光源；阴影资源由 App 管理。
struct DirectionalLight {
    glm::vec3 direction { -0.45f, 0.85f, 0.55f };
    glm::vec3 color { 1.0f, 0.94f, 0.82f };
    float ambient { 0.24f };
    bool specular { true };
};

/// 应用层的简易表面参数，不改变 AVE Shape 的纯几何职责。
struct SurfaceMaterial {
    glm::vec3 color { 1.0f };
    float specular { 0.08f };
    float shininess { 24.0f };
    float emission { 0.0f };
    int voxel { 0 }; // -1=自发光（无表面照明），0=普通，1=草，2=石，3=木，4=叶，5=水
    /// 方块法线图层：0=无，1=草，2=石，3=木，4=叶，5=沙，6=瓦。
    int normal_layer { 0 };
};

/// 只读场景快照：静态 cube 实例进入批次；其它几何单独绘制。
struct StaticInstance {};
struct DirectDraw {};

/**
 * @brief 相机实体包装（AGE age::world::Camera 的最小版）
 * EntityWrapper 长期持有；组件 ref 在构造时缓存，访问器直接返回引用
 */
struct CameraEntity : public EntityWrapper {
    ref_t<Transform> transform_ref {};
    ref_t<CameraTilt> tilt_ref {};
    ref_t<Viewer> viewer_ref {};
    ref_t<Projector> projector_ref {};

    explicit CameraEntity(EntityManager& manager)
    : EntityWrapper(manager) {
        add<Tag>("camera");
        transform_ref = add<Transform>();
        tilt_ref = add<CameraTilt>();
        viewer_ref = add<Viewer>();
        projector_ref = add<Projector>();
        transform_ref->translate(glm::vec3(10.0f, 8.0f, 14.0f));
        transform_ref->set_yaw_pitch(tilt_ref->yaw_deg, tilt_ref->pitch_deg);
    }

    [[nodiscard]] bool is_valid() noexcept { return !is_null(); }

    [[nodiscard]] Transform& transform() { return transform_ref.get(); }
    [[nodiscard]] CameraTilt& tilt() { return tilt_ref.get(); }
    [[nodiscard]] Viewer& viewer() { return viewer_ref.get(); }
    [[nodiscard]] Projector& projection() { return projector_ref.get(); }

    /// 观察矩阵：Transform 未变则直接复用缓存
    const glm::mat4& view_matrix() { return viewer().build_view_matrix(transform()); }
    /// 透视矩阵：投影参数未变则直接复用缓存
    const glm::mat4& projection_matrix() { return projection().build_projection_matrix(); }
};

/// @brief 天空盒实体（场景唯一）：Skybox 参数 + 用于绘制的 Renderable
struct SkyboxEntity : public EntityWrapper {
    ref_t<Skybox> skybox_ref {};
    ref_t<Renderable> renderable_ref {};

    SkyboxEntity(EntityManager& manager, const Skybox& params, const Renderable& renderable)
    : EntityWrapper(manager) {
        add<Tag>("skybox");
        skybox_ref = add<Skybox>(params);
        renderable_ref = add<Renderable>(renderable);
    }

    [[nodiscard]] Skybox& params() { return skybox_ref.get(); }
    [[nodiscard]] Renderable& renderable() { return renderable_ref.get(); }
};

/// @brief 全局雾实体包装（长期持有）
struct FogEntity : public EntityWrapper {
    ref_t<FogParams> params_ref {};

    FogEntity(EntityManager& manager, glm::vec3 color, float start)
    : EntityWrapper(manager) {
        add<Tag>("fog");
        params_ref = add<FogParams>(color, start);
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
        float angle,
        SurfaceMaterial material = {},
        bool batch_cube = false
    ) : EntityWrapper(manager) {
        add<Tag>(tag);
        transform_ref = add<Transform>();
        renderable_ref = add<Renderable>(renderable);
        add<SurfaceMaterial>(material);
        if(batch_cube) add<StaticInstance>();
        else add<DirectDraw>();
        transform_ref->translate(position);
        if(angle != 0.0f) {
            transform_ref->rotate(axis, glm::radians(angle));
        }
    }

    [[nodiscard]] Transform& transform() { return transform_ref.get(); }
    [[nodiscard]] Renderable& renderable() { return renderable_ref.get(); }
};

} // namespace fog
