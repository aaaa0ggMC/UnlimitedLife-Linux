#include "app.h"
#include "imgui.h"

int main(){
    MainApplicationConfig cfg;
    MainApplication app (cfg);
    app.setup();
    // 需要用到window，必须在setup后面
    ImGUIInjector gui(app);
    app.imgui_draw_injector = [&gui](MainApplication & a){
        gui.draw(a);
    };
    app.imgui_ui_injector = [&gui](MainApplication & a){
        gui.ui(a);
    };
    app.imgui_camera_rot_injector = ImGUIInjector::camera;

    app.run();
    return 0;
}