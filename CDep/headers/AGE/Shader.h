/** @file Shader.h
 * @brief Shader for OpenGL program
 */
#ifndef AGE_SHADER
#define AGE_SHADER
#include <AGE/Base.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <string>

namespace age {
    /** @struct ShaderUniform
     * @brief Used to upload values
     */
    struct AGE_API ShaderUniform{
    public:
    private:
        GLuint id;

        friend class Shader;
    };

    /** @struct CreateShaderInfo
     * @brief leave "" to skip the shader
     * @note compute shader conflicts with the original pipeline,the legacy pipeline is prioritized when conflicting
     */
    struct AGE_API CreateShaderInfo{
        std::string sid;

        std::string vertex;
        std::string fragment;
        std::string geometry;

        //conflicts with above
        std::string compute;

        CreateShaderInfo();
    };

    struct AGE_API Shader{
    public:
        ///bind shader
        inline Shader& bind(){
            glUseProgram(pid);
            return *this;
        }

        inline void destroy(){
            if(pid)glDeleteProgram(pid);
            reset();
        }

        static Shader null();

        ShaderUniform getUniformByLocation(GLuint location);

    private:
        friend class Application;

        //prevent user handy creation
        Shader();

        void reset();

        GLuint pid; ///< shader program id

        //statuses
        bool computeShader;
    };
}

#endif
