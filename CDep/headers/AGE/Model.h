/**
 * @file Model.h
 * @author aaaa0ggmc
 * @brief 提供模型相关的支持，目前不支持Mesh,Mesh要等我再学学
 * @version 0.1
 * @date 2025/07/19
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
        std::vector<float> normals;
        std::vector<float> coords;
        std::vector<int> indices;

        /// 利用模板实现不依赖头文件实际上摆明了就是要这个类的上传,属于扩展功能
        /// 需要一个vao
        template<class VAO,class VBO> inline void bind(
            VAO vao,
            VBO vertBuffer,
            VBO indiceBuffer,
            VBO coordBuffer,
            VBO normalBuffer = VBO::null(),
            GLuint vertLocation = 0,
            GLuint coordLocation = 1,
            GLuint normalLocation = 2
        ){
            vao.bind();
            vertBuffer.template bufferData<float>(vertices);
            coordBuffer.template bufferData<float>(coords);
            indiceBuffer.template bufferData<int>(indices);

            if(vertBuffer.getId()){
                vertBuffer.bind();
                vao.setAttribute(vertBuffer,vertLocation,3,GL_FLOAT);
                vao.setAttribStatus(vertLocation,true);
            }
            if(coordBuffer.getId()){
                coordBuffer.bind();
                vao.setAttribute(coordBuffer,coordLocation,2,GL_FLOAT);
                vao.setAttribStatus(coordLocation,true);
            }
            if(normalBuffer.getId()){
                normalBuffer.bind();
                vao.setAttribute(normalBuffer,normalLocation,3,GL_FLOAT);
                vao.setAttribStatus(normalLocation,true);
            }

            indiceBuffer.bind(GL_ELEMENT_ARRAY_BUFFER); //绑了空的更好,不能通过size从而不绑...不然内存会访问错误的
        }

        inline size_t getIndiceCount(){
            return indices.size();
        }

    };
}

#endif