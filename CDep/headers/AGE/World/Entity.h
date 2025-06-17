#ifndef AGE_ENTITY
#define AGE_ENTITY
#include "EntityManager.h"
#include <AGE/Base.h>
#include <cstdint>
#include <optional>
// 没力了，entity与component的逻辑处理耦合就耦合吧，至少避免了循环依赖header
#include <AGE/World/Components.h>

namespace  age::world {

    struct EntityManager;

    struct AGE_API Entity{
        uint64_t id;
    };
}

#endif
