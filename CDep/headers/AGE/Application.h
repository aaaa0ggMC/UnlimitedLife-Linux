#include <unordered_map>
#include <string>
#include <optional>
 
#include "Window.h"

namespace age{
    struct AGE_API GLInit{
        static void GLFW();
        static void GLEW();

        static void endGLFW();

        static bool inited_glfw;
        static bool inited_glew;
    };

    class AGE_API Application{
    public:
        Application();
        ~Application();

        std::optional<Window*> createWindow(CreateWindowInfo info);
        std::optional<Window*> getWindow(const std::string & sid);
        bool destroyWindow(Window * window);
        bool destroyWindow(const std::string & sid);

        void setGLVersion(unsigned int major,unsigned int minor);
    private:
        std::unordered_map<std::string,Window*> windows;

        static unsigned int counter;
    };
}
