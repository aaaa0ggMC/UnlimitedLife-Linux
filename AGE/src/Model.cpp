#include <AGE/Model.h>

using namespace age;

void ModelData::bind(
    VAO vao,
    VBO vertBuffer,
    VBO indiceBuffer,
    VBO coordBuffer,
    VBO normalBuffer,
    GLuint vertLocation ,
    GLuint coordLocation ,
    GLuint normalLocation 
){
    vao.bind();

    if(vertBuffer.getId()){
        vertBuffer.template bufferData<float>(vertices);
        vertBuffer.bind();
        vao.setAttribute(vertBuffer,vertLocation,3,GL_FLOAT);
        vao.setAttribStatus(vertLocation,true);
    }
    if(coordBuffer.getId()){
        coordBuffer.template bufferData<float>(coords);
        coordBuffer.bind();
        vao.setAttribute(coordBuffer,coordLocation,2,GL_FLOAT);
        vao.setAttribStatus(coordLocation,true);
    }
    if(normalBuffer.getId()){
        normalBuffer.template bufferData<float>(normals);
        normalBuffer.bind();
        vao.setAttribute(normalBuffer,normalLocation,3,GL_FLOAT);
        vao.setAttribStatus(normalLocation,true);
    }
    indiceBuffer.template bufferData<int>(indices);
    indiceBuffer.bind(GL_ELEMENT_ARRAY_BUFFER); //绑了空的更好,不能通过size从而不绑...不然内存会访问错误的
    bindedVAO = vao; // 记录绑定的VAO
}