/**
 * @file VAO.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief VAO
 * @version 0.1
 * @date 2025/07/25
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/06/13 （左右）
 */
#ifndef AGE_H_VAO
#define AGE_H_VAO
#include <AGE/Utils.h>
#include <AGE/VBO.h>

#include <GL/glew.h>
#include <GL/gl.h>
#include <cstring>
#include <string>
#include <vector>

namespace age {
    /** @struct VAO
     * @brief OpenGL封装
     */
    struct AGE_API VAO{
    private:
        friend class VAOManager;
        GLuint id;
        uint32_t index;


        static GLuint current;
    public:
        struct ScopedVAO{
            GLuint old;
            bool valid;
            inline ScopedVAO(const VAO & v){
                if(v.id == VAO::current)valid = false;
                else{
                    valid = true;
                    old = VAO::current;
                    VAO(v.id).bind();
                }
            }

            inline ~ScopedVAO(){
                if(valid){
                    VAO(old,0).bind();
                }
            }
        };

        VAO(GLuint = AGE_NULL_OBJ,uint32_t = 0);

        inline void bind(){
            current = id;
            glBindVertexArray(id);
        }

        inline static void unbind(){
            current = 0;
            glBindVertexArray(0);
        }

        inline GLuint getId(){
            return id;
        }

        inline GLuint getManagerIndex(){
            return index;
        }

        inline static GLuint getCurrent(){
            return current;
        }

        inline void setAttribute(const VBO & vbo,GLuint bindLocation,GLint numbersOfComponents,GLenum type,GLboolean shouldBeNormalized = GL_FALSE,GLsizei stride = 0,GLuint offset = 0){
            VAO::ScopedVAO scp_vao(*this);
            VBO::ScopedVBO scp(vbo);
            glVertexAttribPointer(bindLocation,numbersOfComponents,type,shouldBeNormalized,stride,(void*)(intptr_t)offset);
        }

        inline void setAttribStatus(GLuint bindLocation,bool enableAttrib){
            ScopedVAO scp(*this);
            if(enableAttrib) glEnableVertexAttribArray(bindLocation);
            else glDisableVertexAttribArray(bindLocation);
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
        std::vector<GLuint> freeList;

        ///添加项目
        void add(GLuint id);

        ///置空
        void markAsFree(uint32_t index);

        ///获取
        VAO operator[](unsigned int index);

    private:
        friend class Application;
        void alloc(unsigned int size);
    };

    struct AGE_API CreateVAOsInfo{
        unsigned int count;
        unsigned int reserved; //不这样有bug
    };
}

#endif
