/**
 * @file prefab.cppm
 * @brief Prefab：程序化生成 Shape（box / cube / plane）
 * @version 5.0
 * @date 2026-09-29
 * @start-date 2026/09/29
 *
 * 从 AGE 的 age::model::Prefab 移植：AGE 以「修改 ModelData 的四个 vector」
 * 的出参风格生成，本模块改为函数式按值返回 ave::Shape，语义更直接。
 * 目前提供 box/cube 家族、plane（AGE 没有，地面常用）与 sphere/torus。
 * precision 小于 1 或尺寸全 0 时统一返回空 ave::Shape。
 */
module;
#include <AVE/config.h>

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
     * - plane：XZ 平面（法线 +Y，y = 0，中心对齐），适合地面。
     *
     * 所有生成器在尺寸全为 0 时返回空 ave::Shape（empty() 为 true）。
     * uv_scale 为每面 UV 的重复次数（配合 REPEAT 采样器平铺纹理）。
     */
    struct AVE_API Prefab {
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

        /**
         * @brief 生成 UV 球（经纬球）
         * @start-date 2026/09/29
         * @param precision 细分精度：(precision+1)² 个顶点，赤道 precision 段；
         *                  小于 1 时返回空 ave::Shape
         * @param radius 半径（默认 1）
         * @param uv_scale UV 重复次数（默认 1）
         * @return 生成的 ave::Shape；法线为归一化位置
         */
        [[nodiscard]] static Shape sphere(
            std::uint32_t precision,
            float radius = 1.0f,
            float uv_scale = 1.0f
        );

        /**
         * @brief 生成圆环（torus，环绕 Y 轴）
         * @start-date 2026/09/29
         * @param precision 细分精度：两个方向各 precision 段；
         *                  小于 1 时返回空 ave::Shape
         * @param inner_radius 环心半径（圆心到管中心的距离）
         * @param ring_radius 管半径
         * @param uv_scale UV 重复次数（默认 1）
         * @return 生成的 ave::Shape
         */
        [[nodiscard]] static Shape torus(
            std::uint32_t precision,
            float inner_radius,
            float ring_radius,
            float uv_scale = 1.0f
        );
    };

}
