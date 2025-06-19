#ifndef AGE_ENTITY
#define AGE_ENTITY
#include <AGE/Base.h>
#include <cstdint>
#include <optional>

namespace  age::world {

    struct AGE_API Entity{
        uint64_t id;
    };
}

#endif
