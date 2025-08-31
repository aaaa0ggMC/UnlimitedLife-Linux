#include <AGE/Application.h>
#include <GL/glext.h>
#include <cstring>
#include <GL/glu.h>
#include <AGE/Input.h>
#include <alib-g3/autil.h>
#include <stb/stb_image.h>

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

void Application::setGLVersion(unsigned int major,unsigned int minor){
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,minor);
}

std::optional<Window*> Application::createWindow(const CreateWindowInfo &info){
    Window * win = new Window();
    GLInit::GLFW();
    win->sid = csbuffer.get(info.sid);
    win->window = glfwCreateWindow(
                    info.width,info.height,info.windowTitle.c_str(),
                    (!info.moniter)?NULL:(*info.moniter),
                    (!info.share)?NULL:(*info.share)->window);
    if(win->window == NULL){
        delete win;
        return std::nullopt;
    }
    windows.emplace(win->sid,win);
    if(info.x >= 0 && info.y >= 0)glfwSetWindowPos(win->window,info.x,info.y);
    if(!(Window::current))win->makeCurrent();
    win->setFramerateLimit(info.fps);
    win->binder.bind((intptr_t)(win->window),(intptr_t)win);
    GLInit::GLEW();
    //bindings
    glfwSetWindowSizeCallback(win->window,[](GLFWwindow * win,int nw,int nh){
        Window & window = *((Window*)Window::binderArray.get<Window>((intptr_t)win));
        if(nw == 0 || nh == 0){
            //aspect ratio = 0,glm will not work
            return;
        }
        #ifdef __linux__
        auto scale = window.getContentScale();
        if(window.m_onResize)window.m_onResize(window,nw * scale.first,nh * scale.second);
        #elif defined(_WIN32)
        // Windows系统下，nw和nh已经是经过content scale处理的
        if(window.m_onResize)window.m_onResize(window,nw,nh);
        #endif
    });

    glfwSetKeyCallback(win->window,[](GLFWwindow * glfwWin,int key,int scancode,int action,int mods){
        age::KeyWrapper wrapper (glfwWin,key,scancode,action,mods);
        Window & window = *((Window*)Window::binderArray.get<Window>((intptr_t)glfwWin));
        if(window.m_onKey)window.m_onKey(window,wrapper);
    });

    // no vsync
    glfwSwapInterval(0);
    #ifdef __linux__
    glViewport(0, 0, info.width * win->getContentScale().first, info.height * win->getContentScale().second);
    #endif
    return {win};
}

std::optional<Window*> Application::createWindow(std::string_view sid,std::string_view title,int width,int height,int x,int y,WinStyle style,float fpsRestrict){
    CreateWindowInfo wi;
    wi.sid = sid;
    wi.windowTitle = title;
    wi.width = width;
    wi.height = height;
    wi.x = x;
    wi.y = y;
    wi.style = style;
    wi.fps = fpsRestrict;
    return createWindow(wi);
}

std::optional<Window*> Application::getWindow(std::string_view sid){
    auto v = windows.find(sid);
    if(v == windows.end())return std::nullopt;
    return {v->second};
}

bool Application::destroyWindow(std::string_view sid){
    auto v = windows.find(sid);
    if(v == windows.end())return false;
    Window * win = v->second;
    glfwDestroyWindow(win->window);
    delete win;
    windows.erase(v);//这里我能出错也是没谁了
    return true;
}

bool Application::destroyWindow(Window * win){
    if(!win)return false;
    return destroyWindow(win->sid);
}

Application::Application(world::EntityManager & emm):em{emm}{
    counter++;
    Error::def.setTrigger();
}

