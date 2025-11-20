#ifndef AECS_ENTITY_INCLUDED
#define AECS_ENTITY_INCLUDED
#include <alib-g3/autil.h>

namespace alib::g3::ecs{
    using id_t = uint64_t;

    struct DLL_EXPORT Entity{
        id_t id;
    };

    using entity = alib::g3::ecs::Entity;
    using entity_t = alib::g3::ecs::Entity;
}

#endif