/** @file Application.h
 * @brief 应用，类似Vulkan的概念（但是目前用的是OpenGL）
 * @author aaaa0ggmc
 * @date 2025-6-11
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
#include "Window.h"
#include <AGE/Window.h>
#include <AGE/VAO.h>
#include <AGE/VBO.h>
#include <AGE/Shader.h>
#include <AGE/World/EntityManager.h>
#include <AGE/Texture.h>

#include <GL/glext.h>
#include <unordered_map>
#include <string>
#include <memory_resource>
#include <unordered_set>
#include <optional>

//Graphics begin from -10000 to -19999
#define AGEE_CONFLICT_SHADER -10000
#define AGEE_SHADER_LOG -10001
#define AGEE_SHADER_FAILED_TO_COMPILE -10002
#define AGEE_SHADER_FAILED_TO_LINK -10003
#define AGEE_OPENGL_DEBUG_MESSAGE -10004
#define AGEE_OPENGL_NO_CONTEXT -10005
#define AGEE_OPENGL_EMPTY_SHADER -10006
#define AGEE_TEXTURE_LOADED -10007

/// Aaaa0ggmc's Graphics Engine 我的图形引擎
namespace age{
    /** @struct GLInit
     * @brief 初始化OpenGL
     */
    struct AGE_API GLInit{
        GLInit() = delete;

        /// 初始化GLFW
        static void GLFW();
        /// 初始化 GLEW
        static void GLEW();

        /// 终止GLFW
        static void endGLFW();

        // 指示
        static bool inited_glfw;
        static bool inited_glew;
    };

    struct ConstantStringBuffer{
        std::pmr::unsynchronized_pool_resource upstreamPool;
        // 只能分配不能释放，对于短期字符串不适合存储
        std::pmr::monotonic_buffer_resource poolBuffer { &upstreamPool };
        std::pmr::polymorphic_allocator<char> allocator { &poolBuffer };
        std::pmr::unordered_set<std::pmr::string> pool{0, std::hash<std::pmr::string>{}, std::equal_to<std::pmr::string>{}, allocator};

        std::string_view get(std::string_view str){
            auto [it, inserted] = pool.emplace(std::pmr::string(str, allocator));
            return *it;
        }
    };


    // FIXED: 解耦这个类，这个类是个superclass,干的事情太多了
    // FIXED: left by aaaa0ggmc: 解耦个pee
    /** @struct Application
     * @brief 应用程序，尝试支持多窗口
     */
    class AGE_API Application{
    public:
        VAOManager vaos;
        VBOManager vbos;
        world::EntityManager& em; ///< EntityManager

        Application(world::EntityManager & emm); ///< 构造函数
        ~Application(); ///< 析构函数

        //// Window  ////
        /// 创建一个窗口
        std::optional<Window*> createWindow(const CreateWindowInfo & info);
        /// 通过SID获取窗口
        std::optional<Window*> getWindow(std::string_view sid);
        /// baby模式
        std::optional<Window*> createWindow(std::string_view sid,
                                            std::string_view title,
                                            unsigned int width,
                                            unsigned int height,
                                            int x,int y,WinStyle style,
                                            float fpsRestrict = 60);
        /// 销毁窗口，直接通过指针
        bool destroyWindow(Window * window);
        /// 通过SID销毁窗口
        bool destroyWindow(std::string_view);

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

        //// Shader ////
        /// 创建着色器
        [[nodiscard]] Shader createShader(const CreateShaderInfo & info);
        /// baby模式
        [[nodiscard]] Shader createShaderFromFile(std::string_view  sid,
                                                  std::string_view  fvert,
                                                  std::string_view  ffrag = "",
                                                  std::string_view  fgeom = "",
                                                  std::string_view  fcomp = "");
        [[nodiscard]] Shader createShaderFromSrc(std::string_view  sid,
                                                 std::string_view  vert = "",
                                                 std::string_view  frag = "",
                                                 std::string_view  geom = "",
                                                 std::string_view  comp = "");
        /// 通过sid获取着色器
        Shader getShader(std::string_view  sid);
        /// 获取着色器整体的log
        void getShaderProgramLog(Shader shader,std::string & logger);
        /// 获取着色器单体的log,用户一般不使用
        void getShaderShaderLog(GLuint shader,std::string & logger);
        /// 删除一个shader，如果一个shader处于bind状态，行为未定义，程序endup前谨慎使用
        bool destroyShader(std::string_view  sid);

        //// Texture ////
        std::optional<Texture*> createTexture(const CreateTextureInfo & info);
        Texture& uploadTextureToGL(Texture & texure);
        

        /// 设置OpenGL版本要求
        void setGLVersion(unsigned int major,unsigned int minor);

        /// 检测OpenGL出现的错误，默认使用Application自带的，(NULL)
        void checkOpenGLError();

        /// 设置OpenGL错误回显函数
        inline void setGLErrCallbackFunc(GLDEBUGPROC proc = glErrDefDebugProc,void * useParam = NULL){
            if(windows.size() == 0){
                Error::def.pushMessage({AGEE_OPENGL_NO_CONTEXT,"You havent created a window to activate OpenGL context."});
                return;
            }
            if(proc == glErrDefDebugProc){
                useParam = (void*)this;
            }
            glDebugMessageCallback(proc,useParam);
        }
        /// 设置OpenGL错误是否回显
        inline void setGLErrCallback(bool enable){
            if(windows.size() == 0){
                Error::def.pushMessage({AGEE_OPENGL_NO_CONTEXT,"You havent created a window to activate OpenGL context."});
                return;
            }
            if(enable)glEnable(GL_DEBUG_OUTPUT);
            else glDisable(GL_DEBUG_OUTPUT);
        }

        /// 默认错误回调，使用Error类
        static void GLAPIENTRY glErrDefDebugProc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);

    private:
        /// 窗口列表
        std::unordered_map<std::string_view,Window*> windows;
        std::unordered_map<std::string_view,Shader> shaders;
        /// Application实例数目，当counter减为0时自动释放GLFW
        /// 纹理表
        std::unordered_map<std::string_view,Texture*> textures;
        std::unordered_map<std::string_view,TextureInfo> texturesInfo;
        static unsigned int counter;
        /// 静态字符串常量池
        ConstantStringBuffer csbuffer;
    };
}
#endif
