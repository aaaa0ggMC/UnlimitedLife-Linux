#ifndef AGE_H_VBO
#define AGE_H_VBO
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
            bool valid;
            inline ScopedVBO(const VBO & v){
                if(v.id == VBO::current)valid = false;
                else{
                    valid = true;
                    old = VBO::current;
                    VBO(v.id).bind();
                }
            }

            inline ~ScopedVBO(){
                if(valid){
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

        inline static GLuint getCurrent(){
            return current;
        }

        template<class T> inline void bufferData(const std::vector<T>& data,GLenum usageHint = GL_STATIC_DRAW){
            ScopedVBO scp(*this);
            if(data.empty())return;
            glBufferData(GL_ARRAY_BUFFER,sizeof(T) * data.size(),data.data(),usageHint);
        }

        template<class T> inline void bufferData(const T * data,size_t elementCount,GLenum usageHint = GL_STATIC_DRAW){
            ScopedVBO scp(*this);
            if(data == NULL)return;
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
