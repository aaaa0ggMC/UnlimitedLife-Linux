/**
 * @file Entity.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 基础的entity
 * @version 0.1
 * @date 2025/07/25
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/06/16 
 */
#ifndef AGE_H_ENTITY
#define AGE_H_ENTITY
#include <AGE/Utils.h>

#include <cstdint>
#include <optional>

namespace  age::world {

    struct AGE_API Entity{
        uint64_t id;
    };
}

#endif
