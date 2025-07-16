/** @file planet.cpp
 * @brief simulate simple planet
 * @author aaaa0ggmc
 * @copyright Copyright(c) 2025 aaaa0ggmc
 * @date 2025/07/16
 */
#include <AGE/Application.h>
#include <AGE/World/Components.h>
#include <AGE/World/Camera.h>
#include <AGE/World/Object.h>
#include <AGE/Input.h>
#include <AGE/World/Systems.h>

#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <alib-g3/alogger.h>
#include <iostream>
#include <vector>
#include <numbers>

using namespace age;
using namespace age::world;
using namespace age::world::comps;
using namespace alib::g3;

constexpr float cam_speed = 3;
constexpr glm::vec2 cam_rot = glm::vec2(1,1);
constexpr float framerate = 120;
constexpr int vbo_count = 2;
constexpr int vao_count = 2;
constexpr float fpsCountTimeMS = 1500;

void upload_data(VBOManager & vbos,Application &);
Window* setup(Logger & logger,LogFactory& lg,Application & app,Input & input,Shader & shader);
void dealInput(Input & input,glm::vec3& veloDir,Camera & cam,float mul);

EntityManager& getEntityManager(){
    static EntityManager em;
    return em;
}

Camera& getCamera(){
    static Camera cam (getEntityManager());
    return cam;
}

//Entities
EntityManager & em = getEntityManager();
Camera & camera = getCamera();

//Graphics ptrs
Window * win;

int main(){
    //Log
    Logger logger;
    LogFactory lg("AGETest",logger);
    ////Data////
    glm::vec3 veloDir;
    //Graphics & Input
    Input input (framerate);
    Application app (em);
    Shader shader = Shader::null();
    VAOManager & vaos = app.vaos;
    VBOManager & vbos = app.vbos;
    win = setup(logger,lg,app,input,shader);

    ////ShaderArgs////
    ShaderUniform mvp = shader["mvp_matrix"];
    ////Clocks////
    Clock elapse (false);
    Clock fpsCounter (false);

    uint64_t frame_count = 0;

    ////Bindings////
    vaos[0].bind();
    vbos[0].bind();
    vaos[0].setAttribute(vbos[0],0,3,GL_FLOAT);
    vaos[0].setAttribStatus(0,true);

    vaos[1].bind();
    vbos[1].bind();
    vaos[1].setAttribute(vbos[1],0,3,GL_FLOAT);
    vaos[1].setAttribStatus(0,true);
    ////World Objects////
    Object cube (em);
    Object pyramid (em);

    ////InitWorld////
    camera.transform().move(1,0,10);
    camera.projector().set(std::numbers::pi/3.0f,win->getFrameBufferSize().x,win->getFrameBufferSize().y);
    cube.transform().move(1,-2,1);
    pyramid.transform().move(0,0,4);

    em.addComponent<Parent>(pyramid.getEntity(),cube.getEntity());

    ////Systems////
    systems::ParentSystem parent_system (em);

    //launch clock
    elapse.start();
    fpsCounter.start();

    //// Main Loop ////
    lg.info("Entering main loop...");
    win->makeCurrent();//enable window
    while(!(win->shouldClose())){
        ++frame_count;
        if(fpsCounter.getOffset() >= fpsCountTimeMS){
            fpsCounter.clearOffset();
            lg(LOG_INFO) << "FPS:" << (int)(frame_count / fpsCountTimeMS * 1000) << endlog;
            frame_count = 0;
        }
        win->pollEvents();

        input.update();
        if(input.checkTick()){
            float p = elapse.getOffset() / 1000.0f;
            dealInput(input,veloDir,camera,p);

            //post-input-update
            cube.transform().rotate(glm::vec3(1.0f,0.0f,0.0f),1 * p);
            pyramid.transform().rotate(glm::vec3(0.0f,1.0f,1.0f),2 * p);
            em.update<comps::Transform>(elapse.getOffset(),true);
            parent_system.update();

            elapse.clearOffset();
        }

        ////draw phase////
        win->clear();
        shader.bind();

        //GL statuses//
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_CULL_FACE);

        ///Cube
        vaos[0].bind();
        glFrontFace(GL_CW);
        mvp.uploadmat4(camera.buildVPMatrix() * cube.transform().buildModelMatrix());
        win->draw(PrimitiveType::Triangles,0,36);

        ///Pyramid
        vaos[1].bind();
        glFrontFace(GL_CCW);
        mvp.uploadmat4(camera.buildVPMatrix() * pyramid.transform().buildModelMatrix());
        win->draw(PrimitiveType::Triangles,0,36);
        
        win->display();
    }

    //// cleanup ////
    app.destroyWindow(win);
    return 0;
}

