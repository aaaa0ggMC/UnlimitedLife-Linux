/**
 * @file Systems.h
 * @author aaaa0ggmc
 * @brief 提供一些预设的System插件
 * @version 0.1
 * @date 2025/07/16
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
    }
}
#endif