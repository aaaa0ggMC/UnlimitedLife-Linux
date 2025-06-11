#ifndef AGE_VAO
#define AGE_VAO
#include "Base.h"
#include <AGE/Base.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <string>
#include <vector>

namespace age {
    /** @struct VAO
     * @brief OpenGL封装
     */
    struct AGE_API VAO{
    private:
        GLuint id;
    public:
        VAO(GLuint = AGE_NULL_OBJ);

        static VAO null_vao();
    };

    /** @struct VAOManager
     * @brief 管理VAO
     */
    struct AGE_API VAOManager{
    public:
        ///VAOS
        std::vector<VAO> vaos;

        ///获取
        VAO operator[](unsigned int index);
    };

    struct AGE_API CreateVAOsInfo{
        unsigned int count;
    };
}

#endif
