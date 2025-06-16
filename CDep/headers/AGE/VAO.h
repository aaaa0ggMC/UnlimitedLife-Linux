#ifndef AGE_VAO
#define AGE_VAO
#include <AGE/Base.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <string>
#include <vector>
#include <AGE/VBO.h>

namespace age {
    /** @struct VAO
     * @brief OpenGL封装
     */
    struct AGE_API VAO{
    private:
        friend class VAOManager;
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

        inline void setAttribute(const VBO & vbo,GLuint bindLocation,GLint numbersOfComponents,GLenum type,GLboolean shouldBeNormalized = GL_FALSE,GLsizei stride = 0,GLuint offset = 0){
            VBO::ScopedVBO scp(vbo);
            glVertexAttribPointer(bindLocation,numbersOfComponents,type,shouldBeNormalized,stride,(void*)(intptr_t)offset);
        }

        inline void setAttribStaus(GLuint bindLocation,bool enableAttrib){
            if(!enableAttrib) glDisableVertexAttribArray(bindLocation);
            else glEnableVertexAttribArray(bindLocation);
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
