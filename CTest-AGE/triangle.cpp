/** @file triangle.cpp
 * @brief display a simple moving triangle
 * @author aaaa0ggmc
 * @copyright Copyright(c) 2025 aaaa0ggmc
 * @date 2025/6/16
 */
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
    VAOManager & vaos = app.vaos;
    VBOManager & vbos = app.vbos;

    app.setGLVersion(4,3);

    //Init logger
    auto target = std::make_shared<lot::Console>();
    auto fileTarget = std::make_shared<lot::SingleFile>("logs/triangle.log");

    logger.appendLogOutputTarget("console",target);
    logger.appendLogOutputTarget("file0",fileTarget);

    lg.info("Creating window...");
    //// Window ////
    {
        CreateWindowInfo info;
        info.sid = "TestWindow";
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
            uniform float offset;
            void main(void){
                if(gl_VertexID == 0)gl_Position = vec4(0.25+offset,-0.25,0.0,1.0);
                else if(gl_VertexID == 1)gl_Position = vec4(-0.25+offset,-0.25,0.0,1.0);
                else gl_Position = vec4(0.25+offset,0.25,0.0,1.0);
            }
        )";
        info.fragment = R"(#version 450 core
            out vec4 color;

            void main(void){
                color = vec4(1.0,1.0,0.0,1.0);
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
    }

    vaos[0].bind();

    win->makeCurrent();
    win->setStyle(WinStyle::Resizable,AGE_Apply);

    float x = 0,inc = 0.01;
    ShaderUniform off = shader["offset"];

    //Main Loop
    lg.info("Entering main loop...");
    while(!(win->shouldClose())){
        win->pollEvents();

        if(x > 1.0f)inc = -0.01f;
        else if(x < -1.0f)inc = 0.01f;
        x += inc;

        off.upload1f(x);

        win->clear();
        shader.bind();
        glPointSize(16.0f);
        glDrawArrays(GL_TRIANGLES,0,3);
        win->display();
    }

    app.destroyWindow(win);
    return 0;
}

