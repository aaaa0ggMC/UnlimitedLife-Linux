module;
#include <AVE/config.h>
#include <glm/glm.hpp>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>

module ave.shape;

import std;
import alib6;
import ave.ecode;
import :loader;
import :shape;

namespace ave {

namespace {

    /// OBJ 面顶点引用：v / vt / vn 索引（-1 表示缺失）
    struct FaceRef {
        std::int32_t v  { -1 };
        std::int32_t vt { -1 };
        std::int32_t vn { -1 };

        friend bool operator==(const FaceRef&, const FaceRef&) = default;
    };

    struct FaceRefHash {
        [[nodiscard]] std::size_t operator()(const FaceRef& r) const noexcept {
            // FNV-1a 逐字段混合，避免引入 <functional> 之外的依赖
            std::size_t h = 1469598103934665603ULL;
            const auto mix = [&h](std::int32_t x) {
                h ^= static_cast<std::size_t>(static_cast<std::uint32_t>(x));
                h *= 1099511628211ULL;
            };
            mix(r.v);
            mix(r.vt);
            mix(r.vn);
            return h;
        }
    };

    /// 从零终止缓冲解析至多 N 个 float；不足补 0 并返回实际解析个数
    template<std::size_t N>
    [[nodiscard]] std::array<float, N> parse_floats(std::string_view text) {
        std::array<float, N> out {};
        std::string buffer(text); // 保证 '\0' 结尾，解析绝不越界
        char* p = buffer.data();
        char* end = nullptr;
        for(std::size_t i = 0; i < N; ++i) {
            const float value = std::strtof(p, &end);
            if(end == p) break; // 没有更多数字
            out[i] = value;
            p = end;
        }
        return out;
    }

    /// 解析 OBJ 面顶点令牌："v" / "v/vt" / "v/vt/vn" / "v//vn"（索引为 0 视为缺失）
    [[nodiscard]] FaceRef parse_face_token(std::string_view token) {
        std::string buffer(token);
        char* p = buffer.data();
        char* end = nullptr;

        const auto next_int = [&p, &end]() -> std::int32_t {
            if(!p || !*p) {
                p = nullptr;
                return 0;
            }
            const long value = std::strtol(p, &end, 10);
            if(end == p) { // 没有数字
                p = nullptr;
                return 0;
            }
            p = end;
            return static_cast<std::int32_t>(value);
        };

        FaceRef ref;
        ref.v = next_int();
        if(p && *p == '/') {
            ++p;
            if(p && *p == '/') {
                ++p;
                ref.vn = next_int();
            } else {
                ref.vt = next_int();
                if(p && *p == '/') {
                    ++p;
                    ref.vn = next_int();
                }
            }
        }

        // OBJ 索引从 1 开始；0/缺失一律归一为 -1
        if(ref.v  == 0) ref.v  = -1;
        if(ref.vt == 0) ref.vt = -1;
        if(ref.vn == 0) ref.vn = -1;
        return ref;
    }

    /// 去除前导空白后判断是否以 "solid" 开头（AGE 的识别规则）
    [[nodiscard]] bool starts_with_solid(std::string_view data) {
        std::size_t i = 0;
        while(i < data.size() && std::isspace(static_cast<unsigned char>(data[i]))) {
            ++i;
        }
        return data.substr(i, 5) == "solid";
    }

    /// 按行切分并对每行回调（不含换行符）
    template<class Func>
    void for_each_line(std::string_view data, Func&& func) {
        std::size_t pos = 0;
        while(pos < data.size()) {
            std::size_t end = data.find('\n', pos);
            if(end == std::string_view::npos) end = data.size();
            func(data.substr(pos, end - pos));
            pos = end + 1;
        }
    }

    /// OBJ：把相对索引（负数从末尾计数）转为 0 基索引
    [[nodiscard]] std::int32_t fix_index(std::int32_t index, std::size_t count) {
        if(index > 0) return index - 1;
        if(index < 0) return static_cast<std::int32_t>(count) + index;
        return -1;
    }

    /// 小端修正（AGE endian_cast 的等价实现）
    template<class T>
    [[nodiscard]] T endian_cast(const T& value) {
        if constexpr(std::endian::native == std::endian::little) {
            return value;
        } else {
            return std::bit_cast<T>(
                std::byteswap(std::bit_cast<std::uint32_t>(value))
            );
        }
    }