Application::~Application(){
    //cleanup
    //Step1:Shaders
    //std::cout << shaders.size() << std::endl;
    for(auto& [sid,shd] : shaders){
        shd.destroy();
    }
    //New Step: Textures
    for(auto& [_,tex] : textures){
        if(tex->texture_id){
            glDeleteTextures(1,&(tex->texture_id));
        }
    }
    for(auto& [_,info] : texturesInfo){
        if(!info.uploaded && info.bits){
            stbi_image_free(info.bits);
        }
    }
    //Samplers
    for(auto & [_,samp] : samplers){
        glDeleteSamplers(1,&samp.sampler_id);
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
            window_names.emplace_back(sid);
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

void Application::checkOpenGLError(){
    Error &err = Error::def;
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
        std::vector<char> buf(len+1,0);
        glGetProgramInfoLog(shader.pid,len,&chWritten,buf.data());
        logger.append(buf.begin(),buf.end());
    }
}

void Application::getShaderShaderLog(GLuint shader,std::string & logger){
    int len = 0,chWritten = 0;
    glGetShaderiv(shader,GL_INFO_LOG_LENGTH,&len);
    if(len > 0){
        std::vector<char> buf(len+1,0);
        glGetShaderInfoLog(shader,len,&chWritten,buf.data());
        logger.append(buf.begin(),buf.end());
    }
}


Shader Application::createShaderFromFile(std::string_view sid,
                                          std::string_view cfvert,
                                          std::string_view cffrag,
                                          std::string_view cfgeom,
                                          std::string_view cfcomp){
    std::string vert,frag,geom,comp;
    vert = frag = geom = comp = "";

    std::string fvert = "";
    fvert += cfvert;
    std::string ffrag = "";
    ffrag += cffrag;
    std::string fgeom = "";
    fgeom += cfgeom;
    std::string fcomp = "";
    fcomp += cfcomp;

    if(fvert.compare("")){
        Util::io_readAll(fvert,vert);
    }
    if(ffrag.compare("")){
        Util::io_readAll(ffrag,frag);
    }
    if(fgeom.compare("")){
        Util::io_readAll(fgeom,geom);
    }
    if(fcomp.compare("")){
        Util::io_readAll(fcomp,comp);
    }

    return createShaderFromSrc(sid,vert,frag,geom,comp);
}
Shader Application::createShaderFromSrc(std::string_view  sid,
                                         std::string_view  vert,
                                         std::string_view  frag,
                                         std::string_view  geom,
                                         std::string_view  comp){
    CreateShaderInfo si;
    si.sid = sid;
    si.vertex = vert;
    si.fragment = frag;
    si.compute = comp;
    return createShader(si);
}

Shader Application::createShader(const CreateShaderInfo &info){
    Shader shader;
    GLuint vid = 0, fid = 0,gid = 0,cid = 0;
    GLint compile_status = 0;
    bool errored = false;
    std::string logv;
    bool created = false;

    Error & err = Error::def;

    if(shaders.find(info.sid) != shaders.end()){
        err.pushMessage({AGEE_CONFLICT_SID,info.sid.c_str()});
        return shader;
    }

    if(info.vertex.compare("")){
        const char * buf[1] = {info.vertex.c_str()};
        vid = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vid,1,buf,NULL);
        glCompileShader(vid);

        checkOpenGLError();
        glGetShaderiv(vid,GL_COMPILE_STATUS,&compile_status);
        if(compile_status != 1){
            logv = "VertexShader:";
            getShaderShaderLog(vid,logv);
            err.pushMessage({AGEE_SHADER_FAILED_TO_COMPILE,logv.c_str()});
            compile_status = 0;
            errored = true;
        }else created = true;
    }
    if(!errored && info.fragment.compare("")){
        const char * buf[1] = {info.fragment.c_str()};
        fid = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fid,1,buf,NULL);
        glCompileShader(fid);

        checkOpenGLError();
        glGetShaderiv(fid,GL_COMPILE_STATUS,&compile_status);
        if(compile_status != 1){
            logv = "FragmentShader:";
            getShaderShaderLog(fid,logv);
            err.pushMessage({AGEE_SHADER_FAILED_TO_COMPILE,logv.c_str()});
            compile_status = 0;
            errored = true;
        }else created = true;
    }
    if(!errored && info.geometry.compare("")){
        const char * buf[1] = {info.geometry.c_str()};
        gid = glCreateShader(GL_GEOMETRY_SHADER);
        glShaderSource(gid,1,buf,NULL);
        glCompileShader(gid);

        checkOpenGLError();
        glGetShaderiv(gid,GL_COMPILE_STATUS,&compile_status);
        if(compile_status != 1){
            logv = "GeometryShader:";
            getShaderShaderLog(gid,logv);
            err.pushMessage({AGEE_SHADER_FAILED_TO_COMPILE,logv.c_str()});
            compile_status = 0;
            errored = true;
        }else created = true;
    }
    if(!errored && info.compute.compare("")){
        if(vid || gid || cid){
            err.pushMessage({AGEE_CONFLICT_SHADER,"You have already passed vertex/fragment/geometry shader to the program,which conflicts with compute shader!"});
        }else{
            const char * buf[1] = {info.compute.c_str()};
            cid = glCreateShader(GL_COMPUTE_SHADER);
            glShaderSource(cid,1,buf,NULL);
            glCompileShader(cid);

            checkOpenGLError();
            glGetShaderiv(cid,GL_COMPILE_STATUS,&compile_status);
            if(compile_status != 1){
                logv = "ComputeShader:";
                getShaderShaderLog(cid,logv);
                err.pushMessage({AGEE_SHADER_FAILED_TO_COMPILE,logv.c_str()});
                compile_status = 0;
                errored = true;
            }else {
                shader.computeShader = true;
                created = true;
            }
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
        logv = "ProgramLink:";
        getShaderProgramLog(shader,logv);
        err.pushMessage({AGEE_SHADER_FAILED_TO_LINK,logv.c_str()});
        shader.reset();
        goto cleanup;
    }
    if(!created){
        err.pushMessage({AGEE_OPENGL_EMPTY_SHADER,"The shader has no shader subprogram inside."});
    }
    shaders.emplace(csbuffer.get(info.sid),shader);

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

    Error::def.pushMessage({AGEE_OPENGL_DEBUG_MESSAGE,buf.c_str()});
}

