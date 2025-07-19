#include <AGE/ModelLoader/PrefabGenerator.h>
#include <numbers>
#include <cmath>

using namespace age::model;

void Prefab::sphere(size_t precision,vecf & vertices,veci & indices,vecf & normals,vecf & coords){
    if(precision == 0){
        vertices.resize(3);
        indices.resize(3);
        normals.resize(3);
        coords.resize(2);

        vertices[0] = vertices[1] = vertices[2] = 0;
        normals[0] = normals[1] = normals[2] = 0;
        coords[0] = coords[1] = 0;
        indices[0] = indices[1] = indices[2] = 0;
        return;
    }
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