void dealInput(Input & input,glm::vec3& veloDir,Camera & camera,float p){
    veloDir = glm::vec3(0,0,0);
    if(input.getKeyInfo(KeyCode::W).isPressing()){
        veloDir.z -= 1;
    }else if(input.getKeyInfo(KeyCode::S).isPressing()){
        veloDir.z += 1;
    }

    if(input.getKeyInfo(KeyCode::A).isPressing()){
        veloDir.x -= 1;
    }else if(input.getKeyInfo(KeyCode::D).isPressing()){
        veloDir.x += 1;
    }

    if(input.getKeyInfo(KeyCode::Space).isPressing()){
        veloDir.y += 1;
    }else if(input.getKeyInfo(KeyCode::LeftShift).isPressing()){
        veloDir.y -= 1;
    }

    if(input.getKeyInfo(KeyCode::Left).isPressing()){

        camera.transform().rotate(glm::vec3(0,1,0),-cam_rot.x * p);
    }else if(input.getKeyInfo(KeyCode::Right).isPressing()){
        camera.transform().rotate(glm::vec3(0,1,0),cam_rot.x * p);
    }

    if(input.getKeyInfo(KeyCode::Up).isPressing()){
        camera.transform().rotate(glm::vec3(1,0,0),-cam_rot.y * p);
    }else if(input.getKeyInfo(KeyCode::Down).isPressing()){
        camera.transform().rotate(glm::vec3(1,0,0),cam_rot.y * p);
    }

    camera.transform().buildVelocity(veloDir,cam_speed);
}

Window* setup(Logger & logger,LogFactory& lg,Application & app,Input & input,Shader & shader){
    Window * win = nullptr;

    //Init logger
    logger.appendLogOutputTarget("console",std::make_shared<lot::Console>());
    logger.appendLogOutputTarget("file0",std::make_shared<lot::SingleFile>("logs/cube.log"));

    //// Window ////
    lg.info("Creating window...");
    app.setGLVersion(4,5);
    if(!app.createWindow("TestWindow","TestAGE",800,600,100,100,WinStylePresetNormal,0)){
        lg.error("Failed to create window,now exit...");
        exit(-1);
    }else win = *app.getWindow("TestWindow");
    input.setWindow(*win);
    lg.info("CreateWindow:OK!");

    /// @TODO make the function much more better to use 
    win->setWindowSizeCallback([](GLFWwindow*win,int nw,int nh){
        glViewport(0,0,nw,nh);
        camera.projector().setAspectRatio((float)nw,(float)nh);
    });


    //一定要有window才行
    app.setGLErrCallbackFunc();
    app.setGLErrCallback(true);

    //// Shader ////
    lg.info("Creating shader...");
    shader = app.createShaderFromFile("main","test_data/cube.vert","test_data/cube.frag");
    lg.info("CreateShader:OK!");

    //// VAOs & VBOs ////
    lg.info("Creating VAOs & VBOs...");
    app.createVAOs(vao_count);
    app.checkOpenGLError();
    lg.info("VAO:OK!");

    app.createVBOs(vbo_count);
    app.checkOpenGLError();
    lg.info("VBO:OK!");

    ////VBO data upload////
    lg.info("Uploading data...");
    upload_data(app.vbos,app);
    lg.info("DataUpload:OK!");

    return win;
}

void upload_data(VBOManager & vbos,Application& app){
    std::vector<float> cube_data = {
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
    std::vector<float> pyramid_data = {
    -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, // front face
	 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, // right face
	 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, // back face
	 -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, // left face
	 -1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, // base – left front
	 1.0f, -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f // base – right back
	};
    
    vbos[0].bufferData<float>(cube_data);
    vbos[1].bufferData<float>(pyramid_data);

    app.checkOpenGLError();
}
