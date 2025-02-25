#ifndef ALIB4_AHANDLE_HPP_INCLUDED
#define ALIB4_AHANDLE_HPP_INCLUDED
#include <alib-g4/ahandle.h>
#include <alib-g4/autil.h>
#include <string>
#include <memory_resource>
#include <unordered_map>
#include <memory>

namespace alib4{
    ///Only one ResourceManager is permitted!!!!
    struct ALIB4_API ResourceManager{
        ///String Pool
        AStrHandle allocateString(const std::string &data);
        int freeString(AStrHandle handle);
        const char * getString(AStrHandle handle);
        const char * str_add(AStrHandle a,AStrHandle b);
        const char * str_add(AStrHandle a,const char * b);
        size_t str_length(AStrHandle a);
        AStrHandle str_substr(AStrHandle a,size_t beg,size_t length);

        ResourceManager();

        std::pmr::synchronized_pool_resource pool;
        std::unordered_map<AStrHandle,std::shared_ptr<std::pmr::string>> strHandles;
        AStrHandle strHandleCounter;

        static ResourceManager resManager;
    };

	struct ALIB4_API StrHandle{
		StrHandle(int = 0);

		std::shared_ptr<std::pmr::string> operator *();

		AStrHandle handle;
	};
}

extern "C"{
	ALIB4_API std::shared_ptr<std::pmr::string> astr_getcpp(AStrHandle a);
}

#endif
