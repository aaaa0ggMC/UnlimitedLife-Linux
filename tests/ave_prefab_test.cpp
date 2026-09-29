#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

import ave;
import alib6;

static void check(bool value, const char* message) {
    if(!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

///// box/cube /////
static void test_box_cube() {
    auto box = ave::Prefab::box(2.0f, 3.0f, 4.0f);
    check(box.vertex_count() == 24, "box: expect 24 vertices");
    check(box.index_count() == 36, "box: expect 36 indices");
    check(box.indices[0] == 0 && box.indices[1] == 1 && box.indices[2] == 2,
          "box: first triangle mismatch");
    // 半尺寸 1, 1.5, 2
    bool extent_ok = false;
    for(const auto& v : box.vertices) {
        if(std::abs(v.position.x) <= 1.0f + 1e-5f &&
           std::abs(v.position.y) <= 1.5f + 1e-5f &&
           std::abs(v.position.z) <= 2.0f + 1e-5f) {
            extent_ok = true;
        } else {
            extent_ok = false;
            break;
        }
    }
    check(extent_ok, "box: vertex out of extent");

    auto cube = ave::Prefab::cube(1.0f);
    check(cube.vertex_count() == 24 && cube.index_count() == 36, "cube: wrong counts");
    // uv_scale
    auto scaled = ave::Prefab::cube(1.0f, 4.0f);
    bool uv_ok = true;
    for(const auto& v : scaled.vertices) {
        if(v.uv.x < 0.0f || v.uv.x > 4.0f + 1e-5f || v.uv.y < 0.0f || v.uv.y > 4.0f + 1e-5f) {
            uv_ok = false;
            break;
        }
    }
    check(uv_ok, "cube: uv out of [0, uv_scale]");
    check(scaled.vertices[1].uv == glm::vec2(4.0f, 0.0f), "cube: uv mapping mismatch");

    // 退化：全 0 尺寸 → 空
    check(ave::Prefab::box(0.0f, 0.0f, 0.0f).empty(), "box: zero size should be empty");
    check(ave::Prefab::cube(0.0f).empty(), "cube: zero length should be empty");
    check(ave::Prefab::plane(0.0f, 0.0f).empty(), "plane: zero size should be empty");
    check(ave::Prefab::sphere(0).empty(), "sphere: precision 0 should be empty");
    check(ave::Prefab::torus(0, 1.0f, 0.2f).empty(), "torus: precision 0 should be empty");
}

///// plane /////
static void test_plane() {
    auto plane = ave::Prefab::plane(4.0f, 6.0f, 2.0f);
    check(plane.vertex_count() == 4, "plane: expect 4 vertices");
    check(plane.index_count() == 6, "plane: expect 6 indices");
    check(plane.indices[0] == 0 && plane.indices[5] == 3, "plane: index mismatch");
    bool ok = true;
    for(const auto& v : plane.vertices) {
        if(v.position.y != 0.0f || v.normal != glm::vec3(0.0f, 1.0f, 0.0f)) ok = false;
        if(std::abs(v.position.x) > 2.0f + 1e-5f || std::abs(v.position.z) > 3.0f + 1e-5f) ok = false;
    }
    check(ok, "plane: geometry mismatch (y=0, +Y normal, extent)");
    check(plane.vertices[2].uv == glm::vec2(2.0f, 2.0f), "plane: uv mismatch");
}

///// sphere /////
static void test_sphere() {
    const auto radius = 2.0f;
    auto sphere = ave::Prefab::sphere(16, radius);
    check(sphere.vertex_count() == 17 * 17, "sphere: expect (precision+1)^2 vertices");
    check(sphere.index_count() == 16 * 16 * 6, "sphere: expect precision^2*6 indices");

    bool all_ok = true;
    for(const auto& v : sphere.vertices) {
        const float length = glm::length(v.position);
        // 两极处 top_radius = 0，位置退化为极点，长度仍为 radius
        if(std::abs(length - radius) > 1e-4f) { all_ok = false; break; }
        if(std::abs(glm::length(v.normal) - 1.0f) > 1e-4f) { all_ok = false; break; }
        if(v.uv.x < 0.0f || v.uv.x > 1.0f + 1e-5f ||
           v.uv.y < 0.0f || v.uv.y > 1.0f + 1e-5f) { all_ok = false; break; }
    }
    check(all_ok, "sphere: positions/normals/uv out of spec");

    // 索引范围检查
    for(auto idx : sphere.indices) {
        if(idx >= sphere.vertices.size()) { check(false, "sphere: index out of range"); }
    }

    // 默认参数：单位球
    auto unit = ave::Prefab::sphere(8);
    check(unit.vertex_count() == 81, "sphere: default precision count mismatch");
    for(const auto& v : unit.vertices) {
        check(std::abs(glm::length(v.position) - 1.0f) <= 1e-4f, "sphere: not unit radius");
    }
}

///// torus /////
static void test_torus() {
    const float inner = 2.0f;
    const float ring = 0.5f;
    auto torus = ave::Prefab::torus(16, inner, ring);
    check(torus.vertex_count() == 17 * 17, "torus: expect (precision+1)^2 vertices");
    check(torus.index_count() == 16 * 16 * 6, "torus: expect precision^2*6 indices");

    bool all_ok = true;
    for(const auto& v : torus.vertices) {
        const float axis_dist = std::sqrt(v.position.x * v.position.x + v.position.z * v.position.z);
        // 管中心到轴的距离范围 [inner - ring, inner + ring]
        if(axis_dist < inner - ring - 1e-4f || axis_dist > inner + ring + 1e-4f) all_ok = false;
        if(std::abs(v.position.y) > ring + 1e-4f) all_ok = false;
        if(std::abs(glm::length(v.normal) - 1.0f) > 1e-4f) all_ok = false;
    }
    check(all_ok, "torus: geometry/normals out of spec");

    for(auto idx : torus.indices) {
        if(idx >= torus.vertices.size()) { check(false, "torus: index out of range"); }
    }
}

int main() {
    test_box_cube();
    test_plane();
    test_sphere();
    test_torus();

    std::printf("ALL AVE PREFAB TESTS PASSED\n");
    return 0;
}
