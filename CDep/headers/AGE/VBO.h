#ifndef AGE_VBO
#define AGE_VBO
#include <AGE/Base.h>

namespace age {
    /** @struct VBO
     * @brief OpenGL封装
     */
    struct VBO{
    private:
        friend class VBOManager;
        GLuint id;
        uint32_t index;
    public:
        VBO(GLuint = AGE_NULL_OBJ,uint32_t = 0);

        inline GLuint getId(){
            return id;
        }

        inline GLuint getManagerIndex(){
            return index;
        }

        //WIP
        inline void bind(){

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
