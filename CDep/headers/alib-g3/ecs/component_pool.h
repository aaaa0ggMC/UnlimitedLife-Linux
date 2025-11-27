/**
 * @file component_pool.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 对象池
 * @version 0.1
 * @date 2025/11/27
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/11/27 
 */
#ifndef AECS_COMPPOOL_INCLUDED
#define AECS_COMPPOOL_INCLUDED
#include <alib-g3/autil.h>
#include <alib-g3/ecs/entity.h>
#include <alib-g3/ecs/linear_storage.h>
#include <unordered_map>

namespace alib::g3::ecs{
    namespace detail{
        /// @brief 每个池子对应的销毁基类
        struct PoolDestroyerBase{
            /// @brief 销毁函数，第一个为pool的指针，第二个为对应的entity_id
            /// @note  这里并不需要进行版本号管理，因为获取entity的时候不依赖版本号
            int (*destroyer)(void*,id_t);
        };
    }

    /// @brief 组件池
    /// @tparam T 组件类型
    template<class T> struct DLL_EXPORT ComponentPool : public detail::PoolDestroyerBase {
        /// @brief 具体的组件数据
        detail::LinearStorage<T> data;
        /// @brief 组件与entity id之间对应的映射
        std::unordered_map<id_t,size_t> mapper;

        /// @brief 初始化组件池
        /// @param pool_reserve_size 预留大小 
        inline ComponentPool(size_t pool_reserve_size = 0)
        :data(pool_reserve_size){
            destroyer = nullptr;
        }
    };
}

#endif