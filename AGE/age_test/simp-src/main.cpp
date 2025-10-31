#include <AGE/Window.h>
#include <AGE/Application.h>

using namespace alib::g3;
using namespace age;

int main(){
    Logger logger;
    LogFactory lg("SIMP",logger);
    world::EntityManager entity_manager;
    Application app(entity_manager);
    Window *win;

    logger.appendLogOutputTarget("console",std::make_shared<lot::Console>());

    lg.info("Creating window...");
    app.setGLVersion(4,5);
    {
        CreateWindowInfo ci;
        ci.sid = "SimpTest";
        ci.windowTitle = "SimpTest-测试";
        ci.width = 800;
        ci.height = 600;
        ci.x = 100;
        ci.y = 100;
        ci.style = WinStylePresetNormal;
        ci.fps = 0;
        ci.ScreenPercent(0.5,1,&ci.width,&ci.height);
        ci.KeepRatio(ci.width,ci.height,800,600);
        ci.ScreenPercent(0.2,0.2,&ci.x,&ci.y);

        if(!app.createWindow(ci)){
            lg.error("Failed to create window,now exit...");
            exit(-1);
        }else win = *app.getWindow("SimpTest");
    }

    win->makeCurrent();
    while(!win->shouldClose()){
        win->pollEvents();
        
        win->clear();
        win->display();
    }
    
    app.destroyWindow(win);
    lg.info("closing");
}