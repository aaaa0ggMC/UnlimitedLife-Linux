#include <alib-g4/autil.hpp>
#include <string>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <vector>
#include <iostream>

static thread_local int ecode = AE_SUCCESS;
static thread_local std::string econtent = "";
static std::vector<aErrorCallbackFn> callbacks = {};

extern "C" {

    void asetLastError(int code,const char * content){
        ecode = code;
        if(content)econtent = content;
		if(ecode != AE_SUCCESS){
			for(auto & tg : callbacks){
				tg(ecode,econtent.c_str(),nullptr);
			}
		}
    }

    aError agetLastError(){
        return {ecode,econtent.c_str()};
    }
	
	void aclearLastError(){
		asetLastError(AE_SUCCESS,"");
    }
	
	
	void asetLastErrorf(int code,const char * fmt,...){
		ecode = code;
		if(fmt){
			va_list val;
			va_start(val,fmt);
			avformatStringf(econtent,fmt,val);
			va_end(val);
		}
		if(ecode != AE_SUCCESS){
			for(auto & tg : callbacks){
				tg(ecode,econtent.c_str(),nullptr);
			}
		}
	}
	
	
	void aaddOnErrorCallback(aErrorCallbackFn fn,void * reserved){
		if(fn)callbacks.push_back(fn);
	}
	
	
	int aformatStringf(std::string& tg,const char * fmt,...){
		if(!fmt){
			return AE_EMPTY_DATA;
		}
		va_list val;
		va_start(val,fmt);
		int ret = avformatStringf(tg,fmt,val);
		va_end(val);
		return ret;
	}

	int aformatStringf_pmr(std::pmr::string & tg,const char * fmt,...){
		if(!fmt){
			return AE_EMPTY_DATA;
		}
		va_list val;
		va_start(val,fmt);
		int ret = avformatStringf_pmr(tg,fmt,val);
		va_end(val);
		return ret;
	}


	int avformatStringf(std::string& tg,const char * fmt,va_list ap){
		va_list cpy;
		va_copy(cpy,ap);
		tg.clear();
		tg.resize(tg.capacity());
		int ret = vsnprintf(&tg[0],tg.size()-1,fmt,ap);
		if(abs(ret) >= tg.size()){
			tg.clear();
			tg.resize(abs(ret) + 1);
			ret = vsnprintf(&tg[0],tg.size()-1,fmt,cpy);
		}
		va_end(cpy);
		return ret;
	}

	int avformatStringf_pmr(std::pmr::string & tg,const char * fmt,va_list ap){
		va_list cpy;
		va_copy(cpy,ap);
		tg.clear();
		tg.resize(tg.capacity());
		int ret = vsnprintf(&tg[0],tg.size()-1,fmt,ap);
		if(abs(ret) >= tg.size()){
			tg.clear();
			tg.resize(abs(ret) + 1);
			ret = vsnprintf(&tg[0],tg.size()-1,fmt,cpy);
		}
		va_end(cpy);
		return ret;
	}
	
	void* adefaultErrorCallback(int code,const char * ct,void*){
		std::cerr << "Alib4Error[CODE:" << code << "]:" << ((ct)?ct:"") << std::endl;
		return nullptr;
	}
}