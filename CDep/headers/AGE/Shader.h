/** @file Shader.h
 * @brief Shader for OpenGL program
 */
#ifndef AGE_SHADER
#define AGE_SHADER
#include <AGE/Base.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace age {
    /** @struct ShaderUniform
     * @brief Used to upload values
     */
    struct AGE_API ShaderUniform{
    private:
        GLuint location;
        GLuint program;
        friend class Shader;
    public:
        inline GLuint getLocationId(){return location;}
        inline GLuint getProgramId(){return program;}

        inline ShaderUniform(GLuint locationV,GLuint programV):location{locationV},program{programV}{}

        inline void uploadi(GLint v){
            glProgramUniform1i(program,location,v);
        }

        inline void uploadui(GLuint v){
            glProgramUniform1ui(program,location,v);
        }

        inline void uploadb(GLboolean v){
            glProgramUniform1i(program,location,(int)v);
        }

        inline void upload1f(GLfloat v){
            glProgramUniform1f(program,location,v);
        }

        inline void upload2f(GLfloat x,GLfloat y){
            glProgramUniform2f(program,location,x,y);
        }

        inline void upload2f(GLfloat x,GLfloat y,GLfloat z){
            glProgramUniform3f(program,location,x,y,z);
        }

        inline void upload2f(GLfloat x,GLfloat y,GLfloat z,GLfloat w){
            glProgramUniform4f(program,location,x,y,z,w);
        }

        inline void uploadv1(const glm::vec1& v){
            glProgramUniform1fv(program,location,1,glm::value_ptr(v));
        }

        inline void uploadv2(const glm::vec2& v){
            glProgramUniform2fv(program,location,2,glm::value_ptr(v));
        }

        inline void uploadv3(const glm::vec3& v){
            glProgramUniform3fv(program,location,3,glm::value_ptr(v));
        }

        inline void uploadv4(const glm::vec4& v){
            glProgramUniform4fv(program,location,4,glm::value_ptr(v));
        }

        ///matrix
        inline void uploadmat2(const glm::mat2& m, GLboolean transpose = GL_FALSE) {
            glProgramUniformMatrix2fv(program, location, 1, transpose, glm::value_ptr(m));
        }

        inline void uploadmat3(const glm::mat3& m, GLboolean transpose = GL_FALSE) {
            glProgramUniformMatrix3fv(program, location, 1, transpose, glm::value_ptr(m));
        }

        inline void uploadmat4(const glm::mat4& m, GLboolean transpose = GL_FALSE) {
            glProgramUniformMatrix4fv(program, location, 1, transpose, glm::value_ptr(m));
        }

        //arrays
        inline void uploadarrf(const std::vector<float> & data){
            glProgramUniform1fv(program, location, data.size(), data.data());
        }

        inline void uploadarrf(const float * array,size_t count){
            glProgramUniform1fv(program, location, count, array);
        }

        template<size_t N>
        inline void uploadarrf(const float (&arr)[N]){
            glProgramUniform1fv(program, location, N, arr);
        }

        //sampler
        inline void uploadSampler(GLint textureUint){
            glProgramUniform1i(program, location, textureUint);
        }
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

        ShaderUniform getUniformByLocation(GLuint location){
            return ShaderUniform(location,pid);
        }

        ShaderUniform getUniformByName(const std::string & name){
            return ShaderUniform(glGetUniformLocation(pid,name.c_str()),pid);
        }

        ShaderUniform operator[](GLuint location){
            return getUniformByLocation(location);
        }

        ShaderUniform operator[](const std::string& name){
            return getUniformByName(name);
        }

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
