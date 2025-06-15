#include <AGE/Application.h>
#include <alib-g3/alogger.h>

using namespace age;
using namespace alib::g3;

int main(){
    Application app;
    Logger logger;
    LogFactory lg("AGETest",logger);
    Window * win;
    Shader shader = Shader::null();
    VAO vao;

    app.setGLVersion(4,3);

    //Init logger
    auto target = std::make_shared<lot::Console>();
    auto fileTarget = std::make_shared<lot::SingleFile>("hello.log");

    logger.appendLogOutputTarget("console",target);
    logger.appendLogOutputTarget("file0",fileTarget);

    lg.info("Creating window...");
    //// Window ////
    {
        CreateWindowInfo info;
        info.SID = "TestWindow";
        info.width = 800;
        info.height = 600;
        info.windowTitle = "TestAGE";
        info.x = 100;
        info.y = 100;
        info.fps = 120;
        auto i = app.createWindow(info);
        if(!i){
            lg.error("Failed to create window,now exit...");
            exit(-1);
        }else win = *i;
    }
    //// Shader ////
    {
        CreateShaderInfo info;
        info.sid = "main";
        info.vertex = R"(#version 450 core
            void main(void){
                gl_Position = vec4(0.0,0.0,0.5,1.0);
            }
        )";
        info.fragment = R"(#version 450 core
            out vec4 color;

            void main(void){
                color = vec4(1.0,1.0,1.0,1.0);
            }
        )";
        shader = app.createShader(info);
    }
    //// VAOs & VBOs ////
    {
        CreateVAOsInfo i_vao;
        CreateVBOsInfo i_vbo;

        i_vao.count = 1;
        i_vbo.count = 0;

        app.createVAOs(i_vao);
        app.createVBOs(i_vbo);

        vao = app.getVAO(0);
        vao.bind();
    }

    win->makeCurrent();
    //Main Loop
    lg.info("Entering main loop...");
    while(!(win->shouldClose())){
        win->pollEvents();

        win->clear();
        shader.bind();
        glDrawArrays(GL_POINTS,0,1);
        win->display();
    }

    app.destroyWindow(win);
    return 0;
}
