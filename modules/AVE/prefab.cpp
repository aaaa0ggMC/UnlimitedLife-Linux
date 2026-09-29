module;
#include <AVE/config.h>
#include <glm/glm.hpp>
#include <array>

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

} // namespace ave
