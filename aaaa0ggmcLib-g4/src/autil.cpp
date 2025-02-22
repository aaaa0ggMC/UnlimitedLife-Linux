#include <alib-g4/autil.hpp>
#include <string>

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
}
