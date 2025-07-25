#include <AGE/ModelLoader/PrefabGenerator.h>
#include <numbers>
#include <cmath>
#include <glm/glm.hpp>

using namespace age::model;

void Prefab::sphere(size_t precision,ModelData & m){
    if(check(precision,m.vertices,m.indices,m.normals,m.coords))return;
    size_t vertex_count = (precision+1) * (precision+1);
    size_t index_count = precision * precision * 6;
    //alloc size
    m.vertices.resize(3 * vertex_count,0);
    m.indices.resize(index_count,0);
    m.normals.resize(3 * vertex_count,0);
    m.coords.resize(2 * vertex_count,0);
    for(size_t i = 0;i <= precision;++i){
        float theta = (float)i / precision * std::numbers::pi;
        // 从下面开始生成
        float y = -std::cos(theta);
        float topRadius = std::sin(theta); 
        for(size_t j = 0;j <= precision;++j){
            float omega = (float)j / precision * std::numbers::pi * 2;
            // 也是从极小值(x)开始生成
            float x = -topRadius * std::cos(omega); 
            float z = topRadius * std::sin(omega);

            size_t base = i * (precision + 1) + j;
            m.vertices[3 * base + 0] = x;m.vertices[3 * base + 1] = y;m.vertices[3 * base + 2] = z;
            // 法向量不要求模吗，虽然这里就是模
            m.normals[3 * base + 0] = x;m.normals[3 * base + 1] = y;m.normals[3 * base + 2] = z;
            m.coords[2 * base + 0] = (float)j / precision;m.coords[2 * base + 1] = (float)i / precision;

            //calculate indices
            if(i != precision && j != precision){
                base = 6 * (i * precision + j);
                m.indices[base + 0] = i * (precision + 1) + j;
                m.indices[base + 1] = i * (precision + 1) + j + 1;
                m.indices[base + 2] = (i + 1) * (precision + 1) + j;
                m.indices[base + 3] = i * (precision + 1) + j + 1;
                m.indices[base + 4] = (i + 1) * (precision + 1) + j + 1;
                m.indices[base + 5] = (i + 1) * (precision + 1) + j;
            }
        }
    }
    
    m.meshes.push_back({0,0,0,0,material::Material(),"main"});
}


void Prefab::torus(size_t precision, float innerRadius, float ringRadius,ModelData & m) {
    if (check(precision,m.vertices,m.indices,m.normals,m.coords)) return;

    size_t vertex_count = (precision + 1) * (precision + 1);
    size_t index_count = precision * precision * 6;

    m.vertices.resize(3 * vertex_count);
    m.indices.resize(index_count);
    m.normals.resize(3 * vertex_count);
    m.coords.resize(2 * vertex_count);

    for (size_t i = 0; i <= precision; ++i) {
        float theta = (float)i / precision * 2.0f * std::numbers::pi;
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);

        for (size_t j = 0; j <= precision; ++j) {
            float phi = (float)j / precision * 2.0f * std::numbers::pi;
            float cosPhi = std::cos(phi);
            float sinPhi = std::sin(phi);

            float x = (innerRadius + ringRadius * cosPhi) * cosTheta;
            float y = ringRadius * sinPhi;
            float z = (innerRadius + ringRadius * cosPhi) * sinTheta;

            size_t base = i * (precision + 1) + j;

            m.vertices[3 * base + 0] = x;
            m.vertices[3 * base + 1] = y;
            m.vertices[3 * base + 2] = z;

            glm::vec3 normal = glm::normalize(glm::vec3(cosPhi * cosTheta, sinPhi, cosPhi * sinTheta));
            m.normals[3 * base + 0] = normal.x;
            m.normals[3 * base + 1] = normal.y;
            m.normals[3 * base + 2] = normal.z;

            m.coords[2 * base + 0] = (float)j / precision;
            m.coords[2 * base + 1] = (float)i / precision;

            if (i < precision && j < precision) {
                size_t idx = 6 * (i * precision + j);
                m.indices[idx + 0] = base;
                m.indices[idx + 1] = base + 1;
                m.indices[idx + 2] = base + (precision + 1);
                m.indices[idx + 3] = base + 1;
                m.indices[idx + 4] = base + (precision + 1) + 1;
                m.indices[idx + 5] = base + (precision + 1);
            }
        }
    }

    m.meshes.push_back({0,0,0,0,material::Material(),"main"});
}

void Prefab::box(float w, float h, float d, ModelData & m) {
    float x = w / 2.0f;
    float y = h / 2.0f;
    float z = d / 2.0f;

    // 6个面，每个面4个顶点，共24个顶点
    m.vertices = {
        // Front face (z+)
        -x, -y, z,   x, -y, z,   x, y, z,   -x, y, z,
        // Back face (z-)
        x, -y, -z,  -x, -y, -z,  -x, y, -z,   x, y, -z,
        // Left face (x-)
        -x, -y, -z, -x, -y, z,  -x, y, z,   -x, y, -z,
        // Right face (x+)
        x, -y, z,   x, -y, -z,   x, y, -z,   x, y, z,
        // Bottom face (y-)
        -x, -y, -z,  x, -y, -z,  x, -y, z,   -x, -y, z,
        // Top face (y+)
        -x, y, z,    x, y, z,    x, y, -z,   -x, y, -z
    };

    m.normals = {
        // Front
        0, 0, 1,  0, 0, 1,  0, 0, 1,  0, 0, 1,
        // Back
        0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1,
        // Left
        -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0,
        // Right
        1, 0, 0,  1, 0, 0,  1, 0, 0,  1, 0, 0,
        // Bottom
        0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0,
        // Top
        0, 1, 0,  0, 1, 0,  0, 1, 0,  0, 1, 0
    };

    m.coords = {
        // 每面使用同一组UV（左下、右下、右上、左上）
        0, 0, 1, 0, 1, 1, 0, 1, // Front
        0, 0, 1, 0, 1, 1, 0, 1, // Back
        0, 0, 1, 0, 1, 1, 0, 1, // Left
        0, 0, 1, 0, 1, 1, 0, 1, // Right
        0, 0, 1, 0, 1, 1, 0, 1, // Bottom
        0, 0, 1, 0, 1, 1, 0, 1  // Top
    };

    // 每个面用两个三角形，共 12 个三角形
    m.indices = {
        0, 1, 2,  0, 2, 3,        // Front
        4, 5, 6,  4, 6, 7,        // Back
        8, 9,10,  8,10,11,        // Left
       12,13,14, 12,14,15,        // Right
       16,17,18, 16,18,19,        // Bottom
       20,21,22, 20,22,23         // Top
    };

    m.meshes.push_back({0, 0, 0, 0, material::Material(), "main"});
}
