/**
 * @file Object.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 对组件的封装，提供transform
 * @version 0.1
 * @date 2026/02/11
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/06/24 
 */
#ifndef AGE_H_OBJ
#define AGE_H_OBJ
#include <AGE/Utils.h>
#include <AGE/World/Components.h>
#include <AGE/World/PrefabBase.h>

namespace age::world {
    using namespace alib5::ecs;
    using namespace comps;
    struct AGE_API Object : public MonoBehavior{
    public:
        ref_t<comps::Transform> tran;
        
        inline Object(EntityManager & emm)
        :MonoBehavior(emm){
            tran = add<Transform>();
        }
        
        inline Transform& transform(){return tran.get();}
    };
}

#endif
