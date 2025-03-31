#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <string>
#include <optional>

#include "Base.h"

namespace age{

    class AGE_API Window{
    private:
        friend class Application;
        GLFWwindow *window;
        static Window * current;

        std::string SID;
    public:
        inline static void setSwapInterval(int value){
            glfwSwapInterval(value);
        }
        inline static void pollEvents(){
            glfwPollEvents();
        }

        inline bool ShouldClose(){
           return glfwWindowShouldClose(window)!=0;
        }
        inline void swapBuffers(){
            glfwSwapBuffers(window);
        }
        inline void makeCurrent(){
            current = this;
            glfwMakeContextCurrent(window);
        }
    };

    struct AGE_API CreateWindowInfo{
        std::string SID;

        std::string windowTitle;
        unsigned int width;
        unsigned int height;
        int x;
        int y;

        std::optional<GLFWmonitor*> moniter;
        std::optional<Window*> share;
    };
}

