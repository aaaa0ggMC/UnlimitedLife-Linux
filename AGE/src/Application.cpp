#include <AGE/Application.h>
#include <GL/glext.h>
#include <cstring>
#include <GL/glu.h>

// #include <iostream>

using namespace age;

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

void Application::setGLVersion(unsigned int major,unsigned int minor){
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,minor);
}

std::optional<Window*> Application::createWindow(const CreateWindowInfo &info){
    Window * win = new Window();
    GLInit::GLFW();
    win->sid = info.sid;
    win->window = glfwCreateWindow(
                    info.width,info.height,info.windowTitle.c_str(),
                    (!info.moniter)?NULL:(*info.moniter),
                    (!info.share)?NULL:(*info.share)->window);
    win->s_current = NULL;
    if(win->window == NULL){
        delete win;
        return std::nullopt;
    }
    windows.emplace(info.sid,win);
    if(info.x >= 0 && info.y >= 0)glfwSetWindowPos(win->window,info.x,info.y);
    if(!(Window::current))win->makeCurrent();
    GLInit::GLEW();
    return {win};
}

std::optional<Window*> Application::getWindow(const std::string & sid){
    auto v = windows.find(sid);
    if(v == windows.end())return std::nullopt;
    return {v->second};
}

bool Application::destroyWindow(const std::string & sid){
    auto v = windows.find(sid);
    if(v == windows.end())return false;
    Window * win = v->second;
    glfwDestroyWindow(win->window);
    delete win;
    windows.erase(sid);
    return true;
}

bool Application::destroyWindow(Window * win){
    if(!win)return false;
    return destroyWindow(win->sid);
}

Application::Application(){
    counter++;
    defErr.setTrigger();
}

Application::~Application(){
    //cleanup
    //Step1:Shaders
    //std::cout << shaders.size() << std::endl;
    for(auto& [sid,shd] : shaders){
        shd.destroy();
    }
    //Step2:VAOS
    //std::cout << vaos.vaos.size() << std::endl;
    glDeleteVertexArrays(vaos.vaos.size(),&(vaos.vaos[0]));
    //Step3:VBOS
    //std::cout << vbos.vbos.size() << std::endl;
    glDeleteBuffers(vbos.vbos.size(),&(vbos.vbos[0]));
    //Stepn:Windows
    {
        std::vector<std::string> window_names;
        for(auto& [sid,_] : windows){
            window_names.push_back(sid);
        }

        for(auto & sid : window_names){
            destroyWindow(sid);
        }
    }

    counter--;
    if(counter == 0){
        GLInit::endGLFW();
    }
}

void Application::createVAOs(const CreateVAOsInfo & info){
    if(info.count != 0){
        std::vector<GLuint> values;
        values.resize(info.count);
        glGenVertexArrays(info.count,&(values[0]));
        for(auto v : values){
            vaos.add(v);
        }
    }
}

