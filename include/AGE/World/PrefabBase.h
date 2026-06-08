#ifndef AGE_PREFAB_BASE_H
#define AGE_PREFAB_BASE_H
#include <AGE/Utils.h>
#include <alib5/aecs.h>
#include <utility>

namespace age::world{
    using namespace alib5::ecs;

    struct MonoBehavior : public NonCopyable{
        EntityManager& em;
        EntityWrapper entity;

        MonoBehavior(EntityManager & emm):
        em{emm},
        entity{emm}{};

        virtual ~MonoBehavior(){
            entity.destroy();
        }

        template<class T> auto get(){
            return entity.get<T>();
        }

        template<class T,class... Args> auto add(Args&&... args){
            return entity.add<T>(std::forward<Args>(args)...);
        }

        template<class... Types> auto adds(){
            return entity.adds<Types...>();
        }

        template<class T> void remove(){
            entity.remove<T>();
        }

        EntityWrapper& getEntityWrapper(){
            return entity;
        }

        Entity getEntity(){
            return entity.get_entity();
        }
    };
}

#endif