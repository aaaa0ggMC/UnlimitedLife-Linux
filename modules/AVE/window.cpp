module ave.window;

import ave;
using namespace ave;

/// 这里需要自动终止 glfw
struct AutoFix{
    ~AutoFix(){
        if(GLFWManager::inited()){
            GLFWManager::terminate();
        }
    }
} __auto__fix;