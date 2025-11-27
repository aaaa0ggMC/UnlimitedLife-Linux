#ifndef AECS_COMPPOOL_INCLUDED
#define AECS_COMPPOOL_INCLUDED
#include <alib-g3/autil.h>
#include <alib-g3/ecs/entity.h>
#include <alib-g3/ecs/linear_storage.h>
#include <unordered_map>

namespace alib::g3::ecs{
    namespace detail{
        struct PoolDestroyerBase{
            int (*destroyer)(void*,id_t);
        };
    }

    template<class T> struct DLL_EXPORT ComponentPool : public detail::PoolDestroyerBase {
        detail::LinearStorage<T> data;
        std::unordered_map<id_t,size_t> mapper;

        inline ComponentPool(size_t pool_reserve_size = 0)
        :data(pool_reserve_size){
            destroyer = nullptr;
        }
    };
}

#endif