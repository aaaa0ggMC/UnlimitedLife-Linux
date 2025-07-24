/**
 * @file Light.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 光照支持
 * @version 0.1
 * @date 2025/07/24
 *
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/07/24 
 */
#ifndef AGE_H_LIGHT
#define AGE_H_LIGHT
#include <AGE/Utils.h>
#include <AGE/Shader.h>

namespace age{
    namespace light{
        /// @brief Light,a bunch of cpu data
        struct AGE_API Light{
            glm::vec3 position;
            glm::vec3 color;

            
        };
    }
}


#endif