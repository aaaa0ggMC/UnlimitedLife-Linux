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
#ifndef AGE_APP
#define AGE_APP
#include "VAO.h"
#include "VBO.h"
#include <unordered_map>
#include <string>
#include <optional>
#include <AGE/Window.h>
#include <AGE/VAO.h>
#include <AGE/VBO.h>

/// Aaaa0ggmc's Graphics Engine 我的图形引擎
namespace age{
    /** @struct GLInit
     * @brief 初始化OpenGL
     */
    struct AGE_API GLInit{
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

    /** @struct Application
     * @brief 应用程序，尝试支持多窗口
     */
    class AGE_API Application{
    public:
        VAOManager vaos;
        VBOManager vbos;

        Application(); ///< 构造函数
        ~Application(); ///< 析构函数

        //// Window  ////
        /// 创建一个窗口
        std::optional<Window*> createWindow(CreateWindowInfo info);
        /// 通过SID获取窗口
        std::optional<Window*> getWindow(const std::string & sid);
        /// 销毁窗口，直接通过指针
        bool destroyWindow(Window * window);
        /// 通过SID销毁窗口
        bool destroyWindow(const std::string & sid);

        //// VAO & VBO ////
        VAOManager& createVAOs(CreateVAOsInfo info);
        VBOManager& createVBOs(CreateVBOsInfo info);

        /// 设置OpenGL版本要求
        void setGLVersion(unsigned int major,unsigned int minor);

    private:
        /// 窗口列表
        std::unordered_map<std::string,Window*> windows;
        /// Application实例数目，当counter减为0时自动释放GLFW
        static unsigned int counter;
    };
}
#endif