void Application::createVBOs(const CreateVBOsInfo & info){
    if(info.count != 0){
        std::vector<GLuint> values;
        values.resize(info.count);
        glGenBuffers(info.count,&(values[0]));
        for(auto v : values){
            vbos.add(v);
        }
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

void Application::checkOpenGLError(Error* errs){
    Error &err = (errs ? *errs : defErr);
    GLint errc = glGetError();

    while(errc != GL_NO_ERROR){
        const char* errStr = (const char *)gluErrorString(errc);
        if(errStr){
            err.pushMessage({errc, errStr});
        }else{
            err.pushMessage({errc, "(GL_INTERNAL_ERROR)"});
        }
        errc = glGetError();  // 继续检查下一个错误
    }
}

void Application::getShaderProgramLog(Shader shader,std::string & logger){
    int len = 0,chWritten = 0;
    glGetProgramiv(shader.pid,GL_INFO_LOG_LENGTH,&len);
    if(len > 0){
        char * buf = new char[len+1];
        std::memset(buf,len+1,sizeof(char));
        glGetProgramInfoLog(shader.pid,len,&chWritten,buf);
        logger += buf;
        delete [] buf;
    }
}

void Application::getShaderShaderLog(GLuint shader,std::string & logger){
    int len = 0,chWritten = 0;
    glGetShaderiv(shader,GL_INFO_LOG_LENGTH,&len);
    if(len > 0){
        char * buf = new char[len+1];
        std::memset(buf,len+1,sizeof(char));
        glGetShaderInfoLog(shader,len,&chWritten,buf);
        logger += buf;
        delete [] buf;
    }
}

Shader Application::createShader(const CreateShaderInfo &info,Error * errs){
    Shader shader;
    GLuint vid = 0, fid = 0,gid = 0,cid = 0;
    GLint compile_status = 0;
    bool errored = false;
    std::string logv = "";

    Error & err = ((errs)?*errs:defErr);

    if(shaders.find(info.sid) != shaders.end()){
        err.pushMessage({AGEE_CONFLICT_SID,info.sid.c_str()});
        return shader;
    }

    if(info.vertex.compare("")){
        const char * buf[1] = {info.vertex.c_str()};
        vid = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vid,1,buf,NULL);
        glCompileShader(vid);

        checkOpenGLError(&err);
        glGetShaderiv(vid,GL_COMPILE_STATUS,&compile_status);
        if(compile_status != 1){
            getShaderShaderLog(vid,logv);
            err.pushMessage({AGEE_SHADER_FAILED_TO_COMPILE,logv.c_str()});
            logv = "";
            compile_status = 0;
            errored = true;
        }
    }
    if(!errored && info.fragment.compare("")){
        const char * buf[1] = {info.fragment.c_str()};
        fid = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fid,1,buf,NULL);
        glCompileShader(fid);

        checkOpenGLError(&err);
        glGetShaderiv(fid,GL_COMPILE_STATUS,&compile_status);
        if(compile_status != 1){
            getShaderShaderLog(fid,logv);
            err.pushMessage({AGEE_SHADER_FAILED_TO_COMPILE,logv.c_str()});
            logv = "";
            compile_status = 0;
            errored = true;
        }
    }
    if(!errored && info.geometry.compare("")){
        const char * buf[1] = {info.geometry.c_str()};
        gid = glCreateShader(GL_GEOMETRY_SHADER);
        glShaderSource(gid,1,buf,NULL);
        glCompileShader(gid);

        checkOpenGLError(&err);
        glGetShaderiv(gid,GL_COMPILE_STATUS,&compile_status);
        if(compile_status != 1){
            getShaderShaderLog(gid,logv);
            err.pushMessage({AGEE_SHADER_FAILED_TO_COMPILE,logv.c_str()});
            logv = "";
            compile_status = 0;
            errored = true;
        }
    }
    if(!errored && info.compute.compare("")){
        if(vid || gid || cid){
            err.pushMessage({AGEE_CONFLICT_SHADER,"You have already passed vertex/fragment/geometry shader to the program,which conflicts with compute shader!"});
        }else{
            const char * buf[1] = {info.compute.c_str()};
            cid = glCreateShader(GL_COMPUTE_SHADER);
            glShaderSource(cid,1,buf,NULL);
            glCompileShader(cid);

            checkOpenGLError(&err);
            glGetShaderiv(cid,GL_COMPILE_STATUS,&compile_status);
            if(compile_status != 1){
                getShaderShaderLog(cid,logv);
                err.pushMessage({AGEE_SHADER_FAILED_TO_COMPILE,logv.c_str()});
                logv = "";
                compile_status = 0;
                errored = true;
            }else shader.computeShader = true;
        }
    }

    if(errored){
        shader.reset();
        goto cleanup;
    }

    shader.pid = glCreateProgram();

    if(vid)glAttachShader(shader.pid,vid);
    if(fid)glAttachShader(shader.pid,fid);
    if(gid)glAttachShader(shader.pid,gid);
    if(cid)glAttachShader(shader.pid,cid);

    glLinkProgram(shader.pid);

    glGetProgramiv(shader.pid,GL_LINK_STATUS,&compile_status);
    if(compile_status != 1){
        getShaderProgramLog(shader,logv);
        err.pushMessage({AGEE_SHADER_FAILED_TO_LINK,logv.c_str()});
        shader.reset();
        goto cleanup;
    }
    shaders.emplace(info.sid,shader);

cleanup:
    if(vid)glDeleteShader(vid);
    if(gid)glDeleteShader(gid);
    if(cid)glDeleteShader(cid);
    if(fid)glDeleteShader(fid);

    return shader;
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
    static std::string buf = "";
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

    app.defErr.pushMessage({AGEE_OPENGL_DEBUG_MESSAGE,buf.c_str()});
}

Shader Application::getShader(const std::string & sid){
    auto iter = shaders.find(sid);
    if(iter != shaders.end()){
        return iter->second;
    }
    return Shader::null();
}

bool Application::destroyShader(const std::string & sid){
    auto sh = shaders.find(sid);
    if(sh != shaders.end()){
        sh->second.destroy();
        shaders.erase(sh);
        return true;
    }
    return false;
}
