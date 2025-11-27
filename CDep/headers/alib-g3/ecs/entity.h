/**
 * @file entity.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 基础的实体
 * @version 0.1
 * @date 2025/11/27
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/11/27 
 */
#ifndef AECS_ENTITY_INCLUDED
#define AECS_ENTITY_INCLUDED
#include <alib-g3/autil.h>

namespace alib::g3::ecs{
    /// @brief 实体id的类型
    using id_t = uint64_t;

    /// @brief 实体类
    struct DLL_EXPORT Entity{
        /// @brief 实体的id
        id_t id; 
        /// @brief 实体的版本
        uint32_t version;

        /// @brief 默认的实体构造
        Entity(){
            id = version = 0;
        }

        /// @brief 包含id以及版本的构造
        Entity(id_t i_id,uint32_t i_version = 0){
            id = i_id;
            version = i_version;
        }

        /// @brief 复用实体，这个会增加版本号
        void reset(){
            version++;
        }

        /// @brief 语义区分创建一个空的实体
        /// @return 空实体
        static inline Entity null(){
            return Entity();
        }
    };

    //// 一些alias ////
    using entity = alib::g3::ecs::Entity;
    using entity_t = alib::g3::ecs::Entity;
}

#endif