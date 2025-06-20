/** @file cube.cpp
 * @brief cubic
 * @author aaaa0ggmc
 * @copyright Copyright(c) 2025 aaaa0ggmc
 * @date 2025/6/20
 */
#include <AGE/Application.h>
#include <AGE/World/Components.h>
#include <AGE/World/Camera.h>
#include <AGE/World/Object.h>
#include <AGE/Input.h>

#include <GL/gl.h>
#include <alib-g3/alogger.h>
#include <iostream>
#include <vector>
#include <numbers>

using namespace age;
using namespace age::world;
using namespace age::world::comps;
using namespace alib::g3;

void upload_data(VBOManager & vbos,Application &);

int main(){
    Logger logger;
    LogFactory lg("AGETest",logger);
    Window * win;
    EntityManager em;
    Application app (em);
    VAOManager & vaos = app.vaos;
    VBOManager & vbos = app.vbos;
    Shader shader = Shader::null();
    Input input;

    //Init logger
    logger.appendLogOutputTarget("console",std::make_shared<lot::Console>());
    logger.appendLogOutputTarget("file0",std::make_shared<lot::SingleFile>("logs/cube.log"));

    //// Window ////
    lg.info("Creating window...");
    app.setGLVersion(4,5);
    if(!app.createWindow("TestWindow","TestAGE",800,600,100,100,WinStylePresetNormal^(~WinStyle::Resizable),60)){
        lg.error("Failed to create window,now exit...");
        exit(-1);
    }else win = *app.getWindow("TestWindow");
    /*win->setKeyCallback([](GLFWwindow* win,int key,int scancode,int action,int mods){
        KeyWrapper kw (win,key,scancode,action,mods);
        if(action == GLFW_PRESS)std::cout << "Pressed " << kw.getKeyCodeString() << std::endl;
    });*/
    input.setWindow(*win);
    lg.info("CreateWindow:OK!");


    //一定要有window才行
    app.setGLErrCallbackFunc();
    app.setGLErrCallback(true);

    //// Shader ////
    lg.info("Creating shader...");
    shader = app.createShaderFromFile("main","test_data/cube.vert","test_data/cube.frag");
    lg.info("CreateShader:OK!");

    //// VAOs & VBOs ////
    lg.info("Creating VAOs & VBOs...");
    app.createVAOs(1);
    app.checkOpenGLError();
    lg.info("VAO:OK!");

    app.createVBOs(1);
    app.checkOpenGLError();
    lg.info("VBO:OK!");

    ////VBO data upload////
    lg.info("Uploading data...");
    upload_data(vbos,app);
    lg.info("DataUpload:OK!");

    ////Bindings////
    vaos[0].bind();
    vbos[0].bind();
    vaos[0].setAttribute(vbos[0],0,3,GL_FLOAT);
    vaos[0].setAttribStatus(0,true);

    ////Entities////
    Camera camera (em);
    Object cube (em);

    ////ShaderArgs////
    ShaderUniform mvp = shader["mvp_matrix"];

    ////InitWorld////
    camera.transform().move(0,0,10);
    camera.projector().set(std::numbers::pi/3.0f,win->getFrameBufferSize().x,win->getFrameBufferSize().y);
    cube.transform().move(1,-2,1);

    //// Main Loop ////
    lg.info("Entering main loop...");
    win->makeCurrent();//enable window
    while(!(win->shouldClose())){
        win->pollEvents();
        ////upload shader data////
        mvp.uploadmat4(camera.buildVPMatrix() * cube.transform().buildModelMatrix());

        ////update////
        cube.transform().rotate(glm::vec3(1.0f,0.0f,0.0f),0.004);

        input.update();
        /// @todo follow with input's tick to update,which means i need to do some extra change
        if(input.getKeyInfo(KeyCode::A).status == KeyState::Pressed){
            std::cout << "Pressed A" << std::endl;
        }

        ////draw phase////
        win->clear();
        shader.bind();

        //GL statuses//
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_CULL_FACE);
        glFrontFace(GL_CW);

        win->draw(PrimitiveType::Triangles,0,36);
        win->display();
    }

    //// cleanup ////
    app.destroyWindow(win);
    return 0;
}

void upload_data(VBOManager & vbos,Application& app){
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
}
