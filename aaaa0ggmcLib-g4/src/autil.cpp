#include <alib-g4/autil.hpp>
#include <string>
#include <string.h>
#include <math.h>
#include <stdio.h>

static thread_local int ecode = AE_SUCCESS;
static thread_local std::string econtent = "";

extern "C" {

    void asetLastError(int code,const char * content){
        ecode = code;
        if(content)econtent = content;
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
			alib4::avformatStringf(econtent,fmt,val);
			va_end(val);
		}
	}
}

namespace alib4{
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

	int aformatStringf(std::pmr::string & tg,const char * fmt,...){
		if(!fmt){
			return AE_EMPTY_DATA;
		}
		va_list val;
		va_start(val,fmt);
		int ret = avformatStringf(tg,fmt,val);
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

	int avformatStringf(std::pmr::string & tg,const char * fmt,va_list ap){
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
}