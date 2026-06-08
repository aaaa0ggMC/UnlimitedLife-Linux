#ifndef AGE_MSHADER_H_INCLUDED
#define AGE_MSHADER_H_INCLUDED
#include <AGE/Utils.h>
#include <AGE/Shader.h>
#include <AGE/Details/CSBuffer.h>

namespace age::manager{
    struct AGE_API ShaderManager{
    private:
        std::unordered_map<std::string_view,Shader> shaders;
        detail::ConstantStringBuffer& csbuffer;
    public:
        ShaderManager(detail::ConstantStringBuffer & cb):csbuffer{cb}{}
        ~ShaderManager();

        /// 创建着色器
        [[nodiscard]] Shader create(const CreateShaderInfo & info);
        /// 通过sid获取着色器
        Shader get(std::string_view  sid);
        /// 删除一个shader，如果一个shader处于bind状态，行为未定义，程序endup前谨慎使用
        bool destroy(std::string_view sid);

        /// baby模式
        [[nodiscard]] Shader fromFile(std::string_view  sid,
                                                  std::string_view  fvert,
                                                  std::string_view  ffrag = "",
                                                  std::string_view  fgeom = "",
                                                  std::string_view  fcomp = "");
        [[nodiscard]] Shader fromSrc(std::string_view  sid,
                                                 std::string_view  vert = "",
                                                 std::string_view  frag = "",
                                                 std::string_view  geom = "",
                                                 std::string_view  comp = "");

        /// 获取着色器整体的log
        void getProgramLog(Shader shader,std::string & logger);
        /// 获取着色器单体的log,用户一般不使用
        void getShaderLog(GLuint shader,std::string & logger);

        inline bool has(std::string_view sid){
            return shaders.find(sid) != shaders.end();
        }

        inline bool destroy(Shader & sh){
            return destroy(sh.sid);
        }
    }; 
}

#endif