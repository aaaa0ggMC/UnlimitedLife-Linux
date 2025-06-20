/** @file cube.cpp
 * @brief cubic
 * @author aaaa0ggmc
 * @copyright Copyright(c) 2025 aaaa0ggmc
 * @date 2025/6/20
 */
#include <AGE/Application.h>
#include <AGE/World/Components.h>
#include <AGE/World/Camera.h>
#undef private
#include <GL/gl.h>
#include <alib-g3/alogger.h>
#include <iostream>
#include <vector>

using namespace age;
using namespace age::world;
using namespace age::world::comps;
using namespace alib::g3;

int main(){
    Application app;
    Logger logger;
    LogFactory lg("AGETest",logger);
    Window * win;
    Shader shader = Shader::null();
    VAOManager & vaos = app.vaos;
    VBOManager & vbos = app.vbos;
    EntityManager & em = app.em;

    app.setGLVersion(4,5);

    //Init logger
    auto target = std::make_shared<lot::Console>();
    auto fileTarget = std::make_shared<lot::SingleFile>("logs/cube.log");

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
        info.fps = 60;
        auto i = app.createWindow(info);
        if(!i){
            lg.error("Failed to create window,now exit...");
            exit(-1);
        }else win = *i;

        Window::setSwapInterval(0);
    }
    app.checkOpenGLError();

    //一定要有window才行
    // app.setGLErrCallbackFunc();
    // app.setGLErrCallback(true);

    lg.info("CreateWindow:OK!");
    //// Shader ////
    lg.info("Creating shader...");
    {
        CreateShaderInfo info;
        info.sid = "main";
        Util::io_readAll("test_data/cube.vert",info.vertex);
        Util::io_readAll("test_data/cube.frag",info.fragment);
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

    //Entities
    Camera camera (em);
    EntityWrapper cube (em.createEntity(),em);
    Transform * tcube = cube.add<Transform>();

    ShaderUniform mvp = shader["mvp_matrix"];
    camera.transform().move(0,0,-10);
    tcube->move(1,-2,1);

    Clock myCounter;
    uint64_t ct = 0;

    std::cout << "PreTest:" << std::endl;
    {
        RateLimiter rps (60);
        while(ct <= 100){
            ++ct;
            rps.wait();
        }
        std::cout << "rps:" << 1000 * ct /myCounter.getOffset() << std::endl;
        myCounter.reset();
    }

    //Main Loop
    lg.info("Entering main loop...");
    while(!(win->shouldClose())){
        win->pollEvents();
        mvp.uploadmat4(camera.buildVPMatrix() * tcube->buildModelMatrix());

        tcube->rotate(glm::vec3(1.0f,0.0f,0.0f),0.004);

        win->clear();
        shader.bind();

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_CULL_FACE);
        glFrontFace(GL_CW);
        win->draw(PrimitiveType::Triangles,0,36);
        win->display();
        ct++;
    }
    std::cout << "Average FPS in " << myCounter.getOffset() << " ms is" << ct/myCounter.getOffset() * 1000 << ". " << std::endl;

    app.destroyWindow(win);
    return 0;
}
