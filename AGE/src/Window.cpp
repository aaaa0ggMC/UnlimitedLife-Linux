#ifdef _WIN32
#include <windows.h>
#include <shellscalingapi.h>
#endif

#include <AGE/Window.h>
#include <GLFW/glfw3.h>

using namespace age;

Window * Window::current = NULL;
BinderArray Window::binderArray;

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
fpsLimiter{120}{}

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
    #if defined(_WIN32)
        float scale = 1.0f; // 默认 100% 缩放
        HMONITOR hMonitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
        DEVICE_SCALE_FACTOR scaleFactor;
        if(SUCCEEDED(GetScaleFactorForMonitor(hMonitor, &scaleFactor))){
            scale = static_cast<float>(scaleFactor) / 100.0f; // 转换为浮点数 (例如 125 → 1.25f)
        }
        sx = GetSystemMetrics(SM_CXSCREEN) * scale;
        sy = GetSystemMetrics(SM_CYSCREEN) * scale;
    #elif defined(__linux__)
        //@todo linux上我还不知道具体怎么获取屏幕数据，暂定 1920 * 1280
        Error::def.pushMessage({AGEE_FEATURE_NOT_SUPPORTED,"ScreenPercent is unsupported now for linux platform...So default is 1920*1280"});    
    #endif //其他平台在其他地方都过不了这里就别想了
    sx *= px;
    sy *= py;
    if(x)*x = sx;
    if(y)*y = sy;
    return std::make_pair(sx,sy);
}
