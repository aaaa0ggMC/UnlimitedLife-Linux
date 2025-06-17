#include <AGE/Window.h>
#include <GLFW/glfw3.h>

using namespace age;

Window * Window::current = NULL;

CreateWindowInfo::CreateWindowInfo(){
    sid = "default";
    windowTitle = "default";
    width = height = 600;
    x = y = 0;
    fps = 0;
    moniter = std::nullopt;
    share = std::nullopt;
    style = WinStyle::FollowGLFW;
}

Window::Window():
window{nullptr},
sid{""},
fpsLimiter{120},
s_current{nullptr}{

}

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
