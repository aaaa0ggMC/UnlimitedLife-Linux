/** @file cube.cpp
 * @brief display a simple cube
 * @author aaaa0ggmc
 * @copyright Copyright(c) 2025 aaaa0ggmc
 * @date 2025/6/16
 */
#include <AGE/Application.h>
#undef private
#include <GL/gl.h>
#include <alib-g3/alogger.h>
#include <iostream>
#include <vector>

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

    app.setGLVersion(4,5);

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
    app.checkOpenGLError();

    //一定要有window才行
    app.setGLErrCallbackFunc();
    app.setGLErrCallback(true);

    lg.info("CreateWindow:OK!");
    //// Shader ////
    lg.info("Creating shader...");
    {
        CreateShaderInfo info;
        info.sid = "main";
        info.vertex = R"(#version 450 core
            layout(location = 0) in vec3 position;
            uniform float offset;

            mat4 rotateZ(float angle) {
                float c = cos(angle);
                float s = sin(angle);
                return mat4(
                    c, -s, 0.0, 0.0,
                    s,  c, 0.0, 0.0,
                    0.0, 0.0, 1.0, 0.0,
                    0.0, 0.0, 0.0, 1.0
                );
            }

            void main(void){
                vec4 pos = vec4(position,1.0);
                pos *= 0.5;
                pos *= rotateZ(offset * 3.141592);
                gl_Position = pos;
            }
        )";
        info.fragment = R"(#version 450 core
            out vec4 color;

            void main(void){
                color = vec4(1.0,1.0,0.0,1.0);
            }
        )";
        shader = app.createShader(info);
        lg.info("CreateShader:OK!");
    }
    //// VAOs & VBOs ////
    {
        lg.info("Creating VAOs & VBOs...");
        CreateVAOsInfo i_vao;
        CreateVBOsInfo i_vbo;

        i_vao.count = 1;
        i_vbo.count = 1;

        app.createVAOs(i_vao);
        app.checkOpenGLError();
        lg.info("VAO:OK!");
        app.createVBOs(i_vbo);
        app.checkOpenGLError();
        lg.info("VBO:OK!");

        lg.info("Uploading data...");
        std::vector<float> data = {
            -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f,
            1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, -1.0f,
            1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f,
            1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,
            -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f
        };
        vbos[0].bufferData<float>(data);
        app.checkOpenGLError();
        lg.info("DataUpload:OK!");
    }

    vaos[0].bind();
    vbos[0].bind(); //test: move it later
    vaos[0].setAttribute(vbos[0],0,3,GL_FLOAT);
    vaos[0].setAttribStatus(0,true);

    win->makeCurrent();
    win->setStyle(WinStyle::Resizable,AGE_Disable);

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
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDrawArrays(GL_TRIANGLES,0,36);
        win->display();
    }

    app.destroyWindow(win);
    return 0;
}
