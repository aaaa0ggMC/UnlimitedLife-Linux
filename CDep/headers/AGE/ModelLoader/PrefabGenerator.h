/**
 * @file PrefabGenerator.h
 * @author aaaa0ggmc
 * @brief 生成模型预设
 * @version 0.1
 * @date 2025/07/19
 * @start-date 2025/07/19
 * @copyright Copyright (c) 2025
*/
#ifndef AGE_H_PREFABG
#define AGE_H_PREFABG
#include <AGE/Utils.h>
#include <vector>

namespace age::model{
    struct AGE_API Prefab{
        using vecf = std::vector<float>;
        using veci = std::vector<int>;

        /// @brief 生成一个球
        /// @param precision 精度，为0时生成一个点
        /// @param vertices 顶点数据
        /// @param indices 索引数据
        /// @param normals 法向量
        /// @param coords 纹理坐标
        /// @note 必定会修改四个vector!
        static void sphere(size_t precision,vecf & vertices,veci & indices,vecf & normals,vecf & coords);
    };
};

#endif