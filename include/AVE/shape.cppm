/**
 * @file shape.cppm
 * @brief ave.shape：纯 CPU 侧几何数据与程序化生成，不与 Renderer 联动
 * @version 5.0
 * @date 2026-09-29
 *
 * 从 AGE 的 ModelData 移植并重新划分概念：
 * - AGE 的 ModelData 混合了「顶点/法线/UV 数据 + VAO 绑定 + 场景/子网格清单」，
 *   名字也容易误导成 Model（实际上远达不到 Model 的及格线：无材质、无节点层级）；
 * - AVE 将其收敛为 @ref ave::Shape：只承载一整块几何数据
 *   （interleaved 顶点 position/normal/uv + 索引），不绑定任何 GPU 资源、
 *   不感知 Renderer；绑定与上传完全交给用户（配合 ave::vertex_layout 反射
 *   + ave::VMABuffer::alloc 即可）。
 * - @ref ave::Prefab 提供程序化生成（box/cube/plane），替代手写顶点数组。
 * - 更完整的网格/模型能力（多子网格、材质、层级）规划在 ave.model，与这里无关。
 */
module;

export module ave.shape;

export import :shape;
export import :prefab;
export import :loader;
