#include <alib-g4/ahandle.hpp>
#include <climits>
#include <memory_resource>
#include <string>

using namespace alib4;

ResourceManager ResourceManager::resManager;

ResourceManager::ResourceManager(){
    //std::pmr::set_default_resource(&pool);
    strHandleCounter = 0;
}

AStrHandle ResourceManager::allocateString(const std::string& data){
    ++strHandleCounter;
    strHandles.emplace(strHandleCounter,std::make_shared<std::pmr::string>(data,&pool));
    return strHandleCounter;
}

int ResourceManager::freeString(AStrHandle handle){
    auto it = strHandles.find(handle);
    if(it == strHandles.end()){
        asetLastErrorf(AE_CANT_FIND,"astr_free->Cannot find the string handle [%u] in the string pool.",handle);
        return AE_CANT_FIND;
    }
    strHandles.erase(it);
	aclearLastError();
    return AE_SUCCESS;
}

const char * ResourceManager::getString(AStrHandle handle){
    auto it = strHandles.find(handle);
    if(it == strHandles.end()){
        asetLastErrorf(AE_CANT_FIND,"astr_get->Cannot find the string handle [%u] in the string pool.",handle);
        return nullptr;
    }
	aclearLastError();
    return it->second->c_str();
}

const char * ResourceManager::str_add(AStrHandle a,AStrHandle b){
    auto av = strHandles.find(a);
    if(av == strHandles.end()){
        asetLastErrorf(AE_CANT_FIND,"astr_add->Cannot find the first string handle [%u] in the string pool.",a);
        return nullptr;
    }
    auto bv = strHandles.find(b);
    if(bv == strHandles.end()){
        asetLastErrorf(AE_CANT_FIND,"astr_add->Cannot find the second string handle [%u] in the string pool.",b);
        return av->second->c_str();
    }

    (*av->second) += (*bv->second);
	aclearLastError();
    return av->second->c_str();
}

const char * ResourceManager::str_add(AStrHandle a,const char * b){
    auto av = strHandles.find(a);
    if(av == strHandles.end()){
        asetLastErrorf(AE_CANT_FIND,"astr_add_ptr->Cannot find the string handle [%u] in the string pool.",a);
        return nullptr;
    }
	aclearLastError();
    if(b)(*av->second) += b;
    return av->second->c_str();
}

size_t ResourceManager::str_length(AStrHandle a){
    auto av = strHandles.find(a);
    if(av == strHandles.end()){
        asetLastErrorf(AE_CANT_FIND,"astr_length->Cannot find the string handle [%u] in the string pool.",a);
        return 0;
    }
	aclearLastError();
    return av->second->length();
}

AStrHandle ResourceManager::str_substr(AStrHandle a,size_t beg,size_t length){
    auto av = strHandles.find(a);
    if(av == strHandles.end()){
        asetLastErrorf(AE_CANT_FIND,"astr_substr->Cannot find the string handle [%u] in the string pool.",a);
        return AINVALID_HANDLE;
    }
    return allocateString(av->second->substr(beg,length).c_str());
}

extern "C"{
    AStrHandle astr_allocate(const char * data){
		if(!data){
			asetLastErrorf(AE_EMPTY_DATA,"%s->Null is given to allocate a string in string pool.",__func__);
			return AE_EMPTY_DATA;
		}
		return ResourceManager::resManager.allocateString(data);
	}
	
    const char* astr_add(AStrHandle a,AStrHandle b){
		return ResourceManager::resManager.str_add(a,b);
	}
	
    const char* astr_add_ptr(AStrHandle a,const char * b){
		return ResourceManager::resManager.str_add(a,b);
	}
	
    size_t astr_length(AStrHandle a){
		return ResourceManager::resManager.str_length(a);
	}
	
    AStrHandle astr_substr(AStrHandle a,size_t beg,size_t length){
		return ResourceManager::resManager.str_substr(a,beg,length);
	}
	
    int astr_free(AStrHandle handle){
		return ResourceManager::resManager.freeString(handle);
	}

    const char * astr_get(AStrHandle handle){
		return ResourceManager::resManager.getString(handle);
	}
	
	
	std::shared_ptr<std::pmr::string> astr_getstring(AStrHandle a){
		auto it = ResourceManager::resManager.strHandles.find(a);
		if(it == ResourceManager::resManager.strHandles.end()){
			asetLastErrorf(AE_CANT_FIND,"%s->Cannot find the string handle [%u] in the string pool.",__func__,a);
			return nullptr;
		}
		return it->second;
	}
}
