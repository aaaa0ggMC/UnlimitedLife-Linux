#ifndef AGE_VAO
#define AGE_VAO
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
        friend class VBOManager;
        GLuint id;
        uint32_t index;
    public:
        VAO(GLuint = AGE_NULL_OBJ,uint32_t = 0);

        inline void bind(){
            glBindVertexArray(id);
        }

        inline GLuint getId(){
            return id;
        }

        inline GLuint getManagerIndex(){
            return index;
        }

        static VAO null();
    };

    /** @struct VAOManager
     * @brief 管理VAO
     */
    struct AGE_API VAOManager{
    public:
        ///VAOS
        std::vector<GLuint> vaos;

        ///添加项目
        void add(GLuint id);

        ///置空
        void markAsFree(uint32_t index);

        ///获取
        VAO operator[](unsigned int index);
    };

    struct AGE_API CreateVAOsInfo{
        unsigned int count;
    };
}

#endif
