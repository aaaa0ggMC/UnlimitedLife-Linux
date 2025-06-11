#ifndef AGE_VBO
#define AGE_VBO
#include "Base.h"
#include <AGE/Base.h>
#include <AGE/VAO.h>

namespace age {
    /** @struct VBO
     * @brief OpenGL封装
     */
    struct VBO{
    private:
        GLuint id;
    public:
        VBO(GLuint = AGE_NULL_OBJ);

        static VBO null_vbo();
    };

    /** @struct VBOManager
     * @brief 管理VBO
     */
    struct AGE_API VBOManager{
    public:
        ///VAOS
        std::vector<VBO> vbos;

        ///获取
        VBO operator[](unsigned int index);
    };

    struct AGE_API CreateVBOsInfo{
        unsigned int count;
    };
}

#endif
