/**
 * @file Model.h
 * @author aaaa0ggmc
 * @brief 提供模型相关的支持，目前不支持Mesh,Mesh要等我再学学
 * @version 0.1
 * @date 2025/07/22
 * @start-date 2025/07/18
 * @copyright Copyright (c) 2025
*/
#ifndef AGE_H_MODEL
#define AGE_H_MODEL
#include <AGE/Utils.h>
#include <GL/glew.h>
#include <AGE/Material.h>
#include <AGE/VAO.h>
#include <AGE/VBO.h>

namespace age{
    template<class T> concept CanDrawElements = requires(T t){
        {t.drawElements(std::declval<age::PrimitiveType>(),std::declval<size_t>(),std::declval<GLuint>(),std::declval<const std::vector<int>&>(),std::declval<age::VAO>())};
    };

    /// @todo Mesh & Material
    struct AGE_API Mesh{
        size_t vertex_offset;
        size_t normal_offset;
        size_t coord_offset;
        size_t index_offset;

        Material material;
        std::string meshName;
    };

    /**
     * @brief 存储模型数据
     * @note 作为过渡的小玩意儿
     */
    struct AGE_API ModelData{
    private:
        VAO bindedVAO;
    public:
        std::vector<float> vertices;
        std::vector<float> normals;
        std::vector<float> coords;
        std::vector<int> indices;
        std::vector<Mesh> meshes;

        /// 属于扩展功能,一般不建议使用
        /// 需要一个vao
        void bind(
            VAO vao,
            VBO vertBuffer,
            VBO indiceBuffer,
            VBO coordBuffer,
            VBO normalBuffer = VBO::null(),
            GLuint vertLocation = 0,
            GLuint coordLocation = 1,
            GLuint normalLocation = 2
        );

        inline size_t getIndiceCount() const{
            return indices.size();
        }
        
        template<CanDrawElements T> inline void draw(T & window,GLuint instanceCount,PrimitiveType type) const{
            if(!instanceCount)return;
            window.drawElements(
                type,
                getIndiceCount(),
                instanceCount,
                {}, // use GL_ELEMENT_ARRAY_BUFFER
                bindedVAO
            );
        }
    };
}

#endif