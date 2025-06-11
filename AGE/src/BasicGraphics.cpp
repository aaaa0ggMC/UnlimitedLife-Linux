#include <AGE/VAO.h>
#include <AGE/VBO.h>

using namespace age;

VAO::VAO(GLuint idd){
    id = idd;
}

VAO VAO::null_vao(){
    VAO ret;
    ret.id = AGE_NULL_OBJ;
    return ret;
}

VAO VAOManager::operator [](unsigned int index){
    if(index >= vaos.size())return VAO::null_vao();
    return vaos[index];
}

VBO::VBO(GLuint idd){
    id = idd;
}

VBO VBO::null_vbo(){
    VBO ret;
    ret.id = AGE_NULL_OBJ;
    return ret;
}

VBO VBOManager::operator [](unsigned int index){
    if(index >= vbos.size())return VBO::null_vbo();
    return vbos[index];
}
