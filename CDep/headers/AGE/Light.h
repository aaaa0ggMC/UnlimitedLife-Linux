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