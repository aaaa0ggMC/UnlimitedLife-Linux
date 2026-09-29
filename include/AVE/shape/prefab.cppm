/**
 * @file prefab.cppm
 * @brief Prefab：程序化生成 Shape（box / cube / plane）
 * @version 5.0
 * @date 2026-09-29
 * @start-date 2026/09/29
 *
 * 从 AGE 的 age::model::Prefab 移植：AGE 以「修改 ModelData 的四个 vector」
 * 的出参风格生成，本模块改为函数式按值返回 ave::Shape，语义更直接。
 * 目前先移植 box/cube 家族并补一个 plane；球/环等留待后续按需补充。
 */
module;
#include <AVE/config.h>
#include <glm/glm.hpp>

export module ave.shape:prefab;

import std;
import alib6;
import :shape;

export namespace ave {

    /**
     * @brief 程序化几何生成器
     * @start-date 2026/09/29
     *
     * 目前提供：
     * - box/cube：长方/正方体，24 顶点（每面独立 UV 与法线），12 三角形，绕序 CCW；
     * - plane：XZ 平面（法线 +Y），y = 0，适合地面。
     *
     * 所有生成器在尺寸全为 0 时返回空 ave::Shape（empty() 为 true）。
     * uv_scale 为每面 UV 的重复次数（配合 REPEAT 采样器平铺纹理）。
     */
    struct Prefab {
        /**
         * @brief 生成长方体
         * @param width  X 方向长度
         * @param height Y 方向长度
         * @param depth  Z 方向长度
         * @param uv_scale 每面 UV 重复次数（默认 1）
         * @return 生成的 ave::Shape；三维全为 0 时为空
         */
        [[nodiscard]] static Shape box(
            float width,
            float height,
            float depth,
            float uv_scale = 1.0f
        );

        /**
         * @brief 生成正方体（等价于 box(n, n, n)）
         * @param length 边长
         * @param uv_scale 每面 UV 重复次数（默认 1）
         * @return 生成的 ave::Shape；边长为 0 时为空
         */
        [[nodiscard]] static Shape cube(float length, float uv_scale = 1.0f);

        /**
         * @brief 生成 XZ 平面（法线 +Y，位于 y = 0，中心对齐）
         * @param width  X 方向长度
         * @param depth  Z 方向长度
         * @param uv_scale UV 重复次数（默认 1；地面平铺常用较大值）
         * @return 生成的 ave::Shape；尺寸全为 0 时为空
         */
        [[nodiscard]] static Shape plane(
            float width,
            float depth,
            float uv_scale = 1.0f
        );
    };

    inline Shape Prefab::box(
        float width,
        float height,
        float depth,
        float uv_scale
    ){
        if(width == 0.0f && height == 0.0f && depth == 0.0f) {
            return {};
        }

        using Vertex = Shape::Vertex;
        Shape shape;
        shape.vertices.reserve(24);
        shape.indices.reserve(36);

        const float x = width / 2.0f;
        const float y = height / 2.0f;
        const float z = depth / 2.0f;

        struct Face {
            glm::vec3 normal;
            std::array<glm::vec3, 4> corners; ///< 左下、右下、右上、左上（正面朝外）
        };

        // 六面顺序：前(+Z) 后(-Z) 左(-X) 右(+X) 下(-Y) 上(+Y)
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

    inline Shape Prefab::cube(float length, float uv_scale) {
        return box(length, length, length, uv_scale);
    }

    inline Shape Prefab::plane(float width, float depth, float uv_scale) {
        if(width == 0.0f && depth == 0.0f) {
            return {};
        }

        using Vertex = Shape::Vertex;
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

}
