#include <alib-g4/ahandle.hpp>
#include <climits>
#include <memory_resource>
#include <string>

using namespace alib4;

ResourceManager ResourceManager::resManager;

ResourceManager::ResourceManager(){
    std::pmr::set_default_resource(&pool);
    strHandleCounter = 0;
}

AStrHandle ResourceManager::allocateString(const std::string& data){
    ++strHandleCounter;
    strHandles.emplace(strHandleCounter,std::make_shared<std::pmr::string>(data));
    return strHandleCounter;
}

int ResourceManager::freeString(AStrHandle handle){
    auto it = strHandles.find(handle);
    if(it == strHandles.end()){
        std::string output = "Cannot find the string handle [";
        output += std::to_string(handle);
        output += "] in the string pool!";
        asetLastError(AE_CANT_FIND,output.c_str());
        return AE_CANT_FIND;
    }
    strHandles.erase(it);
    return AE_SUCCESS;
}

const char * ResourceManager::getString(AStrHandle handle){
    auto it = strHandles.find(handle);
    if(it == strHandles.end()){
        std::string output = "Cannot find the string handle [";
        output += std::to_string(handle);
        output += "] in the string pool!";
        asetLastError(AE_CANT_FIND,output.c_str());
        return nullptr;
    }
    return it->second->c_str();
}

const char * ResourceManager::str_add(AStrHandle a,AStrHandle b){
    auto av = strHandles.find(a);
    if(av == strHandles.end()){
        std::string output = "Cannot find the first string handle [";
        output += std::to_string(a);
        output += "] in the string pool!";
        asetLastError(AE_CANT_FIND,output.c_str());
        return nullptr;
    }
    auto bv = strHandles.find(b);
    if(bv == strHandles.end()){
        std::string output = "Cannot find the second string handle [";
        output += std::to_string(b);
        output += "] in the string pool!";
        asetLastError(AE_CANT_FIND,output.c_str());
        return av->second->c_str();
    }

    (*av->second) += (*bv->second);

    return av->second->c_str();
}

const char * ResourceManager::str_add(AStrHandle a,const char * b){
    auto av = strHandles.find(a);
    if(av == strHandles.end()){
        std::string output = "Cannot find the string handle [";
        output += std::to_string(a);
        output += "] in the string pool!";
        asetLastError(AE_CANT_FIND,output.c_str());
        return nullptr;
    }
    if(b)(*av->second) += b;

    return av->second->c_str();
}

size_t ResourceManager::str_length(AStrHandle a){
    auto av = strHandles.find(a);
    if(av == strHandles.end()){
        std::string output = "Cannot find the string handle [";
        output += std::to_string(a);
        output += "] in the string pool!";
        asetLastError(AE_CANT_FIND,output.c_str());
        return 0;
    }
    return av->second->length();
}

AStrHandle ResourceManager::str_substr(AStrHandle a,size_t beg,size_t length){
    auto av = strHandles.find(a);
    if(av == strHandles.end()){
        std::string output = "Cannot find the string handle [";
        output += std::to_string(a);
        output += "] in the string pool!";
        asetLastError(AE_CANT_FIND,output.c_str());
        return AINVALID_HANDLE;
    }
    return allocateString(av->second->substr(beg,length).c_str());
}

extern "C"{

}
