/**
 * @file Shader.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 着色器
 * @version 0.1
 * @date 2025/12/02 （左右）
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/07/25 
 */
#ifndef AGE_H_SHADER
#define AGE_H_SHADER
#include <AGE/Utils.h>

#include <GL/glew.h>
#include <GL/gl.h>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace age::manager{
    class ShaderManager;
}
namespace age {
    template<class T>
    inline constexpr bool always_false_v = false;
    
    /** @struct ShaderUniform
     * @brief Used to upload values
     */
    struct AGE_API ShaderUniform{
    private:
        // location竟然是glint!!!
        GLint location;
        GLuint program;
        friend class Shader;
    public:

        inline GLint getLocationId(){return location;}
        inline GLuint getProgramId(){return program;}

        inline ShaderUniform(GLint locationV = -1,GLuint programV = 0):location{locationV},program{programV}{}

        inline bool isInvalid(){
            return location == -1 || program == 0;
        }

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

        inline void upload3f(GLfloat x,GLfloat y,GLfloat z){
            glProgramUniform3f(program,location,x,y,z);
        }

        inline void upload4f(GLfloat x,GLfloat y,GLfloat z,GLfloat w){
            glProgramUniform4f(program,location,x,y,z,w);
        }

        // aaaa0ggmc说： 操你妈aaaa0ggmc,tmd你怎么不测试这四个函数，搞出了这妥实
        /* origin : inline void uploadv3(const glm::vec3& v){
            glProgramUniform3fv(program,location,3,glm::value_ptr(v));
        }*/
        inline void uploadv1(const glm::vec1& v){
            upload1f(v.x);
        }

        inline void uploadv2(const glm::vec2& v){
            upload2f(v.x,v.y);
        }

        inline void uploadv3(const glm::vec3& v){
            upload3f(v.x,v.y,v.z);
        }

        inline void uploadv4(const glm::vec4& v){
            upload4f(v.x,v.y,v.z,v.w);
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

        template<class T> inline void upload(const T & val){
            if constexpr (std::is_same_v<T, GLint>){
                uploadi(val);
            }else if constexpr (std::is_same_v<T, GLuint>){
                uploadui(val);
            }else if constexpr (std::is_same_v<T, GLboolean>){
                uploadb(val);
            }else if constexpr (std::is_same_v<T, GLfloat>){
                upload1f(val);
            }else if constexpr (std::is_same_v<T, glm::vec1>){
                uploadv1(val);
            }else if constexpr (std::is_same_v<T, glm::vec2>){
                uploadv2(val);
            }else if constexpr (std::is_same_v<T, glm::vec3>){
                uploadv3(val);
            }else if constexpr (std::is_same_v<T, glm::vec4>){
                uploadv4(val);
            }else if constexpr (std::is_same_v<T, glm::mat2>){
                uploadmat2(val);
            }else if constexpr (std::is_same_v<T, glm::mat3>){
                uploadmat3(val);
            }else if constexpr (std::is_same_v<T, glm::mat4>){
                uploadmat4(val);
            }else if constexpr (std::is_same_v<T, std::vector<float>>){
                uploadarrf(val);
            }else if constexpr (std::is_same_v<T, std::vector<const float*>>){
                static_assert(!std::is_same_v<T, const float*>, "You need to pass the array's count when passing const float *");
            }else {
                static_assert(always_false_v<T>, "Unsupported type passed to ShaderUniform::upload()");
            }
        }

        inline void upload(const float * data,size_t count){
            uploadarrf(data,count);
        }

        template<size_t N>
        inline void upload(const float (&arr)[N]){
            glProgramUniform1fv(program, location, N, arr);
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
    private:
        std::string_view sid;
    public:
        inline std::string_view get_sid(){return sid;}
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

        ShaderUniform getUniformByName(std::string_view name){
            return ShaderUniform(glGetUniformLocation(pid,name.data()),pid);
        }

        ShaderUniform operator[](GLuint location){
            return getUniformByLocation(location);
        }

        ShaderUniform operator[](std::string_view name){
            return getUniformByName(name);
        }

    private:
        friend class age::manager::ShaderManager;

        //prevent user handy creation
        Shader();

        void reset();

        GLuint pid; ///< shader program id

        //statuses
        bool computeShader;
    };
}

#endif
