#ifndef AGE_EM
#define AGE_EM
#include <AGE/World/Entity.h>
#include <AGE/Base.h>
#include <AGE/Components.h>
#include <cstdint>
#include <unordered_map>
#include <memory>

namespace age::world{
    struct AGE_API EntityManager{
    public:
        using namespace comps;

        std::unordered_map<uint64_t,std::shared_ptr<Basic>> components;

        //initial value is 0,so entity id won't be 0
        uint64_t id_max;

        EntityManager();

        inline Entity createEntity(){
            return Entity(++id_max,*this);
        }

        inline Entity getEntity(uint64_t id){
            //deleted entity may be invalid
            if(id > id_max)return Entity(0,*this);
            return Entity(id,*this);
        }

        void destroyEntity(const Entity& e);

    };
}

#endif
