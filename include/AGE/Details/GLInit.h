#ifndef AGE_INIT_GL_H_INCLUDED
#define AGE_INIT_GL_H_INCLUDED
#include <AGE/Utils.h>

namespace age{
    /** @struct GLInit
     * @brief 初始化OpenGL
     */
    struct AGE_API GLInit{
        GLInit() = delete;

        /// 初始化GLFW
        static void GLFW();
        /// 初始化 GLEW
        static void GLEW();

        /// 终止GLFW
        static void endGLFW();

        /// 指示
        static bool inited_glfw;
        static bool inited_glew;
    };
}

#endif