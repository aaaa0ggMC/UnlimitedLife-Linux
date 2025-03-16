#include <AGE/Application.h>
#include <alib-g3/alogger.h>

using namespace age;
using namespace alib::g3;

int main(){
    Application app;
    Logger logger;
    LogFactory lg("AGETest",logger);
    Window * win;

    ///Init logger
    auto target = std::make_shared<log_output_targets::Console>();
    logger.appendLogOutputTarget("console",target);

    auto fileTarget = std::make_shared<log_output_targets::SingleFile>("hello.log");
    logger.appendLogOutputTarget("file0",fileTarget);

    lg.info("Creating window...");
    ///Create Window
    {
        CreateWindowInfo info;
        info.SID = "TestWindow";
        info.width = 800;
        info.height = 600;
        info.windowTitle = "TestAGE";
        info.x = 100;
        info.y = 100;
        auto i = app.createWindow(info);
        if(!i){
            lg.error("Failed to create window,now exit...");
            exit(-1);
        }else win = *i;
    }

    win->makeCurrent();
    ///Main Loop
    while(!(win->ShouldClose())){
        win->swapBuffers();
        win->pollEvents();
    }

    app.destroyWindow(win);
    return 0;
}
