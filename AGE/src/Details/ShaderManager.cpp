#include <AGE/Details/ShaderManager.h>
#include <alib5/autil.h>

using namespace age;
using namespace alib5;
using namespace age::manager;

ShaderManager::~ShaderManager(){
    for(auto& [sid,shd] : shaders){
        shd.destroy();
    }
}

Shader ShaderManager::create(const CreateShaderInfo & info){
    panic_debug(info.sid.empty(),"Cannot pass empty sid to create function!");
    auto sh_it = shaders.find(info.sid);
    if(sh_it != shaders.end()){
        panicf_debug(true,"Conflict shader sid[{}]!",info.sid);
        Error::def.pushMessage({AGEE_CONFLICT_SID,"Shader SID conflicts!!!"});
        return { sh_it->second };
    }
    
    Shader shader;
    GLuint vid = 0, fid = 0,gid = 0,cid = 0;
    GLint compile_status = 0;
    bool errored = false;
    std::string logv;
    bool created = false;

    Error & err = Error::def;
    shader.sid = info.sid;

    if(info.vertex.compare("")){
        const char * buf[1] = {info.vertex.c_str()};
        vid = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vid,1,buf,NULL);
        glCompileShader(vid);

        Error::checkOpenGLError();
        glGetShaderiv(vid,GL_COMPILE_STATUS,&compile_status);
        if(compile_status != 1){
            logv = "VertexShader:";
            getShaderLog(vid,logv);
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

        Error::checkOpenGLError();
        glGetShaderiv(fid,GL_COMPILE_STATUS,&compile_status);
        if(compile_status != 1){
            logv = "FragmentShader:";
            getShaderLog(fid,logv);
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

        Error::checkOpenGLError();
        glGetShaderiv(gid,GL_COMPILE_STATUS,&compile_status);
        if(compile_status != 1){
            logv = "GeometryShader:";
            getShaderLog(gid,logv);
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

            Error::checkOpenGLError();
            glGetShaderiv(cid,GL_COMPILE_STATUS,&compile_status);
            if(compile_status != 1){
                logv = "ComputeShader:";
                getShaderLog(cid,logv);
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
        getProgramLog(shader,logv);
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

Shader ShaderManager::get(std::string_view sid){
    panic_debug(sid.empty(),"Cannot pass empty sid to function!");
    auto iter = shaders.find(sid);
    if(iter != shaders.end()){
        return iter->second;
    }
    return Shader::null();
}

bool ShaderManager::destroy(std::string_view sid){
    panic_debug(sid.empty(),"Cannot pass empty sid to function!");
    auto sh = shaders.find(sid);
    if(sh != shaders.end()){
        sh->second.destroy();
        shaders.erase(sh);
        return true;
    }
    return false;
}


Shader ShaderManager::fromFile(std::string_view  sid,
                std::string_view  cfvert,
                std::string_view  cffrag,
                std::string_view  cfgeom,
                std::string_view  cfcomp){
    std::string vert,frag,geom,comp;
    vert = frag = geom = comp = "";

    if(cfvert.compare("")){
        io::read_all(cfvert,vert);
    }
    if(cffrag.compare("")){
        io::read_all(cffrag,frag);
    }
    if(cfgeom.compare("")){
        io::read_all(cfgeom,geom);
    }
    if(cfcomp.compare("")){
        io::read_all(cfcomp,comp);
    }

    return fromSrc(sid,vert,frag,geom,comp);
}

Shader ShaderManager::fromSrc(std::string_view  sid,
                std::string_view  vert,
                std::string_view  frag,
                std::string_view  geom,
                std::string_view  comp){
    CreateShaderInfo si;
    si.sid = sid;
    si.vertex = vert;
    si.fragment = frag;
    si.compute = comp;
    return create(si);
}

void ShaderManager::getProgramLog(Shader shader,std::string & logger){
    int len = 0,chWritten = 0;
    glGetProgramiv(shader.pid,GL_INFO_LOG_LENGTH,&len);
    if(len > 0){
        std::vector<char> buf(len+1,0);
        glGetProgramInfoLog(shader.pid,len,&chWritten,buf.data());
        logger.append(buf.begin(),buf.end());
    }
}

void ShaderManager::getShaderLog(GLuint shader,std::string & logger){
    int len = 0,chWritten = 0;
    glGetShaderiv(shader,GL_INFO_LOG_LENGTH,&len);
    if(len > 0){
        std::vector<char> buf(len+1,0);
        glGetShaderInfoLog(shader,len,&chWritten,buf.data());
        logger.append(buf.begin(),buf.end());
    }
}