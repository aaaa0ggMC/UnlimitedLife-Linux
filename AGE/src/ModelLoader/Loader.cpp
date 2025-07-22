#include <AGE/ModelLoader/Loader.h>
#include <sstream>
#include <vector>
#include <glm/glm.hpp>
#include <alib-g3/autil.h>
#include <unordered_set>

using namespace age::model::fmt;
using namespace age::model;
using namespace age;

void Stl::parse(std::string_view data,ModelData & md){
    
}


struct Verts{
    int vert;
    int coord;
    int normal;
    
    bool operator ==(const Verts & other) const {
        return vert == other.vert && coord == other.coord && normal == other.normal;
    }
};

namespace std{
    template <>
    struct hash<Verts> {
        size_t operator()(const Verts & v) const {
            size_t h1 = std::hash<int>()(v.vert);
            size_t h2 = std::hash<int>()(v.coord);
            size_t h3 = std::hash<int>()(v.normal);
            return ((h1 ^ (h2 << 1)) >> 1) ^ (h3 << 1);
        }
    };
}

void Obj::parse(std::string_view data, ModelData& md) {
    std::vector<glm::vec3> vertDatas;
    std::vector<glm::vec3> normalDatas;
    std::vector<glm::vec2> coordDatas;
    std::unordered_map<Verts, int> vertSet;
    int diffIndex = 0;

    // 负索引转换函数
    auto fixIndex = [](int idx, int size) -> int {
        if (idx > 0) return idx - 1;
        else if (idx < 0) return size + idx;
        return -1;
    };

    // 构造唯一顶点索引的函数
    auto vert_auto = [&](int vIndex, int vtIndex, int vnIndex) -> int {
        vIndex = fixIndex(vIndex, (int)vertDatas.size());
        vtIndex = fixIndex(vtIndex, (int)coordDatas.size());
        vnIndex = fixIndex(vnIndex, (int)normalDatas.size());

        Verts v{vIndex, vtIndex, vnIndex};
        auto it = vertSet.find(v);
        if (it != vertSet.end()) {
            return it->second;
        }

        // 新顶点，加入
        int ret = diffIndex++;

        if (vIndex >= 0) {
            md.vertices.push_back(vertDatas[vIndex].x);
            md.vertices.push_back(vertDatas[vIndex].y);
            md.vertices.push_back(vertDatas[vIndex].z);
        } else {
            md.vertices.push_back(0.0f);
            md.vertices.push_back(0.0f);
            md.vertices.push_back(0.0f);
        }

        if (vtIndex >= 0) {
            md.coords.push_back(coordDatas[vtIndex].x);
            md.coords.push_back(1.0f - coordDatas[vtIndex].y); // 翻转V分量
        } else {
            md.coords.push_back(0.0f);
            md.coords.push_back(0.0f);
        }

        if (vnIndex >= 0) {
            md.normals.push_back(normalDatas[vnIndex].x);
            md.normals.push_back(normalDatas[vnIndex].y);
            md.normals.push_back(normalDatas[vnIndex].z);
        } else {
            md.normals.push_back(0.0f);
            md.normals.push_back(0.0f);
            md.normals.push_back(0.0f);
        }

        vertSet[v] = ret;
        return ret;
    };

    size_t pos = 0, len = data.size();
    while (pos < len) {
        size_t end = data.find('\n', pos);
        if (end == std::string_view::npos) end = len;
        std::string_view line = data.substr(pos, end - pos);
        pos = end + 1;

        if (line.empty() || line[0] == '#') continue;
        else if (line.starts_with("v ")) {
            glm::vec3& v = vertDatas.emplace_back();
            sscanf(line.data(), "v %f %f %f", &v.x, &v.y, &v.z);
        } else if (line.starts_with("vn")) {
            glm::vec3& n = normalDatas.emplace_back();
            sscanf(line.data(), "vn %f %f %f", &n.x, &n.y, &n.z);
        } else if (line.starts_with("vt")) {
            glm::vec2& uv = coordDatas.emplace_back();
            // 注意有些obj只有u，没有v，默认0
            int ret = sscanf(line.data(), "vt %f %f", &uv.x, &uv.y);
            if (ret == 1) uv.y = 0.0f;
        } else if (line.starts_with("f ")) {
            // 解析面
            struct FaceVertex {
                int v = -1, vt = -1, vn = -1;
            };
            FaceVertex faceVerts[4];
            int vertCount = 0;

            const char* p = line.data() + 2;
            while (*p && vertCount < 4) {
                FaceVertex fv = {};
                while (*p == ' ') ++p;

                int vIdx = 0, vtIdx = 0, vnIdx = 0;
                int consumed = 0;
                if (sscanf(p, "%d/%d/%d%n", &vIdx, &vtIdx, &vnIdx, &consumed) == 3) {
                    // OK
                } else if (sscanf(p, "%d//%d%n", &vIdx, &vnIdx, &consumed) == 2) {
                    vtIdx = 0;
                } else if (sscanf(p, "%d/%d%n", &vIdx, &vtIdx, &consumed) == 2) {
                    vnIdx = 0;
                } else if (sscanf(p, "%d%n", &vIdx, &consumed) == 1) {
                    vtIdx = vnIdx = 0;
                } else {
                    break;
                }

                fv.v = vIdx;
                fv.vt = (vtIdx == 0) ? -1 : vtIdx;
                fv.vn = (vnIdx == 0) ? -1 : vnIdx;

                faceVerts[vertCount++] = fv;
                p += consumed;
            }

            if (vertCount == 3) {
                for (int i = 0; i < 3; ++i) {
                    md.indices.push_back(vert_auto(faceVerts[i].v, faceVerts[i].vt, faceVerts[i].vn));
                }
            } else if (vertCount == 4) {
                // 四边形拆三角形
                md.indices.push_back(vert_auto(faceVerts[0].v, faceVerts[0].vt, faceVerts[0].vn));
                md.indices.push_back(vert_auto(faceVerts[1].v, faceVerts[1].vt, faceVerts[1].vn));
                md.indices.push_back(vert_auto(faceVerts[2].v, faceVerts[2].vt, faceVerts[2].vn));

                md.indices.push_back(vert_auto(faceVerts[0].v, faceVerts[0].vt, faceVerts[0].vn));
                md.indices.push_back(vert_auto(faceVerts[2].v, faceVerts[2].vt, faceVerts[2].vn));
                md.indices.push_back(vert_auto(faceVerts[3].v, faceVerts[3].vt, faceVerts[3].vn));
            }
        }
    }

    std::cout << md.vertices.size() / 3 << " vertices, "
              << md.normals.size() / 3 << " normals, "
              << md.coords.size() / 2 << " coords, "
              << md.indices.size() << " indices." << std::endl;
}