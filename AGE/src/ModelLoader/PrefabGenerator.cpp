#include <AGE/ModelLoader/PrefabGenerator.h>
#include <numbers>
#include <cmath>
#include <glm/glm.hpp>

using namespace age::model;

void Prefab::sphere(size_t precision,vecf & vertices,veci & indices,vecf & normals,vecf & coords){
    if(check(precision,vertices,indices,normals,coords))return;
    size_t vertex_count = (precision+1) * (precision+1);
    size_t index_count = precision * precision * 6;
    //alloc size
    vertices.resize(3 * vertex_count,0);
    indices.resize(index_count,0);
    normals.resize(3 * vertex_count,0);
    coords.resize(2 * vertex_count,0);
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
            vertices[3 * base + 0] = x;vertices[3 * base + 1] = y;vertices[3 * base + 2] = z;
            // 法向量不要求模吗，虽然这里就是模
            normals[3 * base + 0] = x;normals[3 * base + 1] = y;normals[3 * base + 2] = z;
            coords[2 * base + 0] = (float)j / precision;coords[2 * base + 1] = (float)i / precision;

            //calculate indices
            if(i != precision && j != precision){
                base = 6 * (i * precision + j);
                indices[base + 0] = i * (precision + 1) + j;
                indices[base + 1] = i * (precision + 1) + j + 1;
                indices[base + 2] = (i + 1) * (precision + 1) + j;
                indices[base + 3] = i * (precision + 1) + j + 1;
                indices[base + 4] = (i + 1) * (precision + 1) + j + 1;
                indices[base + 5] = (i + 1) * (precision + 1) + j;
            }
        }
    }
}


void Prefab::torus(size_t precision, float innerRadius, float ringRadius,
                   vecf &vertices, veci &indices, vecf &normals, vecf &coords) {
    if (check(precision, vertices, indices, normals, coords)) return;

    size_t vertex_count = (precision + 1) * (precision + 1);
    size_t index_count = precision * precision * 6;

    vertices.resize(3 * vertex_count);
    indices.resize(index_count);
    normals.resize(3 * vertex_count);
    coords.resize(2 * vertex_count);

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

            vertices[3 * base + 0] = x;
            vertices[3 * base + 1] = y;
            vertices[3 * base + 2] = z;

            glm::vec3 normal = glm::normalize(glm::vec3(cosPhi * cosTheta, sinPhi, cosPhi * sinTheta));
            normals[3 * base + 0] = normal.x;
            normals[3 * base + 1] = normal.y;
            normals[3 * base + 2] = normal.z;

            coords[2 * base + 0] = (float)j / precision;
            coords[2 * base + 1] = (float)i / precision;

            if (i < precision && j < precision) {
                size_t idx = 6 * (i * precision + j);
                indices[idx + 0] = base;
                indices[idx + 1] = base + 1;
                indices[idx + 2] = base + (precision + 1);
                indices[idx + 3] = base + 1;
                indices[idx + 4] = base + (precision + 1) + 1;
                indices[idx + 5] = base + (precision + 1);
            }
        }
    }
}
