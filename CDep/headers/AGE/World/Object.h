/**
 * @file Object.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 对组件的封装，提供transform
 * @version 0.1
 * @date 2025/11/29
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/06/24 
 */
#ifndef AGE_H_OBJ
#define AGE_H_OBJ
#include <AGE/Utils.h>
#include <AGE/World/Components.h>
#include <cerrno>

namespace age::world {
    using namespace alib::g3::ecs;
    using namespace comps;
    struct AGE_API Object : public NonCopyable{
    public:
        EntityManager & em;
        EntityWrapper e;

        ref_t<comps::Transform> tran;
        
        inline Object(EntityManager & emm)
        :em{emm}
        ,e(emm)
        ,tran{e.add<Transform>()}{}

        inline Entity getEntity(){
            return e.get_entity();
        }

        inline EntityWrapper getEntityWrapper(){
            return e;
        }

        inline ~Object(){
            e.destroy();
        }

        inline Transform& transform(){return tran.get();}
    };
}

#endif
