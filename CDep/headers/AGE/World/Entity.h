#ifndef AGE_ENTITY
#define AGE_ENTITY
#include <AGE/World/EntityManager.h>
#include <AGE/Base.h>
#include <cstdint>
#include <optional>

namespace  age::world {

    struct AGE_API Entity{
        using namespace comps;
        uint64_t id;
        EntityManager& em;
        //typeid
        std::unordered_map<uint64_t,uint64_t> components;

        template<class T,class... Ts> T& addComponent(Ts&&... args){

        }

        template<class T> T& getComponent(){

        }

    private:
        friend class EntityManager;

        Entity(uint64_t,EntityManager&);
    };
}

#endif
