/**
 * @file PrefabGenerator.h
 * @author aaaa0ggmc
 * @brief 生成模型预设
 * @version 0.1
 * @date 2025/07/25
 * @start-date 2025/07/19
 * @copyright Copyright (c) 2025
*/
#ifndef AGE_H_PREFABG
#define AGE_H_PREFABG
#include <AGE/Utils.h>
#include <AGE/Model.h>
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
        static void sphere(size_t precision,ModelData & model,float uvscale = 1);
        /// @return scene id
        static inline int sphere(size_t precision,Model & model,float uvscale = 1,
            int index = 0,GLuint vloc = 0,GLuint vtloc = 1,GLuint vnloc = 2,bool loadTheScene = true){
            if(precision == 0)return -1;
            auto v = model.get(index);
            auto data = (!v)?model.add():(*v);
            ModelData& md = *data.first;
            Model::GraphRes& gr = *data.second;
            sphere(precision,md,uvscale);
            md.bind(gr.vao,gr.vbuffer,gr.ibuffer,gr.vtbuffer,gr.vnbuffer,vloc,vtloc,vnloc);
            if(loadTheScene)model.loadScene(model.getSceneCount() - 1);
            return model.getSceneCount() - 1;
        }

        
        /// @brief 生成一个环
        /// @param precision 精度，为0时生成一个点
        /// @param vertices 顶点数据
        /// @param indices 索引数据
        /// @param normals 法向量
        /// @param coords 纹理坐标
        /// @param innerRadius 环内部的半径
        /// @param ringRadius 环的半径(就是那一个个切片的半径)
        /// @note 必定会修改四个vector!
        static void torus(size_t precision,float innerRadius,float ringRadius,ModelData & model,float uvscale = 1);
        /// @return scene id
        static inline int torus(size_t precision,float innerRadius,float ringRadius,Model & model,float uvscale = 1,
            int index = 0,GLuint vloc = 0,GLuint vtloc = 1,GLuint vnloc = 2,bool loadTheScene = true){
            if(precision == 0)return -1;
            auto v = model.get(index);
            auto data = (!v)?model.add():(*v);
            ModelData& md = *data.first;
            Model::GraphRes& gr = *data.second;
            torus(precision,innerRadius,ringRadius,md,uvscale);
            md.bind(gr.vao,gr.vbuffer,gr.ibuffer,gr.vtbuffer,gr.vnbuffer,vloc,vtloc,vnloc);
            if(loadTheScene)model.loadScene(model.getSceneCount() - 1);
            return model.getSceneCount() - 1;
        }

        /// @brief 生成一个立方体,绕序 CCW
        static void box(float w,float h,float d,ModelData & model,float uvscale = 1);
        /// @return scene id
        static inline int box(float w,float h,float d,Model & model,float uvscale = 1,
            int index = 0,GLuint vloc = 0,GLuint vtloc = 1,GLuint vnloc = 2,bool loadTheScene = true){
            if(w == 0 && h == 0 && d == 0)return -1;
            auto v = model.get(index);
            auto data = (!v)?model.add():(*v);
            ModelData& md = *data.first;
            Model::GraphRes& gr = *data.second;
            box(w,h,d,md,uvscale);
            md.bind(gr.vao,gr.vbuffer,gr.ibuffer,gr.vtbuffer,gr.vnbuffer,vloc,vtloc,vnloc);
            if(loadTheScene)model.loadScene(model.getSceneCount() - 1);
            return model.getSceneCount() - 1;
        }

        static inline void cube(float length,ModelData & model,float uvscale = 1){
            if(length == 0)return;
            box(length,length,length,model,uvscale);
        }
        /// @return scene id
        static inline int cube(float length,Model & model,float uvscale = 1,
            int index = 0,GLuint vloc = 0,GLuint vtloc = 1,GLuint vnloc = 2,bool loadTheScene = true){
            if(length == 0)return -1;
            auto v = model.get(index);
            auto data = (!v)?model.add():(*v);
            ModelData& md = *data.first;
            Model::GraphRes& gr = *data.second;
            cube(length,md,uvscale);
            md.bind(gr.vao,gr.vbuffer,gr.ibuffer,gr.vtbuffer,gr.vnbuffer,vloc,vtloc,vnloc);
            if(loadTheScene)model.loadScene(model.getSceneCount() - 1);
            return model.getSceneCount() - 1;
        }

        /// 复用precision检查逻辑
        static inline bool check(size_t precision,vecf & vertices,veci & indices,vecf & normals,vecf & coords){
            if(precision == 0){
                vertices.resize(3);
                indices.resize(3);
                normals.resize(3);
                coords.resize(2);

                vertices[0] = vertices[1] = vertices[2] = 0;
                normals[0] = normals[1] = normals[2] = 0;
                coords[0] = coords[1] = 0;
                indices[0] = indices[1] = indices[2] = 0;
                return true;
            }
            return false;
        }
    };
};

#endif