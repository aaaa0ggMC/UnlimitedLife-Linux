#include <AGE/Application.h>

using namespace age;

bool GLInit::inited_glew = false;
bool GLInit::inited_glfw = false;
unsigned int Application::counter = 0;

void GLInit::GLEW(){
    if(!inited_glew){
        inited_glew = true;
        glewInit();
    }
}

void GLInit::GLFW(){
    if(!inited_glfw){
        inited_glfw = true;
        glfwInit();
    }
}

void GLInit::endGLFW(){
    if(inited_glfw){
        inited_glfw = false;
        glfwTerminate();
    }
}

void Application::setGLVersion(unsigned int major,unsigned int minor){
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,minor);
}

std::optional<Window*> Application::createWindow(CreateWindowInfo info){
    Window * win = new Window();
    GLInit::GLFW();
    win->SID = info.SID;
    win->window = glfwCreateWindow(
                    info.width,info.height,info.windowTitle.c_str(),
                    (!info.moniter)?NULL:(*info.moniter),
                    (!info.share)?NULL:(*info.share)->window);
    if(win->window == NULL){
        delete win;
        return std::nullopt;
    }
    windows.emplace(info.SID,win);
    if(info.x >= 0 && info.y >= 0)glfwSetWindowPos(win->window,info.x,info.y);
    if(!(Window::current))win->makeCurrent();
    GLInit::GLEW();
    return {win};
}

std::optional<Window*> Application::getWindow(const std::string & sid){
    auto v = windows.find(sid);
    if(v == windows.end())return std::nullopt;
    return {v->second};
}

bool Application::destroyWindow(const std::string & sid){
    auto v = windows.find(sid);
    if(v == windows.end())return false;
    Window * win = v->second;
    glfwDestroyWindow(win->window);
    delete win;
    windows.erase(sid);
    return true;
}

bool Application::destroyWindow(Window * win){
    if(!win)return false;
    return destroyWindow(win->SID);
}

Application::Application(){
    counter++;
}

Application::~Application(){
    counter--;
    if(counter == 0){
        GLInit::endGLFW();
    }
}
