/**
 * @file Object.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 对组件的封装，提供transform
 * @version 0.1
 * @date 2025/07/25
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/06/24 
 */
#ifndef AGE_H_OBJ
#define AGE_H_OBJ
#include <AGE/Utils.h>
#include <AGE/World/Components.h>
#include <AGE/World/EntityManager.h>
#include <cerrno>

namespace age::world {
    using namespace comps;
    struct AGE_API Object : public NonCopyable{
    private:
        ComponentWrapper<comps::Transform> tran;
        EntityManager & em;
        EntityWrapper e;
    public:
        inline Object(EntityManager & emm):em{emm},
        e{emm.createEntity(),emm}{
            e.add<Transform>();
            tran.build(emm,e.e.id);
        }

        inline Entity getEntity(){
            return e.e;
        }

        inline EntityWrapper getEntityWrapper(){
            return e;
        }

        inline ~Object(){
            e.destroy();
        }

        inline Transform& transform(){return *tran;}
    };
}

#endif
