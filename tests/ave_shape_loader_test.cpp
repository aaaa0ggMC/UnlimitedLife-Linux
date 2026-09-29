#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

import ave;
import alib6;

static void check(bool value, const char* message) {
    if(!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

static alib6::Error g_error;
static alib6::ErrorWrapper ew(g_error);

///// OBJ: position + uv + normal 三角形，校验索引/UV/法线 /////
static void test_obj_triangle() {
    const std::string obj =
        "o triangle\n"
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "vt 0.0 0.0\n"
        "vt 1.0 0.0\n"
        "vt 0.0 1.0\n"
        "vn 0.0 0.0 1.0\n"
        "# comment line\n"
        "f 1/1/1 2/2/1 3/3/1\n";

    auto shape = ave::ShapeLoader::from_memory(obj, ave::ShapeFormat::Obj, {}, ew);
    check(shape.has_value(), "obj triangle: load failed");
    check(shape->vertex_count() == 3, "obj triangle: expect 3 vertices");
    check(shape->index_count() == 3, "obj triangle: expect 3 indices");
    check(shape->indices[0] == 0 && shape->indices[1] == 1 && shape->indices[2] == 2,
          "obj triangle: indices mismatch");
    check(shape->vertices[0].position.x == 0.0f && shape->vertices[2].position.y == 1.0f,
          "obj triangle: position mismatch");
    check(shape->vertices[0].uv == glm::vec2(0.0f, 0.0f), "obj triangle: uv0 mismatch");
    check(shape->vertices[2].uv == glm::vec2(0.0f, 1.0f), "obj triangle: uv2 mismatch");
    check(shape->vertices[1].normal == glm::vec3(0.0f, 0.0f, 1.0f), "obj triangle: normal mismatch");
}

///// OBJ: flip_v /////
static void test_obj_flip_v() {
    const std::string obj =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vt 0.25 0.25\nvt 0.75 0.25\nvt 0.25 0.75\n"
        "vn 0 0 1\n"
        "f 1/1/1 2/2/1 3/3/1\n";

    auto shape = ave::ShapeLoader::from_memory(
        obj, ave::ShapeFormat::Obj, { .flip_v = true }, ew);
    check(shape.has_value(), "obj flip_v: load failed");
    check(shape->vertices[0].uv.y == 0.75f, "obj flip_v: uv.y not flipped");
}

///// OBJ: 四边形扇形三角化 + 顶点去重 /////
static void test_obj_quad_and_dedup() {
    const std::string obj =
        "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
        "f 1 2 3 4\n"
        "f 1 2 3 4\n";

    auto shape = ave::ShapeLoader::from_memory(obj, ave::ShapeFormat::Obj, {}, ew);
    check(shape.has_value(), "obj quad: load failed");
    check(shape->vertex_count() == 4, "obj quad: dedup produced extra vertices");
    check(shape->index_count() == 12, "obj quad: expect 2 quads -> 12 indices");
    // 扇形三角化: (0,1,2) (0,2,3)
    check(shape->indices[0] == 0 && shape->indices[1] == 1 && shape->indices[2] == 2,
          "obj quad: first triangle mismatch");
    check(shape->indices[3] == 0 && shape->indices[4] == 2 && shape->indices[5] == 3,
          "obj quad: second triangle mismatch");
}

///// OBJ: 负索引 + v//vn 组合 /////
static void test_obj_negative_indices() {
    const std::string obj =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vn 0 0 1\n"
        "f -3//-1 -2//-1 -1//-1\n";

    auto shape = ave::ShapeLoader::from_memory(obj, ave::ShapeFormat::Obj, {}, ew);
    check(shape.has_value(), "obj negative: load failed");
    check(shape->vertex_count() == 3, "obj negative: expect 3 vertices");
    check(shape->vertices[2].position == glm::vec3(0.0f, 1.0f, 0.0f),
          "obj negative: -1 index not resolved to last position");
    check(shape->vertices[0].normal == glm::vec3(0.0f, 0.0f, 1.0f),
          "obj negative: vn index not resolved");
}

///// STL ASCII /////
static void test_stl_ascii() {
    const std::string stl =
        "solid test\n"
        "facet normal 0 0 1\n"
        " outer loop\n"
        "  vertex 0 0 0\n"
        "  vertex 1 0 0\n"
        "  vertex 0 1 0\n"
        " endloop\n"
        "endfacet\n"
        "endsolid test\n";

    auto shape = ave::ShapeLoader::from_memory(stl, ave::ShapeFormat::Stl, {}, ew);
    check(shape.has_value(), "stl ascii: load failed");
    check(shape->vertex_count() == 3, "stl ascii: expect 3 vertices");
    check(shape->indices.size() == 3, "stl ascii: expect 3 indices");
    for(const auto& v : shape->vertices) {
        check(v.normal == glm::vec3(0.0f, 0.0f, 1.0f), "stl ascii: facet normal not applied");
        check(v.uv == glm::vec2(0.0f, 0.0f), "stl ascii: default uv mismatch");
    }
}

///// STL Binary（内存中构造） /////
static std::string make_binary_stl(std::uint32_t triangle_count, std::uint32_t actual_triangles) {
    std::string data(80, '\0');
    data.append(reinterpret_cast<const char*>(&triangle_count), 4);
    for(std::uint32_t i = 0; i < actual_triangles; ++i) {
        const float normal[3] { 0.0f, 1.0f, 0.0f };              // +Y
        const float vertices[9] {                                  // 3 x (x,y,z)
            0.0f, 0.0f, 0.0f,
            2.0f, 0.0f, 0.0f,
            0.0f, 2.0f, 0.0f,
        };
        data.append(reinterpret_cast<const char*>(normal), sizeof(normal));
        data.append(reinterpret_cast<const char*>(vertices), sizeof(vertices));
        const std::uint16_t attr = 0;
        data.append(reinterpret_cast<const char*>(&attr), sizeof(attr));
    }
    return data;
}

static void test_stl_binary() {
    const auto data = make_binary_stl(1, 1);
    auto shape = ave::ShapeLoader::from_memory(data, ave::ShapeFormat::Stl, {}, ew);
    check(shape.has_value(), "stl binary: load failed");
    check(shape->vertex_count() == 3, "stl binary: expect 3 vertices");
    check(shape->vertices[1].position == glm::vec3(2.0f, 0.0f, 0.0f),
          "stl binary: vertex position mismatch");
    for(const auto& v : shape->vertices) {
        check(v.normal == glm::vec3(0.0f, 1.0f, 0.0f),
              "stl binary: facet normal must be replicated to all 3 vertices");
    }
}

///// STL Binary 截断保护：头部声称 5 个三角形但只有 1 个的数据 /////
static void test_stl_binary_truncated() {
    const auto data = make_binary_stl(5, 1);
    auto shape = ave::ShapeLoader::from_memory(data, ave::ShapeFormat::Stl, {}, ew);
    check(shape.has_value(), "stl truncated: load failed");
    check(shape->vertex_count() == 3,
          "stl truncated: should parse only the 1 complete triangle (no OOB read)");
}

///// 自动识别 /////
static void test_auto_detection() {
    const std::string obj = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    const std::string stl_ascii =
        "solid s\nfacet normal 0 0 1\n outer loop\n  vertex 0 0 0\n"
        "  vertex 1 0 0\n  vertex 0 1 0\n endloop\nendfacet\nendsolid s\n";

    auto obj_shape = ave::ShapeLoader::from_memory(obj, {}, ew);
    check(obj_shape.has_value() && obj_shape->vertex_count() == 3,
          "auto: obj not recognized");

    auto stl_shape = ave::ShapeLoader::from_memory(stl_ascii, {}, ew);
    check(stl_shape.has_value() && stl_shape->vertex_count() == 3,
          "auto: stl not recognized");
}

///// 失败路径 /////
static void test_failure_paths() {
    // 不存在文件
    auto missing = ave::ShapeLoader::from_file("test_data/__no_such_file__.obj", {}, ew);
    check(!missing.has_value(), "failure: missing file should return nullopt");
    check(g_error.has_error(), "failure: no error reported for missing file");

    // 空数据
    auto empty = ave::ShapeLoader::from_memory(std::string_view{}, ave::ShapeFormat::Obj, {}, ew);
    check(!empty.has_value(), "failure: empty data should return nullopt");
}

///// 真实文件：dragon.obj（1.7MB，v//vn + 负索引，AGE 的测试样本同款） /////
static void test_real_dragon_obj() {
    auto shape = ave::ShapeLoader::from_file("assets/test_data/dragon.obj", {}, ew);
    check(shape.has_value(), "dragon.obj: load failed");
    check(shape->vertex_count() > 10000, "dragon.obj: suspiciously few vertices");
    check(shape->index_count() % 3 == 0, "dragon.obj: index count must be a multiple of 3");
    bool has_normal = false;
    for(const auto& v : shape->vertices) {
        if(v.normal != glm::vec3(0.0f)) { has_normal = true; break; }
    }
    check(has_normal, "dragon.obj: no normals parsed");
    std::printf("  dragon.obj: %zu vertices, %zu indices\n",
                shape->vertex_count(), shape->index_count());
}

///// 真实文件：main.model（内容是 OBJ 但后缀不对，显式指定格式） /////
static void test_real_obj_with_other_extension() {
    auto shape = ave::ShapeLoader::from_file(
        "assets/test_data/main.model", ave::ShapeFormat::Obj, {}, ew);
    check(shape.has_value(), "main.model: load failed with explicit Obj format");
    check(shape->vertex_count() > 100, "main.model: suspiciously few vertices");
    std::printf("  main.model: %zu vertices, %zu indices\n",
                shape->vertex_count(), shape->index_count());
}

int main() {
    test_obj_triangle();
    test_obj_flip_v();
    test_obj_quad_and_dedup();
    test_obj_negative_indices();
    test_stl_ascii();
    test_stl_binary();
    test_stl_binary_truncated();
    test_auto_detection();
    test_failure_paths();
    test_real_dragon_obj();
    test_real_obj_with_other_extension();

    std::printf("ALL AVE SHAPE LOADER TESTS PASSED\n");
    return 0;
}
