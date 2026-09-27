#pragma once
#include <array>
#include <cstdint>

#include <glm/glm.hpp>

namespace avetest {

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
};

/// 位置 → RGB 颜色：把 [-0.5, 0.5]³ 的 8 个角点映射到 RGB 立方体的 8 个端角点。
/// 颜色由位置直接推导，任何共享棱边的两个面在接缝处颜色天然连续。
inline constexpr Vertex cube_vertex(float x, float y, float z) {
    return Vertex{ glm::vec3(x, y, z), glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f) };
}

/// 立方体的 8 个顶点（索引绘制下无需按面重复顶点）
inline constexpr std::array<Vertex, 8> cube_vertices {{
    cube_vertex(-0.5f, -0.5f,  0.5f), // 0
    cube_vertex( 0.5f, -0.5f,  0.5f), // 1
    cube_vertex( 0.5f,  0.5f,  0.5f), // 2
    cube_vertex(-0.5f,  0.5f,  0.5f), // 3
    cube_vertex(-0.5f, -0.5f, -0.5f), // 4
    cube_vertex( 0.5f, -0.5f, -0.5f), // 5
    cube_vertex( 0.5f,  0.5f, -0.5f), // 6
    cube_vertex(-0.5f,  0.5f, -0.5f), // 7
}};

/// 12 个三角形的 36 个索引（每面两个三角形）
inline constexpr std::array<std::uint16_t, 36> cube_indices {{
     0,  1,  2,  2,  3,  0, // 前 (+Z)
     4,  7,  6,  6,  5,  4, // 后 (-Z)
     3,  7,  4,  4,  0,  3, // 左 (-X)
     2,  1,  5,  5,  6,  2, // 右 (+X)
     7,  3,  2,  2,  6,  7, // 上 (+Y)
     4,  5,  1,  1,  0,  4  // 下 (-Y)
}};

} // namespace avetest
