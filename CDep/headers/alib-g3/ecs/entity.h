#ifndef AECS_ENTITY_INCLUDED
#define AECS_ENTITY_INCLUDED
#include <alib-g3/autil.h>

namespace alib::g3::ecs{
    using id_t = uint64_t;

    struct DLL_EXPORT Entity{
        id_t id;
        uint32_t version;

        Entity(){
            id = version = 0;
        }

        Entity(id_t i_id,uint32_t i_version = 0){
            id = i_id;
            version = i_version;
        }

        void reset(){
            version++;
        }

        static inline Entity null(){
            return Entity();
        }
    };

    using entity = alib::g3::ecs::Entity;
    using entity_t = alib::g3::ecs::Entity;
}

#endif