Shader Application::getShader(std::string_view sid){
    auto iter = shaders.find(sid);
    if(iter != shaders.end()){
        return iter->second;
    }
    return Shader::null();
}

bool Application::destroyShader(std::string_view sid){
    auto sh = shaders.find(sid);
    if(sh != shaders.end()){
        sh->second.destroy();
        shaders.erase(sh);
        return true;
    }
    return false;
}

std::optional<Texture*> Application::createTexture(const CreateTextureInfo & info){
    if(textures.find(info.sid) != textures.end()){
        Error::def.pushMessage({AGEE_CONFLICT_SID,"Texture SID conflicts!!!"});
        return std::nullopt;
    }
    TextureInfo tinfo;
    const GLchar * buf = info.buffer.data;
    size_t eleC = info.buffer.eleCount;
    stbi_uc* bits;

    tinfo.hasbits = true;
    
    switch(info.source){
    case CreateTextureInfo::FromVector:
        buf = info.vec.data->data();
        eleC = info.vec.data->size();
    case CreateTextureInfo::FromBuffer:
        if(!buf){
            Error::def.pushMessage({AGEE_EMPTY_DATA,"Buffer passed to createTexture is empty!"});
            return std::nullopt;
        }
        bits = stbi_load_from_memory((const stbi_uc*)buf,eleC,&tinfo.width,&tinfo.height,&tinfo.channels,info.channel_desired);
        break;
    case CreateTextureInfo::FromFile:
        if(!info.file.path.compare("")){
            Error::def.pushMessage({AGEE_EMPTY_DATA,"The file path provided is empty!"});
            return std::nullopt;
        }
        bits = stbi_load(info.file.path.c_str(),&tinfo.width,&tinfo.height,&tinfo.channels,info.channel_desired);
        break;
    case CreateTextureInfo::CreateEmpty:
        bits = nullptr;
        tinfo.hasbits = false;
        break;
    default:{
            std::string em = "The source enum passed to createTexture is invalid: ";
            em += std::to_string((int)info.source);
            Error::def.pushMessage({AGEE_WRONG_ENUM,em.c_str()});
            break;
        }
    }
    //stbi自动填充，不知道会不会改channels(问了问Chat说不会改，保险点留着)
    tinfo.channels = (info.channel_desired != 0) ? info.channel_desired : tinfo.channels;
    tinfo.bits = bits;

    auto sid = csbuffer.get(info.sid);
    auto tex = textures.emplace(sid,new Texture());
    auto& tex_info = texturesInfo.emplace(sid,tinfo).first->second;
    Texture* texture = tex.first->second;
    texture->sid = sid;
    texture->textureInfo = &tex_info;
    tex_info.uploaded = false;
    tex_info.mipmap = info.genMipmap;

    if(info.uploadToOpenGL){
        //上传到OpenGL
        uploadImageToGL(*texture);
    }

    return texture;
}

