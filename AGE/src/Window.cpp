#ifdef _WIN32
#include <windows.h>
#endif

#include <AGE/Window.h>
#include <GLFW/glfw3.h>

using namespace age;

Window * Window::current = NULL;
BinderArray Window::binderArray;

Window::Window():
window{nullptr},
sid{""},
fpsLimiter{120},
cursorVisibility{true}{}

void Window::setStyle(WinStyle styles,bool enableV){
    if(ws_hasFlag(styles,age::WinStyle::FollowGLFW)){
        if(!enableV)return;
    }
    auto setAttribute = [this,enableV](int styleId){
        glfwSetWindowAttrib(this->window,styleId,enableV?GLFW_TRUE:GLFW_FALSE);
    };
    if(ws_hasFlag(styles,age::WinStyle::Resizable))setAttribute(GLFW_RESIZABLE);
    if(ws_hasFlag(styles,age::WinStyle::Visible))setAttribute(GLFW_VISIBLE);
    if(ws_hasFlag(styles,age::WinStyle::Decorated))setAttribute(GLFW_DECORATED);
    if(ws_hasFlag(styles,age::WinStyle::Floating))setAttribute(GLFW_FLOATING);
    if(ws_hasFlag(styles,age::WinStyle::FocusOnShow))setAttribute(GLFW_FOCUS_ON_SHOW);
    if(ws_hasFlag(styles,age::WinStyle::TransparentFrameBuffer))setAttribute(GLFW_TRANSPARENT_FRAMEBUFFER);
    if(ws_hasFlag(styles,age::WinStyle::ScaleToMonitor))setAttribute(GLFW_SCALE_TO_MONITOR);
    if(ws_hasFlag(styles,age::WinStyle::CenterCursor))setAttribute(GLFW_CENTER_CURSOR);
    if(ws_hasFlag(styles,age::WinStyle::Focused))setAttribute(GLFW_FOCUSED);
    if(ws_hasFlag(styles,age::WinStyle::AutoIconify))setAttribute(GLFW_AUTO_ICONIFY);

    if(ws_hasFlag(styles,age::WinStyle::Maximized)){
        if(enableV)glfwRestoreWindow(window);
        else glfwMaximizeWindow(window);
    }
}

std::pair<int, int> CreateWindowInfo::ScreenPercent(float px, float py, int *x, int * y) {
    int sx,sy;
    sx = 1920;
    sy = 1280;
    float scale = getMonitorScale();
    #if defined(_WIN32)
        sx = GetSystemMetrics(SM_CXSCREEN) * scale;
        sy = GetSystemMetrics(SM_CYSCREEN) * scale;
    #elif defined(__linux__)
        glfwInit();
        glfwGetMonitorWorkarea(glfwGetPrimaryMonitor(), nullptr, nullptr, &sx, &sy);
    #endif //其他平台在其他地方都过不了这里就别想了
    sx *= px;
    sy *= py;
    if(x)*x = sx;
    if(y)*y = sy;
    return std::make_pair(sx,sy);
}
