/**
 * @file shape.cppm
 * @brief Shape：一整块纯 CPU 几何数据（interleaved 顶点 + 索引）
 * @version 5.0
 * @date 2026-09-29
 * @start-date 2026/09/29
 *
 * 命名说明：刻意不叫 Model / Mesh。
 * - 不是 Model：没有材质、节点、场景图，距离及格线很远；
 * - 不是 Mesh：完整的网格概念连同渲染侧能力规划在 ave.model；
 *   Shape 只回答「这块几何长什么样」，是一整块、不再拆分的基本数据。
 */
module;
#include <AVE/config.h>
#include <glm/glm.hpp>

export module ave.shape:shape;

import std;
import alib6;

export namespace ave {

    /**
     * @brief 一整块纯几何数据（CPU 侧，无 GPU 资源）
     * @start-date 2026/09/29
     *
     * 顶点为 interleaved 布局，字段顺序即 ave::vertex_layout 反射出的
     * attribute location 顺序（position 0 / normal 1 / uv 2）；
     * 索引统一使用 uint32_t（VK_INDEX_TYPE_UINT32 为核心支持）。
     *
     * @note 绑定与上传由用户自理，典型用法：
     * @code
     * ave::Shape cube = ave::Prefab::cube(1.0f);
     * auto layout = ave::vertex_layout<ave::Shape::Vertex>().build();
     * // buffer->alloc(cube.vertices, slice_info) / buffer->alloc(cube.indices, ...)
     * // graphics.bind_index_buffer<std::uint32_t>(cube_indices);
     * @endcode
     */
    struct Shape {
        /// @brief interleaved 顶点：position / normal / uv
        struct Vertex {
            glm::vec3 position { 0.0f };
            glm::vec3 normal { 0.0f };
            glm::vec2 uv { 0.0f };
        };

        std::vector<Vertex> vertices {};
        std::vector<std::uint32_t> indices {};

        /// @brief 顶点数量
        [[nodiscard]] std::size_t vertex_count() const noexcept {
            return vertices.size();
        }

        /// @brief 索引数量
        [[nodiscard]] std::size_t index_count() const noexcept {
            return indices.size();
        }

        /// @brief 是否为空（无顶点或无索引的退化数据）
        [[nodiscard]] bool empty() const noexcept {
            return vertices.empty() || indices.empty();
        }

        /// @brief 清空数据（保留容量）
        void clear() noexcept {
            vertices.clear();
            indices.clear();
        }
    };

}
