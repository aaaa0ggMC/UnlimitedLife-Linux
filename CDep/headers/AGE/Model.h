/**
 * @file Model.h
 * @author aaaa0ggmc
 * @brief 提供模型相关的支持，目前不支持Mesh,Mesh要等我再学学
 * @version 0.1
 * @date 2025/07/18
 * @start-date 2025/07/18
 * @copyright Copyright (c) 2025
*/
#ifndef AGE_H_MODEL
#define AGE_H_MODEL
#include <AGE/Utils.h>
#include <GL/glew.h>

namespace age{
    /**
     * @brief 存储模型数据
     * @note 作为过渡的小玩意儿
     */
    struct ModelData{
        std::vector<float> vertices;
        std::vector<float> coords;
        std::vector<int> indices;

        /// 利用模板实现不依赖头文件实际上摆明了就是要这个类的上传,属于扩展功能
        /// 需要一个vao
        inline template<class VAO,class VBO> void _ext_upload(VAO & vao,VBO & vertBuffer,VBO& coordBuffer,VBO & indiceBuffer,GLuint vertPort = 0,GLuint coordPort = 1){
            vao.bind();
            vertBuffer.bufferData<float>(vertices);
            coordBuffer.bufferData<float>(coords);
            indiceBuffer.bufferData<int>(indices);

            vertBuffer.bind();
            vao.setAttribute(vertBuffer,vertPort,3,GL_FLOAT);
            vao.setAttribStatus(vertPort,true);
            coordBuffer.bind();
            vao.setAttribute(coordBuffer,coordPort,2,GL_FLOAT);
            vao.setAttribStatus(coordPort,true);

            indiceBuffer.bind(GL_ELEMENT_ARRAY_BUFFER);
        }

        inline template<class VAO> void _ext_draw(VAO & vao){
            vao.bind();
            glDrawElements(GL_TRIANGLES,indices.size(),GL_UNSIGNED_INT,0);
            vao.unbind();
        } 
    };
}

#endif