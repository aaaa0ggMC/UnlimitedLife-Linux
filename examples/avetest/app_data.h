#pragma once
#include <array>
#include <cstdint>

#include <glm/glm.hpp>

namespace avetest {

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 uv;
};

/// 位置 → RGB 颜色：把 [-0.5, 0.5]³ 的 8 个角点映射到 RGB 立方体的 8 个端角点。
/// 每个面 4 个独立顶点，携带该面 [0,1]² 的平面 UV，纹理无跨面拉伸/割裂。
inline constexpr auto cube_vertices = []() {
    std::array<Vertex, 24> vertices {};
    constexpr glm::vec3 rgb_offset(0.5f);

    // 顶点顺序 a,b,c,d 对应索引 {a,b,c, c,d,a}，UV 与正面朝向一致
    auto put_face = [&](
        std::size_t base,
        glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d
    ) {
        vertices[base + 0] = { a, a + rgb_offset, glm::vec2(0.0f, 0.0f) };
        vertices[base + 1] = { b, b + rgb_offset, glm::vec2(1.0f, 0.0f) };
        vertices[base + 2] = { c, c + rgb_offset, glm::vec2(1.0f, 1.0f) };
        vertices[base + 3] = { d, d + rgb_offset, glm::vec2(0.0f, 1.0f) };
    };

    put_face(0,  {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}); // 前 (+Z)
    put_face(4,  {-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}); // 后 (-Z)
    put_face(8,  {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f, 0.5f}); // 左 (-X)
    put_face(12, { 0.5f, 0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}); // 右 (+X)
    put_face(16, {-0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f}); // 上 (+Y)
    put_face(20, {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f}); // 下 (-Y)

    return vertices;
}();

/// 12 个三角形的 36 个索引（每面 quad 两个三角形）
inline constexpr std::array<std::uint16_t, 36> cube_indices {{
     0,  1,  2,  2,  3,  0, // 前 (+Z)
     4,  5,  6,  6,  7,  4, // 后 (-Z)
     8,  9, 10, 10, 11,  8, // 左 (-X)
    12, 13, 14, 14, 15, 12, // 右 (+X)
    16, 17, 18, 18, 19, 16, // 上 (+Y)
    20, 21, 22, 22, 23, 20  // 下 (-Y)
}};

} // namespace avetest
