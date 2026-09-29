module;
#include <AVE/config.h>
#include <glm/glm.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

module ave.shape;

import std;
import alib6;
import :prefab;
import :shape;

namespace ave {

namespace {

    using Vertex = Shape::Vertex;

    // 六面顺序：前(+Z) 后(-Z) 左(-X) 右(+X) 下(-Y) 上(+Y)
    // 每面 4 角点顺序：左下、右下、右上、左上（正面朝外看）
    struct Face {
        glm::vec3 normal;
        std::array<glm::vec3, 4> corners;
    };

} // namespace

Shape Prefab::box(float width, float height, float depth, float uv_scale) {
    if(width == 0.0f && height == 0.0f && depth == 0.0f) {
        return {};
    }

    Shape shape;
    shape.vertices.reserve(24);
    shape.indices.reserve(36);

    const float x = width / 2.0f;
    const float y = height / 2.0f;
    const float z = depth / 2.0f;

    const std::array<Face, 6> faces {{
        { { 0.0f,  0.0f,  1.0f }, {{ {-x, -y,  z}, { x, -y,  z}, { x,  y,  z}, {-x,  y,  z} }} },
        { { 0.0f,  0.0f, -1.0f }, {{ { x, -y, -z}, {-x, -y, -z}, {-x,  y, -z}, { x,  y, -z} }} },
        { {-1.0f,  0.0f,  0.0f }, {{ {-x, -y, -z}, {-x, -y,  z}, {-x,  y,  z}, {-x,  y, -z} }} },
        { { 1.0f,  0.0f,  0.0f }, {{ { x, -y,  z}, { x, -y, -z}, { x,  y, -z}, { x,  y,  z} }} },
        { { 0.0f, -1.0f,  0.0f }, {{ {-x, -y, -z}, { x, -y, -z}, { x, -y,  z}, {-x, -y,  z} }} },
        { { 0.0f,  1.0f,  0.0f }, {{ {-x,  y,  z}, { x,  y,  z}, { x,  y, -z}, {-x,  y, -z} }} },
    }};

    // 每面 UV：左下、右下、右上、左上
    const std::array<glm::vec2, 4> face_uv {{
        { 0.0f,     0.0f     },
        { uv_scale, 0.0f     },
        { uv_scale, uv_scale },
        { 0.0f,     uv_scale },
    }};

    for(std::size_t f = 0; f < faces.size(); ++f) {
        const auto base = static_cast<std::uint32_t>(f * 4);
        for(std::size_t v = 0; v < 4; ++v) {
            shape.vertices.push_back(Vertex{
                .position = faces[f].corners[v],
                .normal = faces[f].normal,
                .uv = face_uv[v],
            });
        }
        shape.indices.push_back(base + 0);
        shape.indices.push_back(base + 1);
        shape.indices.push_back(base + 2);
        shape.indices.push_back(base + 0);
        shape.indices.push_back(base + 2);
        shape.indices.push_back(base + 3);
    }

    return shape;
}

Shape Prefab::cube(float length, float uv_scale) {
    return box(length, length, length, uv_scale);
}

Shape Prefab::plane(float width, float depth, float uv_scale) {
    if(width == 0.0f && depth == 0.0f) {
        return {};
    }

    Shape shape;
    shape.vertices.reserve(4);
    shape.indices.reserve(6);

    const float x = width / 2.0f;
    const float z = depth / 2.0f;
    constexpr glm::vec3 up(0.0f, 1.0f, 0.0f);

    shape.vertices.push_back(Vertex{ .position = {-x, 0.0f, -z}, .normal = up, .uv = { 0.0f,     0.0f     } });
    shape.vertices.push_back(Vertex{ .position = { x, 0.0f, -z}, .normal = up, .uv = { uv_scale, 0.0f     } });
    shape.vertices.push_back(Vertex{ .position = { x, 0.0f,  z}, .normal = up, .uv = { uv_scale, uv_scale } });
    shape.vertices.push_back(Vertex{ .position = {-x, 0.0f,  z}, .normal = up, .uv = { 0.0f,     uv_scale } });

    shape.indices = { 0, 1, 2, 0, 2, 3 };

    return shape;
}

Shape Prefab::sphere(std::uint32_t precision, float radius, float uv_scale) {
    if(precision < 1) {
        return {};
    }

    Shape shape;
    const auto rings = static_cast<std::size_t>(precision) + 1;      // 纬度环（含南北极）
    const auto segments = static_cast<std::size_t>(precision) + 1;   // 经度分段（含接缝）
    shape.vertices.reserve(rings * segments);
    shape.indices.reserve(static_cast<std::size_t>(precision) * precision * 6);

    const float pi = std::numbers::pi_v<float>;

    for(std::uint32_t i = 0; i <= precision; ++i) {
        const float theta = static_cast<float>(i) / precision * pi; // 0..π（-Y → +Y）
        const float y = -std::cos(theta);
        const float top_radius = std::sin(theta);

        for(std::uint32_t j = 0; j <= precision; ++j) {
            const float omega = static_cast<float>(j) / precision * 2.0f * pi;
            const glm::vec3 position(
                -top_radius * std::cos(omega),
                y,
                 top_radius * std::sin(omega)
            );

            shape.vertices.push_back(Vertex{
                .position = position * radius,
                .normal = position, // 单位球上位置即法线
                .uv = glm::vec2(static_cast<float>(j) / precision,
                                static_cast<float>(i) / precision) * uv_scale,
            });

            if(i != precision && j != precision) {
                const auto row = static_cast<std::uint32_t>(i * segments + j);
                const auto next_row = row + static_cast<std::uint32_t>(segments);
                shape.indices.push_back(row);
                shape.indices.push_back(row + 1);
                shape.indices.push_back(next_row);
                shape.indices.push_back(row + 1);
                shape.indices.push_back(next_row + 1);
                shape.indices.push_back(next_row);
            }
        }
    }

    return shape;
}

Shape Prefab::torus(
    std::uint32_t precision,
    float inner_radius,
    float ring_radius,
    float uv_scale
){
    if(precision < 1) {
        return {};
    }

    Shape shape;
    const auto rings = static_cast<std::size_t>(precision) + 1;      // 大环方向（绕 Y）
    const auto segments = static_cast<std::size_t>(precision) + 1;   // 管截面方向
    shape.vertices.reserve(rings * segments);
    shape.indices.reserve(static_cast<std::size_t>(precision) * precision * 6);

    const float pi = std::numbers::pi_v<float>;
    const float two_pi = 2.0f * pi;

    for(std::uint32_t i = 0; i <= precision; ++i) {
        const float theta = static_cast<float>(i) / precision * two_pi; // 大环角
        const float cos_theta = std::cos(theta);
        const float sin_theta = std::sin(theta);

        for(std::uint32_t j = 0; j <= precision; ++j) {
            const float phi = static_cast<float>(j) / precision * two_pi; // 管截面角
            const float cos_phi = std::cos(phi);
            const float sin_phi = std::sin(phi);

            const float major = inner_radius + ring_radius * cos_phi;

            shape.vertices.push_back(Vertex{
                .position = glm::vec3(
                    major * cos_theta,
                    ring_radius * sin_phi,
                    major * sin_theta
                ),
                .normal = glm::normalize(glm::vec3(
                    cos_phi * cos_theta,
                    sin_phi,
                    cos_phi * sin_theta
                )),
                .uv = glm::vec2(static_cast<float>(j) / precision,
                                static_cast<float>(i) / precision) * uv_scale,
            });

            if(i < precision && j < precision) {
                const auto base = static_cast<std::uint32_t>(i * segments + j);
                const auto stride = static_cast<std::uint32_t>(segments);
                shape.indices.push_back(base);
                shape.indices.push_back(base + 1);
                shape.indices.push_back(base + stride);
                shape.indices.push_back(base + 1);
                shape.indices.push_back(base + stride + 1);
                shape.indices.push_back(base + stride);
            }
        }
    }

    return shape;
}

} // namespace ave
