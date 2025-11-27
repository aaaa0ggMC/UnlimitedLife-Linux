/**
 * @file Systems.h
 * @author aaaa0ggmc
 * @brief 提供一些预设的System插件
 * @version 0.1
 * @date 2025/11/27
 * @start-date 2025/7/16
 * @copyright Copyright (c) 2025
 */
#ifndef AGE_H_SYSTEMS
#include <AGE/Utils.h>
#include <AGE/World/Components.h>

namespace age::world{
    using namespace alib::g3::ecs;
    namespace systems{
        //@post-update
        /// @brief note that parent system must be a post update of your own transform
        struct AGE_API ParentSystem{
        private:
            EntityManager &em;
            ComponentPool<comps::Parent>& pool;
            ComponentPool<comps::Transform>& transformPool;

        public:
            inline ParentSystem(EntityManager & iem)
            :em{iem}
            ,pool{iem.get_or_create_pool<comps::Parent>()}
            ,transformPool{iem.get_or_create_pool<comps::Transform>()}{}

            void update();
        };


        //需要对Marker的comp限制，起码要有dm_mark才行
        template<typename T>
        concept ComponentMarkable = requires(T & t) {
            t.dm_mark();
        };

        /// @brief 直接把一个pool都标记了
        template<ComponentMarkable T> struct AGE_API DirtySystem{
            EntityManager & em;
            ComponentPool<T> & pool;

            inline DirtySystem(EntityManager & iem)
            :em{iem}
            ,pool{iem.get_or_create_pool<T>()}{}

            inline void update(){
                pool.data.available_bits.for_each_skip_1_bits(
                    [this](size_t index){
                        auto & target = this->pool.data[index];
                        if constexpr(requires(T&t){
                            t.system_dm_mark();
                        }){
                            target.system_dm_mark();
                        }else{
                            target.dm_mark();
                        }
                    }
                ,pool.data.size());
            }
        };

        /// @brief 更加通用的Marker
        struct AGE_API GenericDirtySystem{
            EntityManager &em;

            inline GenericDirtySystem(EntityManager & iem): em{iem}{}

            template<ComponentMarkable T> inline void update(){
                auto opool = em.get_component_pool_unsafe<T>();
                if(!opool)return;
                auto &pool = *opool;
                pool.data.available_bits.for_each_skip_1_bits(
                    [&pool](size_t index){
                        auto & target = pool.data[index];
                        if constexpr(requires(T&t){
                            t.system_dm_mark();
                        }){
                            target.system_dm_mark();
                        }else{
                            target.dm_mark();
                        }
                    }
                ,pool.data.size());
            }
        };
    }
}
#endif