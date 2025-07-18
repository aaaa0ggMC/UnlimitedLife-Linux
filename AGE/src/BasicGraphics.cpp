#include <AGE/VAO.h>
#include <AGE/VBO.h>
#include <AGE/Texture.h>

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
    ret.index = 0;
    return ret;
}

VAO VAOManager::operator [](unsigned int index){
    if(index >= vaos.size()){
        alloc(index);
    }
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

void VAOManager::alloc(unsigned int index){
    if(index < vaos.size())return;
    size_t oldSize = vaos.size();
    vaos.resize(index);
    glGenVertexArrays(index - oldSize,&vaos[oldSize]);
}

VBO::VBO(GLuint idd,uint32_t index){
    id = idd;
    this->index = index;
}

VBO VBO::null(){
    VBO ret;
    ret.id = AGE_NULL_OBJ;
    ret.index = 0;
    return ret;
}

VBO VBOManager::operator [](unsigned int index){
    if(index >= vbos.size()){
        alloc(index);
    }
    if(vbos[index])return VBO(vbos[index],index);
    else return VBO::null();
}

void VBOManager::alloc(unsigned int index){
    if(index < vbos.size())return;
    size_t oldSize = vbos.size();
    vbos.resize(index);
    glGenBuffers(index - oldSize,&vbos[oldSize]);
}

void VBOManager::add(GLuint v){
    vbos.push_back(v);
}

void VBOManager::markAsFree(uint32_t index){
    if(index >= vbos.size())return;
    vbos[index] = 0;
}

// 你知道为什么texture在这里吗，因为我不想多写个文件了
CreateTextureInfo::CreateTextureInfo(){
    this->source = FromFile;

    this->file.path = "";
    this->sid = "";
    this->buffer.eleCount = 0;
    this->buffer.data = nullptr;
    this->vec.data = nullptr;
    this->channel_desired = 4;
    this->uploadToOpenGL = true;
    this->genMipmap = false;
}