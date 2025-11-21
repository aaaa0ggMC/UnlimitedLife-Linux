#ifndef AECS_COMPPOOL_INCLUDED
#define AECS_COMPPOOL_INCLUDED
#include <alib-g3/autil.h>
#include <alib-g3/ecs/entity.h>
#include <alib-g3/ecs/linear_storage.h>
#include <unordered_map>

namespace alib::g3::ecs{
    template<class T> struct DLL_EXPORT ComponentPool{
        // 保证offset为0
        int (*destroyer)(void*,id_t);
        detail::LinearStorage<T> data;
        std::unordered_map<id_t,size_t> mapper;

        inline ComponentPool(size_t pool_reserve_size = 0)
        :destroyer(nullptr)
        ,data(pool_reserve_size){}
    };
}

#endif