Texture& Application::uploadImageToGL(Texture & texture){
    if(texture.textureInfo->uploaded){
        Error::def.pushMessage({AGEE_TEXTURE_LOADED,"The texture has already uploaded to OpenGL."});
        return texture;
    }
    glGenTextures(1,&texture.texture_id);
    if(!texture.texture_id){
        Error::def.pushMessage({AGEE_OPENGL_CREATE_ERROR,"Cant create a new texture!"});
        return texture;
    }
    if(!texture.textureInfo->hasbits)return texture; // 用户自己创建更多的东西
    //  由于hasbits的加入，这里要往后，逻辑才通顺
    if(!(texture.textureInfo->bits)){
        Error::def.pushMessage({AGEE_EMPTY_DATA,"The texture data is empty!"});
        return texture;
    }

    texture.textureInfo->uploaded = true;
    glBindTexture(GL_TEXTURE_2D,texture.texture_id);
    int internalFormat = GL_RGB;
    switch(texture.textureInfo->channels){
    case 1:
        internalFormat = GL_RED;
        break;
    case 2:
        internalFormat = GL_LUMINANCE_ALPHA;
        break;
    case 4:
        internalFormat = GL_RGBA;
        break;
    case 3:
        break;
    default:
        Error::def.pushMessage({AGEE_FEATURE_NOT_SUPPORTED,"File with channel count bigger than 4 or lower than 1 is unsupported!"});
        glBindTexture(GL_TEXTURE_2D,0);
        return texture;
    }
    texture.texImage2D(
        Texture::TexImageParams().
        internalformat(internalFormat).format(internalFormat).
        width(texture.textureInfo->width).height(texture.textureInfo->height).
        data(texture.textureInfo->bits)
    );
    if(texture.textureInfo->mipmap){
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    stbi_image_free(texture.textureInfo->bits);
    glBindTexture(GL_TEXTURE_2D,0);
    return texture;
}

std::optional<Sampler> Application::createSampler(std::string_view sid){
    auto rest = samplers.find(sid);
    if(rest != samplers.end())return rest->second;
    Sampler sampler;
    auto usid = csbuffer.get(sid);
    sampler.sid = usid;
    sampler.info = nullptr;
    glCreateSamplers(1,&sampler.sampler_id);
    if(!sampler.sampler_id){
        Error::def.pushMessage({AGEE_OPENGL_CREATE_ERROR,"Failed to create a new sampler!"});
        return std::nullopt; //空对象
    }
    auto samp = samplersInfo.emplace(usid,SamplerInfo()).first->second;
    sampler.info = &samp;
    samplers.emplace(usid,sampler);
    return sampler;
}


bool Application::destroySampler(std::string_view sid){
    auto it = samplers.find(sid);
    if(it == samplers.end()){
        std::string msg = "Cant find an existing sampler named \"";
        msg += sid;
        msg += "\"";
        Error::def.pushMessage({AGEE_CANT_FIND_SID,msg.c_str()});
        return false;
    }
    glDeleteSamplers(1,&(it->second.sampler_id));
    samplers.erase(it);
    auto info = samplersInfo.find(sid);
    if(info != samplersInfo.end())samplersInfo.erase(info);
    return true;
}

bool Application::destroySampler(Sampler & sampler){
    return destroySampler(sampler.sid);
}

std::optional<Sampler> Application::getSampler(std::string_view sid){
    auto sp = samplers.find(sid);
    if(sp == samplers.end()){
        Error::def.pushMessage({AGEE_CANT_FIND_SID,"Cannot find required sampler!"});
        return std::nullopt;
    }
    return sp->second;
}


std::optional<Texture*> Application::getTexture(std::string_view sid){
    auto tex = textures.find(sid);
    if(tex == textures.end()){
        Error::def.pushMessage({AGEE_CANT_FIND_SID,"Cannot find required texture!"});
        return std::nullopt;
    }
    return tex->second;
}

bool Application::destroyTexture(std::string_view sid){
    auto tex = textures.find(sid);
    if(tex == textures.end()){
        Error::def.pushMessage({AGEE_CANT_FIND_SID,"Cannot find required texture to delete!"});
        return false;
    }
    glDeleteTextures(1,&(tex->second->texture_id));
    textures.erase(tex);
    auto tinfo =  texturesInfo.find(sid);
    if(tinfo != texturesInfo.end())texturesInfo.erase(tinfo);
    return true;
}

bool Application::destroyTexture(Texture& tex){
    return destroyTexture(tex.sid);
}