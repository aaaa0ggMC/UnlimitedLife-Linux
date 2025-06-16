#ifndef AGE_VBO
#define AGE_VBO
#include "Base.h"
#include <AGE/Base.h>
#include <GL/glew.h>

namespace age {
    /** @struct VBO
     * @brief OpenGL封装
     */
    struct VBO{
    private:
        friend class VBOManager;
        friend class VAO;
        GLuint id;
        uint32_t index;

        static GLuint current;
        struct ScopedVBO{
            GLuint old;
            inline ScopedVBO(const VBO & v){
                old = (v.id == VBO::current)?0:VBO::current;
                if(old){ // 这里还可以防止VBO::current为NULL
                    VBO(v.id,0).bind();
                }
            }

            inline ~ScopedVBO(){
                if(old){
                    VBO(old,0).bind();
                }
            }
        };
    public:
        VBO(GLuint = AGE_NULL_OBJ,uint32_t = 0);

        inline GLuint getId(){
            return id;
        }

        inline GLuint getManagerIndex(){
            return index;
        }

        inline void bind(){
            current = this->id;
            glBindBuffer(GL_ARRAY_BUFFER,id);
        }

        inline static void unbind(){
            current = 0;
            glBindBuffer(GL_ARRAY_BUFFER,0);
        }

        template<class T> inline void bufferData(const std::vector<T>& data,GLenum usageHint = GL_STATIC_DRAW){
            ScopedVBO scp(*this);
            if(data.empty())return;
            glBufferData(GL_ARRAY_BUFFER,sizeof(T) * data.size(),data.data(),usageHint);
        }

        template<class T> inline void bufferData(const T * data,size_t elementCount,GLenum usageHint = GL_STATIC_DRAW){
            ScopedVBO scp(*this);
            if(data.empty())return;
            glBufferData(GL_ARRAY_BUFFER,sizeof(T) * elementCount,data,usageHint);
        }

        static VBO null();
    };

    /** @struct VBOManager
     * @brief 管理VBO
     */
    struct AGE_API VBOManager{
    public:
        ///VAOS
        std::vector<GLuint> vbos;

        ///添加项目
        void add(GLuint id);

        ///置空
        void markAsFree(uint32_t index);

        ///获取
        VBO operator[](unsigned int index);
    };

    struct AGE_API CreateVBOsInfo{
        unsigned int count;
    };
}

#endif
