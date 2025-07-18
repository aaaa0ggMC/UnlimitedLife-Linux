/**
 * @file Systems.h
 * @author aaaa0ggmc
 * @brief 提供一些预设的System插件
 * @version 0.1
 * @date 2025/07/18
 * @start-date 2025/7/16
 * @copyright Copyright (c) 2025
 */
#ifndef AGE_H_SYSTEMS

#include <AGE/Utils.h>
#include <AGE/World/EntityManager.h>
#include <AGE/World/Components.h>

namespace age::world{
    namespace systems{
        //@post-update
        /// @brief note that parent system must be a post update of your own transform
        struct AGE_API ParentSystem{
        private:
            EntityManager &em;
            ComponentPool<comps::Parent>* pool;
            ComponentPool<comps::Transform>* transformPool;

        public:
            inline ParentSystem(EntityManager & iem):em{iem}{
                initPool();
            }

            inline void initPool(){
                auto opt = em.getComponentPool<comps::Parent>();
                if(!opt)pool = nullptr;
                else pool = *opt;

                auto opt2 = em.getComponentPool<comps::Transform>();
                if(!opt2)transformPool = nullptr;
                else transformPool = *opt2;
            }

            void update();
        };


        //需要对Marker的comp限制，起码要有dm_mark才行
        template<typename T>
        concept ComponentMarkable = requires(T & t) {
            t.dm_mark();
        };

        /// @brief 直接把一个pool都标记了
        template<ComponentMarkable T> struct AGE_API DirtySystem{
            ComponentPool<T> * pool;
            EntityManager &em;

            inline DirtySystem(EntityManager & iem): em{iem}{
                initPool();    
            }

            inline void initPool(){
                auto opt = em.getComponentPool<T>();
                if(!opt)pool = nullptr;
                else pool = *opt;
            }

            inline void update(){
                if(!pool)return;
                if constexpr(std::is_same_v<T,comps::Transform> == true){
                    //额外处理
                    for(T & c: pool->data){
                        c.dm_mark();
                        c.m_rotation.dm_mark();
                    }
                }else{
                    for(T & c: pool->data){
                        c.dm_mark();
                    }
                }
            }
        };

        /// @brief 更加通用的Marker
        struct AGE_API GenericDirtySystem{
            EntityManager &em;

            inline GenericDirtySystem(EntityManager & iem): em{iem}{}

            template<ComponentMarkable T> inline void update(){
                auto opool = em.getComponentPool<T>();
                if(!opool)return;
                auto * pool = *opool;
                if constexpr(std::is_same_v<T,comps::Transform> == true){
                    //额外处理
                    for(T & c: pool->data){
                        c.dm_mark();
                        c.m_rotation.dm_mark();
                    }
                }else{
                    for(T & c: pool->data){
                        c.dm_mark();
                    }
                }
            }
        };
    }
}
#endif