#include <AGE/VAO.h>
#include <AGE/VBO.h>

using namespace age;

GLuint VBO::current = 0;
GLuint VAO::current = 0;

VAO::VAO(GLuint idd,uint32_t index){
    id = idd;
    this->index = index;
}

VAO VAO::null(){
    VAO ret;
    ret.id = AGE_NULL_OBJ;
    return ret;
}

VAO VAOManager::operator [](unsigned int index){
    if(index >= vaos.size())return VAO::null();
    if(vaos[index])return VAO(vaos[index],index);
    else return VAO::null();
}

void VAOManager::add(GLuint v){
    vaos.push_back(v);
}

void VAOManager::markAsFree(uint32_t index){
    if(index >= vaos.size())return;
    vaos[index] = 0;
}

VBO::VBO(GLuint idd,uint32_t index){
    id = idd;
    this->index = index;
}

VBO VBO::null(){
    VBO ret;
    ret.id = AGE_NULL_OBJ;
    return ret;
}

VBO VBOManager::operator [](unsigned int index){
    if(index >= vbos.size())return VBO::null();
    if(vbos[index])return VBO(vbos[index],index);
    else return VBO::null();
}

void VBOManager::add(GLuint v){
    vbos.push_back(v);
}

void VBOManager::markAsFree(uint32_t index){
    if(index >= vbos.size())return;
    vbos[index] = 0;
}
