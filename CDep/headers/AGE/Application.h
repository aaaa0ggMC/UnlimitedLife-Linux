/** @file Application.h
 * @brief 应用，类似Vulkan的概念（但是目前用的是OpenGL）
 * @author aaaa0ggmc
 * @date 2026/02/11
 * @start-date 2025/06/11
 * @version 3.1
 * @copyright copyright(C)2025
 ********************************
 @ **par 修改日志:                      *
 <table>
 <tr><th>时间       <th>版本         <th>作者          <th>介绍
 <tr><td>2025-6-11 <td>3.1          <th>aaaa0ggmc    <td>v0.1
 </table>
 ********************************
 */
#ifndef AGE_H_APP
#define AGE_H_APP
#include <AGE/VAO.h>
#include <AGE/VBO.h>
#include <AGE/Shader.h>
#include <AGE/Texture.h>
#include <AGE/Details/WindowManager.h>
#include <AGE/Details/TextureManager.h>
#include <AGE/Details/ShaderManager.h>
#include <AGE/Details/SamplerManager.h>
#include <AGE/Details/FramebufferManager.h>
#include <alib5/aecs.h>
#include <GL/glext.h>

/// Aaaa0ggmc's Graphics Engine 我的图形引擎
namespace age{
    using namespace alib5::ecs;
    using namespace age::manager;
    
    /** @struct Application
     * @brief 应用程序，尝试支持多窗口
     */
    class AGE_API Application{
    private:
        /// Application实例数目，当counter减为0时自动释放GLFW
        static unsigned int counter;
        /// 静态字符串常量池
        ConstantStringBuffer csbuffer;
    public:
        VAOManager vaos;
        VBOManager vbos;
        WindowManager windows { csbuffer };
        TextureManager textures { csbuffer };
        ShaderManager shaders { csbuffer };
        SamplerManager samplers { csbuffer };
        FramebufferManager framebuffers { csbuffer , textures , samplers };

        EntityManager& em;

        Application(EntityManager & emm); ///< 构造函数
        ~Application(); ///< 析构函数

        //// VAO & VBO ////
        void  createVAOs(const CreateVAOsInfo & info);
        /// baby mode
        void  createVAOs(uint32_t count);
        /// @note 需要注意的是VAO就是纯纯的递增的，所以index不会因为delete而变动
        VAO getVAO(uint32_t index);
        bool destroyVAO(VAO);
        void createVBOs(const CreateVBOsInfo & info);
        /// baby mode
        void  createVBOs(uint32_t count);
        /// @note 需要注意的是VBO就是纯纯的递增的，所以index不会因为delete而变动
        VBO getVBO(uint32_t index);
        bool destroyVBO(VBO);        

        /// 设置OpenGL版本要求
        inline void setGLVersion(unsigned int major,unsigned int minor){
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,major);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,minor);
        }

        /// 检测OpenGL出现的错误，默认使用Application自带的，(NULL)
        inline void checkOpenGLError(){
            Error::checkOpenGLError();
        }

        /// 设置OpenGL错误回显函数
        inline void setGLErrCallbackFunc(GLDEBUGPROC proc = glErrDefDebugProc,void * useParam = NULL){
            AGE_CHECK_GL_CONTEXT;
            if(proc == glErrDefDebugProc){
                useParam = (void*)this;
            }
            glDebugMessageCallback(proc,useParam);
        }
        /// 设置OpenGL错误是否回显
        inline void setGLErrCallback(bool enable){
            AGE_CHECK_GL_CONTEXT;
            if(enable)glEnable(GL_DEBUG_OUTPUT);
            else glDisable(GL_DEBUG_OUTPUT);
        }

        /// 默认错误回调，使用Error类
        static void GLAPIENTRY glErrDefDebugProc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
    };
}
#endif