    /// OBJ 解析
    Shape parse_obj(std::string_view data, const ShapeLoadOptions& options) {
        Shape shape;
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> uvs;
        std::unordered_map<FaceRef, std::uint32_t, FaceRefHash> cache;

        const auto emit_vertex = [&](FaceRef ref) -> std::uint32_t {
            ref.v = fix_index(ref.v, positions.size());
            ref.vt = fix_index(ref.vt, uvs.size());
            ref.vn = fix_index(ref.vn, normals.size());

            if(const auto it = cache.find(ref); it != cache.end()) {
                return it->second;
            }

            Shape::Vertex vertex {};
            if(ref.v >= 0)  vertex.position = positions[static_cast<std::size_t>(ref.v)];
            if(ref.vt >= 0) {
                vertex.uv = uvs[static_cast<std::size_t>(ref.vt)];
                if(options.flip_v) vertex.uv.y = 1.0f - vertex.uv.y;
            }
            if(ref.vn >= 0) vertex.normal = normals[static_cast<std::size_t>(ref.vn)];

            const auto id = static_cast<std::uint32_t>(shape.vertices.size());
            shape.vertices.push_back(vertex);
            cache.emplace(ref, id);
            return id;
        };

        for_each_line(data, [&](std::string_view line) {
            if(line.empty() || line.front() == '#') return;

            if(line.starts_with("v ")) {
                const auto xyz = parse_floats<3>(line.substr(2));
                positions.emplace_back(xyz[0], xyz[1], xyz[2]);
            } else if(line.starts_with("vn ")) {
                const auto xyz = parse_floats<3>(line.substr(3));
                normals.emplace_back(xyz[0], xyz[1], xyz[2]);
            } else if(line.starts_with("vt ")) {
                const auto xy = parse_floats<2>(line.substr(3));
                uvs.emplace_back(xy[0], xy[1]);
            } else if(line.starts_with("f ")) {
                // 令牌化 + 扇形三角化（支持 3 个以上顶点的多边形）
                std::vector<FaceRef> refs;
                std::string_view rest = line.substr(2);
                while(!rest.empty()) {
                    const auto space = rest.find(' ');
                    auto token = rest.substr(0, space);
                    while(!token.empty() &&
                          std::isspace(static_cast<unsigned char>(token.front()))) {
                        token.remove_prefix(1);
                    }
                    if(!token.empty()) refs.push_back(parse_face_token(token));
                    if(space == std::string_view::npos) break;
                    rest = rest.substr(space + 1);
                }
                if(refs.size() < 3) return;
                for(std::size_t i = 2; i < refs.size(); ++i) {
                    shape.indices.push_back(emit_vertex(refs[0]));
                    shape.indices.push_back(emit_vertex(refs[i - 1]));
                    shape.indices.push_back(emit_vertex(refs[i]));
                }
            }
            // 其余指令（mtllib / usemtl / s / o / g ...）对纯几何无意义，忽略
        });

        return shape;
    }

    /// STL Binary 解析（含长度与三角形数边界保护）
    Shape parse_stl_binary(std::string_view data, const ShapeLoadOptions& options) {
        Shape shape;
        if(data.size() < 84) return shape; // 连头部都不足：空 Shape

        const auto* base = reinterpret_cast<const std::byte*>(data.data());
        auto count = endian_cast(*reinterpret_cast<const std::uint32_t*>(base + 80));
        // 按实际可用数据截断（AGE 直接信任头部计数，越界即 UB）
        constexpr std::size_t triangle_bytes = 50; // 12B 法线 + 36B 顶点 + 2B 属性
        const std::size_t available = (data.size() - 84) / triangle_bytes;
        if(static_cast<std::size_t>(count) > available) {
            count = static_cast<std::uint32_t>(available);
        }

        shape.vertices.reserve(static_cast<std::size_t>(count) * 3);
        shape.indices.reserve(static_cast<std::size_t>(count) * 3);

        const std::byte* p = base + 84;
        for(std::uint32_t i = 0; i < count; ++i) {
            glm::vec3 normal(0.0f);
            for(int c = 0; c < 3; ++c) {
                normal[c] = endian_cast(*reinterpret_cast<const float*>(p));
                p += sizeof(float);
            }
            // STL 无 UV：填充 (0, flipV ? 1 : 0)
            const glm::vec2 uv(0.0f, options.flip_v ? 1.0f : 0.0f);
            const auto base_index = static_cast<std::uint32_t>(shape.vertices.size());
            for(int j = 0; j < 3; ++j) {
                glm::vec3 position(0.0f);
                for(int c = 0; c < 3; ++c) {
                    position[c] = endian_cast(*reinterpret_cast<const float*>(p));
                    p += sizeof(float);
                }
                // facet 法线复制到全部 3 个顶点（修正 AGE 法线数组错位问题）
                shape.vertices.push_back(Shape::Vertex{
                    .position = position,
                    .normal = normal,
                    .uv = uv,
                });
                shape.indices.push_back(base_index + static_cast<std::uint32_t>(j));
            }
            p += 2; // 跳过属性字节计数
        }
        return shape;
    }

