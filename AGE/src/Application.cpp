#include <AGE/Details/GLInit.h>
#include <AGE/Application.h>
#include <GL/glext.h>
#include <cstring>
#include <GL/glu.h>
#include <AGE/Input.h>
#include <alib-g3/autil.h>

// #include <iostream>

using namespace age;
using namespace alib::g3;

bool GLInit::inited_glew = false;
bool GLInit::inited_glfw = false;
unsigned int Application::counter = 0;

void GLInit::GLEW(){
    if(!inited_glew){
        inited_glew = true;
        glewInit();
    }
}

void GLInit::GLFW(){
    if(!inited_glfw){
        inited_glfw = true;
        glfwInit();
    }
}

void GLInit::endGLFW(){
    if(inited_glfw){
        inited_glfw = false;
        glfwTerminate();
    }
}

Application::Application(EntityManager & emm):em{emm}{
    counter++;
    Error::def.setTrigger();
}

Application::~Application(){
    //Step2:VAOS
    //std::cout << vaos.vaos.size() << std::endl;
    if(vaos.vaos.size())glDeleteVertexArrays(vaos.vaos.size(),&(vaos.vaos[0]));
    //Step3:VBOS
    //std::cout << vbos.vbos.size() << std::endl;
    if(vbos.vbos.size())glDeleteBuffers(vbos.vbos.size(),&(vbos.vbos[0]));

    counter--;
    if(counter == 0){
        GLInit::endGLFW();
    }
}

void Application::createVAOs(const CreateVAOsInfo & info){
    if(info.count != 0){
        vaos.alloc(info.count + vaos.vaos.size());
    }
}

void Application::createVAOs(uint32_t count){
    createVAOs({count,0});
}

void Application::createVBOs(uint32_t count){
    createVBOs({count,0});
}

void Application::createVBOs(const CreateVBOsInfo & info){
    if(info.count != 0){
        vbos.alloc(info.count + vbos.vbos.size());
    }
}

VAO Application::getVAO(uint32_t index){
    return vaos[index];
}

VBO Application::getVBO(uint32_t index){
    return vbos[index];
}

bool Application::destroyVBO(VBO vbo){
    if(vbo.getId()){
        auto id = vbo.getId();
        glDeleteBuffers(1,&id);
        vbos.markAsFree(vbo.getManagerIndex());
        return true;
    }
    return false;
}

bool Application::destroyVAO(VAO vao){
    if(vao.getId()){
        auto id = vao.getId();
        glDeleteVertexArrays(1,&id);
        vaos.markAsFree(vao.getManagerIndex());
        return true;
    }
    return false;
}

void GLAPIENTRY Application::glErrDefDebugProc(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam
){
    Application & app = *((Application*)userParam);
    static thread_local std::string buf = "";
    
    if(id == 131222 || id == 131185){
        return;
    }

    
    buf.clear();
    const char* sourceStr = "";
    switch (source) {
        case GL_DEBUG_SOURCE_API:             sourceStr = "API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   sourceStr = "Window System"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceStr = "Shader Compiler"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     sourceStr = "Third Party"; break;
        case GL_DEBUG_SOURCE_APPLICATION:     sourceStr = "Application"; break;
        case GL_DEBUG_SOURCE_OTHER:           sourceStr = "Other"; break;
        default:                              sourceStr = "Unknown"; break;
    }

    const char* typeStr = "";
    switch (type) {
        case GL_DEBUG_TYPE_ERROR:                 typeStr = "Error"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:   typeStr = "Deprecated Behavior"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:    typeStr = "Undefined Behavior"; break;
        case GL_DEBUG_TYPE_PORTABILITY:           typeStr = "Portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE:           typeStr = "Performance"; break;
        case GL_DEBUG_TYPE_MARKER:                typeStr = "Marker"; break;
        case GL_DEBUG_TYPE_PUSH_GROUP:            typeStr = "Push Group"; break;
        case GL_DEBUG_TYPE_POP_GROUP:             typeStr = "Pop Group"; break;
        case GL_DEBUG_TYPE_OTHER:                 typeStr = "Other"; break;
        default:                                  typeStr = "Unknown"; break;
    }

    const char* severityStr = "";
    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:         severityStr = "High"; break;
        case GL_DEBUG_SEVERITY_MEDIUM:       severityStr = "Medium"; break;
        case GL_DEBUG_SEVERITY_LOW:          severityStr = "Low"; break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: severityStr = "Notification"; break;
        default:                             severityStr = "Unknown"; break;
    }

    buf += "[OpenGL Debug]";
    buf += "\n[ Source ]" + std::string(sourceStr);
    buf += "\n[  Type  ]" + std::string(typeStr);
    buf += "\n[   ID   ]" + std::to_string(id);
    buf += "\n[Severity]" + std::string(severityStr);
    buf += "\n[Message ]" + std::string(message) + "\n";

    Error::def.pushMessage({AGEE_OPENGL_DEBUG_MESSAGE,buf.c_str()});
}