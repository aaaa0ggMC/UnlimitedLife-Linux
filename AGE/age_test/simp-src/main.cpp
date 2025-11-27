#include <AGE/Window.h>
#include <AGE/Application.h>
#include <AGE/Light.h>

using namespace alib::g3;
using namespace alib::g3::ecs;
using namespace age;
using namespace age::light;
using namespace age::light::uploaders;
using enum LogLevel;

int main(){
    Logger logger;
    LogFactory lg(logger,LogFactoryConfig("SIMP"));
    EntityManager entity_manager;
    Application app(entity_manager);
    Window *win;

    logger.append_mod<lot::Console>("console");

    lg.log(Info,"Creating window...");
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
            lg.log(LogLevel::Error,"Failed to create window,now exit...");
            exit(-1);
        }else win = *app.getWindow("SimpTest");
    }
    
    age::Shader shader = app.createShaderFromFile("main","test_data/cube.vert","test_data/cube.frag");
    ////Lights////
    PositionalLight light;
    LightBindings lb;
    {
        lg.log(Info,"Loading lights..");
        light.ambient.fromRGBA(0.0,0.0,0.0,1.0);
        light.diffuse.fromRGBA(1.0,1.0,1.0,1.0);
        light.specular.fromRGBA(1.0,1.0,1.0,1.0);
        light.position = glm::vec3(0,4,0);
        
        lb.ambient = createUniformName<glm::vec4>(shader,"light.ambient")();
        lb.diffuse = createUniformName<glm::vec4>(shader,"light.diffuse")();
        lb.specular = createUniformName<glm::vec4>(shader,"light.specular")();
        lb.position = createUniformName<glm::vec3>(shader,"light.position")();

        light.upload(lb);
        lg.log(Info,"LoadLight: OK!");
    }


    win->makeCurrent();
    while(!win->shouldClose()){
        shader.bind();
        win->pollEvents();
        
        win->clear();
        win->display();
    }
    
    app.destroyWindow(win);
    lg.log(Info,"closing");
}