    /// STL ASCII 解析（行式状态机）
    Shape parse_stl_ascii(std::string_view data, const ShapeLoadOptions& options) {
        Shape shape;
        std::array<float, 3> normal { 0.0f, 0.0f, 0.0f };
        bool have_normal = false;
        std::array<glm::vec3, 3> triangle {};
        int triangle_verts = 0;

        const auto flush_triangle = [&]() {
            if(triangle_verts < 3 || !have_normal) return;
            const auto base_index = static_cast<std::uint32_t>(shape.vertices.size());
            const glm::vec2 uv(0.0f, options.flip_v ? 1.0f : 0.0f);
            for(int j = 0; j < 3; ++j) {
                shape.vertices.push_back(Shape::Vertex{
                    .position = triangle[static_cast<std::size_t>(j)],
                    .normal = glm::vec3(normal[0], normal[1], normal[2]),
                    .uv = uv,
                });
                shape.indices.push_back(base_index + static_cast<std::uint32_t>(j));
            }
            triangle_verts = 0;
            have_normal = false;
        };

        for_each_line(data, [&](std::string_view line) {
            // 去前导空白
            while(!line.empty() &&
                  std::isspace(static_cast<unsigned char>(line.front()))) {
                line.remove_prefix(1);
            }
            if(line.empty()) return;

            if(line.starts_with("facet normal")) {
                const auto xyz = parse_floats<3>(line.substr(12));
                normal = { xyz[0], xyz[1], xyz[2] };
                have_normal = true;
            } else if(line.starts_with("vertex")) {
                const auto xyz = parse_floats<3>(line.substr(6));
                if(triangle_verts < 3) {
                    triangle[static_cast<std::size_t>(triangle_verts)] =
                        glm::vec3(xyz[0], xyz[1], xyz[2]);
                }
                ++triangle_verts;
                if(triangle_verts == 3) {
                    flush_triangle();
                } else if(triangle_verts > 3) { // 多于 3 个 vertex 的畸形 facet：丢弃
                    triangle_verts = 0;
                    have_normal = false;
                }
            } else if(line.starts_with("endsolid")) {
                flush_triangle();
            }
        });

        return shape;
    }

} // namespace

Shape ShapeLoader::parse(
    std::string_view data,
    ShapeFormat format,
    ShapeLoadOptions options
){
    if(format == ShapeFormat::Obj) {
        return parse_obj(data, options);
    }
    // STL：ASCII 与 Binary 自动分流（与 AGE 相同的 solid 前缀启发式）
    if(starts_with_solid(data)) {
        return parse_stl_ascii(data, options);
    }
    return parse_stl_binary(data, options);
}

std::optional<Shape> ShapeLoader::from_memory(
    std::string_view data,
    ShapeFormat format,
    ShapeLoadOptions options,
    alib6::ErrorWrapper ew
){
    if(data.empty()) {
        ew.report(ave_shape_load_failed,
            "ShapeLoader: input data is empty (format={}).", to_string(format));
        return std::nullopt;
    }
    return parse(data, format, options);
}

std::optional<Shape> ShapeLoader::from_memory(
    std::string_view data,
    ShapeLoadOptions options,
    alib6::ErrorWrapper ew
){
    if(data.empty()) {
        ew.report(ave_shape_load_failed,
            "ShapeLoader: input data is empty (auto detection).");
        return std::nullopt;
    }
    const auto format = starts_with_solid(data) ? ShapeFormat::Stl : ShapeFormat::Obj;
    return parse(data, format, options);
}

std::optional<Shape> ShapeLoader::from_file(
    std::string_view path,
    ShapeFormat format,
    ShapeLoadOptions options,
    alib6::ErrorWrapper ew
){
    std::string data;
    const auto size = alib6::io::read_all(path, data, ew);
    if(size == std::numeric_limits<std::size_t>::max()) {
        // read_all 内部已经过 ew 上报过一次，这里只补充上下文
        ew.report(ave_shape_load_failed,
            "ShapeLoader: failed to read '{}' (format={}).", path, to_string(format));
        return std::nullopt;
    }
    return from_memory(data, format, options, ew);
}

std::optional<Shape> ShapeLoader::from_file(
    std::string_view path,
    ShapeLoadOptions options,
    alib6::ErrorWrapper ew
){
    std::string data;
    const auto size = alib6::io::read_all(path, data, ew);
    if(size == std::numeric_limits<std::size_t>::max()) {
        ew.report(ave_shape_load_failed,
            "ShapeLoader: failed to read '{}' (auto detection).", path);
        return std::nullopt;
    }
    return from_memory(data, options, ew);
}

} // namespace ave
