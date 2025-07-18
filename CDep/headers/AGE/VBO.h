#ifndef AGE_H_VBO
#define AGE_H_VBO
#include <AGE/Utils.h>

#include <GL/glew.h>

#include <cstring>
#include <span>
#include <iostream>

namespace age {
    enum class PrimitiveType : GLenum {
        Points = GL_POINTS,
        Lines = GL_LINES,
        LineLoop = GL_LINE_LOOP,
        LineStrip = GL_LINE_STRIP,
        Triangles = GL_TRIANGLES,
        TriangleStrip = GL_TRIANGLE_STRIP,
        TriangleFan = GL_TRIANGLE_FAN,
        //WIP: 下面的我自己都还不知道是什么
        LinesAdjacency = GL_LINES_ADJACENCY,
        LineStripAdjacency = GL_LINE_STRIP_ADJACENCY,
        TrianglesAdjacency = GL_TRIANGLES_ADJACENCY,
        TriangleStripAdjacency = GL_TRIANGLE_STRIP_ADJACENCY,
        Patches = GL_PATCHES
    };

    struct AGE_API VBOStat{
    public:
        size_t eleCount;
        uint16_t typeSize;

        template<PrimitiveType type> size_t primCount(){
            int64_t ret = 0;
            static_assert(type != PrimitiveType::Patches, "Patches require explicit patch vertex count setup");
            if constexpr(type == PrimitiveType::Points) ret = eleCount;
            else if constexpr(type == PrimitiveType::Lines) ret = eleCount / 2;
            else if constexpr(type == PrimitiveType::LineLoop) ret = eleCount;
            else if constexpr(type == PrimitiveType::LineStrip) ret = eleCount - 1;
            else if constexpr(type == PrimitiveType::Triangles) ret = eleCount / 3;
            else if constexpr(type == PrimitiveType::TriangleStrip) ret = eleCount - 2;
            else if constexpr(type == PrimitiveType::TriangleFan) ret = eleCount - 2;
            else if constexpr(type == PrimitiveType::LinesAdjacency) ret = eleCount / 4;
            else if constexpr(type == PrimitiveType::LineStripAdjacency) ret = eleCount - 3;
            else if constexpr(type == PrimitiveType::TrianglesAdjacency) ret =eleCount / 6;
            else if constexpr(type == PrimitiveType::TriangleStripAdjacency) ret =(eleCount - 4) / 2;
            if(ret < 0)ret = 0;
            return ret;
        }
    };

    /** @struct VBO
     * @brief OpenGL封装
     */
    struct AGE_API VBO{
    private:
        friend class VBOManager;
        friend class VAO;
        GLuint id;
        uint32_t index;

        static GLuint current;
    public:
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

        VBO(GLuint = AGE_NULL_OBJ,uint32_t = 0);

        inline GLuint getId(){
            return id;
        }

        inline GLuint getManagerIndex(){
            return index;
        }

        inline void bind(GLenum target = GL_ARRAY_BUFFER){
            current = this->id;
            glBindBuffer(target,id);
        }

        // 为了支持element buffer
        inline static void unbind(GLenum target = GL_ARRAY_BUFFER){
            current = 0;
            glBindBuffer(target,0);
        }

        inline static GLuint getCurrent(){
            return current;
        }

        template<class T> inline VBOStat bufferData(const std::vector<T>& data,GLenum usageHint = GL_STATIC_DRAW){
            ScopedVBO scp(*this);
            if(data.empty())return {0,0};
            glBufferData(GL_ARRAY_BUFFER,sizeof(T) * data.size(),data.data(),usageHint);
            return VBOStat{data.size(),sizeof(T)};
        }

        template<class T> inline VBOStat bufferData(const T * data,size_t elementCount,GLenum usageHint = GL_STATIC_DRAW){
            ScopedVBO scp(*this);
            if(data == NULL)return {0,0};
            glBufferData(GL_ARRAY_BUFFER,sizeof(T) * elementCount,data,usageHint);
            return VBOStat{elementCount,sizeof(T)};
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

    private:
        friend class Application;
        void alloc(unsigned int size);
    };

    struct AGE_API CreateVBOsInfo{
        unsigned int count;
        unsigned int reserved;
    };
}

#endif
