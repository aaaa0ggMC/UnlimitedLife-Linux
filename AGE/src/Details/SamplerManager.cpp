#include <AGE/Details/SamplerManager.h>

using namespace age;
using namespace age::manager;

SamplerManager::~SamplerManager(){
    for(auto & [_,samp] : samplers){
        glDeleteSamplers(1,&samp.sampler_id);
    }
}

/// 由于sampler属性可以动态调整，所以只需要sid
std::optional<Sampler> SamplerManager::create(std::string_view sid){
    panic_debug(sid.empty(),"Cannot pass empty sid to create function!");
    auto rest = samplers.find(sid);
    if(rest != samplers.end()){
        panicf_debug(true,"Conflict sampler sid[{}]!",sid);
        Error::def.pushMessage({AGEE_CONFLICT_SID,"Sampler SID conflicts!!!"});
        return rest->second;
    }
    Sampler sampler;
    auto usid = csbuffer.get(sid);
    sampler.sid = usid;
    sampler.info = nullptr;
    glCreateSamplers(1,&sampler.sampler_id);
    if(!sampler.sampler_id){
        Error::def.pushMessage({AGEE_OPENGL_CREATE_ERROR,"Failed to create a new sampler!"});
        return std::nullopt; //空对象
    }
    auto& samp = samplersInfo.emplace(usid,SamplerInfo()).first->second;
    sampler.info = &samp;
    samplers.emplace(usid,sampler);
    return sampler;
}

bool SamplerManager::destroy(std::string_view sid){
    panic_debug(sid.empty(),"Cannot pass empty sid to create function!");
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

std::optional<Sampler> SamplerManager::get(std::string_view sid){
    panic_debug(sid.empty(),"Cannot pass empty sid to create function!");
    auto sp = samplers.find(sid);
    if(sp == samplers.end()){
        Error::def.pushMessage({AGEE_CANT_FIND_SID,"Cannot find required sampler!"});
        return std::nullopt;
    }
    return sp->second;
}