#include <AGE/Details/WindowManager.h>
#include <AGE/Details/GLInit.h>

using namespace age::manager;
using namespace age;

WindowManager::~WindowManager(){
    std::vector<std::string> window_names;
    for(auto& [sid,_] : windows){
        window_names.emplace_back(sid);
    }

    for(auto & sid : window_names){
        destroy(sid);
    }
}

bool WindowManager::destroy(std::string_view sid){
    auto v = windows.find(sid);
    if(v == windows.end())return false;
    Window * win = v->second;
    glfwDestroyWindow(win->window);
    delete win;
    windows.erase(v);//这里我能出错也是没谁了
    return true;
}

std::optional<Window*> WindowManager::create(const CreateWindowInfo & info){
    Window * win = new Window();
    GLInit::GLFW();
    win->sid = csbuffer.get(info.sid);
    win->window = glfwCreateWindow(
                    info.width,info.height,info.windowTitle.c_str(),
                    (!info.moniter)?NULL:(*info.moniter),
                    (!info.share)?NULL:(*info.share)->window);
    if(win->window == NULL){
        delete win;
        return std::nullopt;
    }
    windows.emplace(win->sid,win);
    if(info.x >= 0 && info.y >= 0)glfwSetWindowPos(win->window,info.x,info.y);
    if(!(Window::current))win->makeCurrent();
    win->setFramerateLimit(info.fps);
    win->binder.bind((intptr_t)(win->window),(intptr_t)win);
    GLInit::GLEW();
    //bindings
    glfwSetWindowSizeCallback(win->window,[](GLFWwindow * win,int nw,int nh){
        Window & window = *((Window*)Window::binderArray.get<Window>((intptr_t)win));
        if(nw == 0 || nh == 0){
            //aspect ratio = 0,glm will not work
            return;
        }
        #ifdef __linux__
        auto scale = window.getContentScale();
        if(window.m_onResize)window.m_onResize(window,nw * scale.first,nh * scale.second,nw,nh);
        #elif defined(_WIN32)
        // Windows系统下，nw和nh已经是经过content scale处理的
        if(window.m_onResize)window.m_onResize(window,nw,nh);
        #endif
    });

    glfwSetKeyCallback(win->window,[](GLFWwindow * glfwWin,int key,int scancode,int action,int mods){
        age::KeyWrapper wrapper (glfwWin,key,scancode,action,mods);
        Window & window = *((Window*)Window::binderArray.get<Window>((intptr_t)glfwWin));
        if(window.m_onKey)window.m_onKey(window,wrapper);
    });

    // no vsync
    glfwSwapInterval(0);
    #ifdef __linux__
    glViewport(0, 0, info.width * win->getContentScale().first, info.height * win->getContentScale().second);
    #endif
    return {win};
}

std::optional<Window*> WindowManager::get(std::string_view sid){
    auto v = windows.find(sid);
    if(v == windows.end())return std::nullopt;
    return {v->second};
}
