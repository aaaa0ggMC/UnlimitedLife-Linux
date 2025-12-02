#include <AGE/Details/FramebufferManager.h>

using namespace age;
using namespace age::manager;

FramebufferManager::~FramebufferManager(){
    // 析构manager不需要考虑太多的效率
    for(auto&[_,v] : framebuffers){
        glDeleteFramebuffers(1,&v.framebuffer_id);
    }
}

Framebuffer FramebufferManager::create(const CreateFramebufferInfo & info){
    panic_debug(info.sid.empty(),"Cannot pass empty sid to create function!");
    auto sh_it = framebuffers.find(info.sid);
    if(sh_it != framebuffers.end()){
        panicf_debug(true,"Conflict framebuffer sid[{}]!",info.sid);
        Error::def.pushMessage({AGEE_CONFLICT_SID,"Framebuffer SID conflicts!!!"});
        return { sh_it->second };
    }
    Framebuffer fb;
    glCreateFramebuffers(1,&fb.framebuffer_id);
    fb.sid = csbuffer.get(info.sid);

    if(!info.colors.size()){
        fb.colors(info.colors,info.base_index,0);
    }
    if(!info.depth.empty()){
        fb.depth(info.depth,0);
    }
    if(!info.stencil.empty()){
        fb.stencil(info.stencil,0);
    }

    framebuffers.emplace(fb.sid,fb);
    return fb;
}

std::optional<Framebuffer> FramebufferManager::get(std::string_view sid){
    panic_debug(sid.empty(),"Cannot pass empty sid to function!");
    auto fb = framebuffers.find(sid);
    if(fb == framebuffers.end()){
        Error::def.pushMessage({AGEE_CANT_FIND_SID,"Cannot find required framebuffer!"});
        return std::nullopt;
    }
    return fb->second;
}

bool FramebufferManager::destroy(std::string_view sid){
    panic_debug(sid.empty(),"Cannot pass empty sid to function!");
    auto fb = framebuffers.find(sid);
    if(fb == framebuffers.end()){
        Error::def.pushMessage({AGEE_CANT_FIND_SID,"Cannot find required framebuffer to delete!"});
        return false;
    }
    glDeleteFramebuffers(1,&(fb->second.framebuffer_id));
    framebuffers.erase(fb);
    return true;
}