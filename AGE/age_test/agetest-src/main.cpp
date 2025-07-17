/** @file cube.cpp
 * @brief cubic
 * @author aaaa0ggmc
 * @copyright Copyright(c) 2025 aaaa0ggmc
 * @date 2025/07/17
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

////使用IMGUI对帧率的影响：从6000降到3000,imgui大概用了0.1ms来运作（我的电脑）////
////限制imgui 100fps更新后，帧率达到6000fps
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

using namespace age;
using namespace age::world;
using namespace age::world::comps;
using namespace alib::g3;

constexpr float cam_speed = 3;
constexpr glm::vec2 cam_rot = glm::vec2(1,1);
constexpr float framerate = 120;
constexpr int vbo_count = 2;
constexpr int vao_count = 2;
constexpr float fpsCountTimeMS = 500;

void upload_data(VBOManager & vbos,Application &);
Window* setup(Logger & logger,LogFactory& lg,Application & app,Input & input,Shader & shader,Camera & cam);
void dealInput(Input & input,glm::vec3& veloDir,Camera & cam,float mul);

int main(){
    //Log
    Logger logger;
    LogFactory lg("AGETest",logger);
    ////Data////
    glm::vec3 veloDir;
    //Graphics & Input
    Input input (framerate);
    EntityManager em;
    Application app (em);
    ////World Camera////
    Camera camera (em);
    Shader shader = Shader::null();
    VAOManager & vaos = app.vaos;
    VBOManager & vbos = app.vbos;
    Window * win = setup(logger,lg,app,input,shader,camera);

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
    systems::MarkerSystem<comps::Transform> marker (em);

    ////IMGUI Settings////
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& im_io = ImGui::GetIO(); (void)im_io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win->getSystemHandle(),true);
    ImGui_ImplOpenGL3_Init("#version 150");
    ImFont* im_font = im_io.Fonts->AddFontFromFileTTF
    (
        "C:\\Windows\\Fonts\\msyh.ttc",
        30,
        nullptr,
        im_io.Fonts->GetGlyphRangesChineseFull()
    );
    IM_ASSERT(font != nullptr);
    Clock imgui_clock (false);
    Trigger im_trigger(imgui_clock,10); // 100fps
    ImDrawData * im_cached = nullptr;
    float im_showfps = 0;


    //launch clock
    elapse.start();
    fpsCounter.start();
    imgui_clock.start();

    //// Main Loop ////
    lg.info("Entering main loop...");
    win->makeCurrent();//enable window
    while(!(win->shouldClose())){
        ++frame_count;
        if(fpsCounter.getOffset() >= fpsCountTimeMS){
            fpsCounter.clearOffset();
            im_showfps = (int)(frame_count / fpsCountTimeMS * 1000);
            frame_count = 0;
        }
        win->pollEvents();
        ////IMGUI////
        //我想imgui 100fps
        if(im_trigger.test(true) || !im_cached){
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::Begin("Controller", nullptr , ImGuiWindowFlags_MenuBar);
            ImGui::Text("ImGuiFPS: %.2f ", im_io.Framerate);
            ImGui::Text("FPS: %.2f" , im_showfps);
            ImGui::DragFloat3("Cube Position",glm::value_ptr(cube.transform().m_position),0.05F);
            ImGui::DragFloat4("Cube Rotation",glm::value_ptr(cube.transform().m_rotation.get_mutable_unnorm()),0.05F);
            ImGui::DragFloat3("Pyramid Local Position",glm::value_ptr(pyramid.transform().m_position),0.05F);
            ImGui::DragFloat4("Pyramid Rotation",glm::value_ptr(pyramid.transform().m_rotation.get_mutable_unnorm()),0.05F);
            ImGui::DragFloat3("Camera Position",glm::value_ptr(camera.transform().m_position),0.05F);
            ImGui::DragFloat4("Camera Rotation",glm::value_ptr(camera.transform().m_rotation.get_mutable_unnorm()),0.05F);
            ImGui::End();
            ImGui::Render();
            //缓存绘制数据
            im_cached = ImGui::GetDrawData();
        }


        input.update();
        if(input.checkTick()){
            float p = elapse.getOffset() / 1000.0f;
            dealInput(input,veloDir,camera,p);

            
            //由于imgui同步问题，所以数据要dm_mark，但是imgui更新频率大于tick频率，所以要下checkTick里面hhh
            marker.update();

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
        
        //// IMGUI Finish up////
        if(im_cached)ImGui_ImplOpenGL3_RenderDrawData(im_cached);

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

Window* setup(Logger & logger,LogFactory& lg,Application & app,Input & input,Shader & shader,Camera & camera){
    Window * win = nullptr;

    //Init logger
    logger.appendLogOutputTarget("console",std::make_shared<lot::Console>());
    logger.appendLogOutputTarget("file0",std::make_shared<lot::SingleFile>("logs/cube.log"));

    //// Window ////
    lg.info("Creating window...");
    app.setGLVersion(4,5);
    {
        CreateWindowInfo ci;
        ci.sid = "TestWindow";
        ci.windowTitle = "TestAGE";
        ci.width = 800;
        ci.height = 600;
        ci.x = 100;
        ci.y = 100;
        ci.style = WinStylePresetNormal;
        ci.fps = 0;
        ci.ScreenPercent(0.6,1,&ci.width,&ci.height);
        ci.KeepRatio(ci.width,ci.height,800,600);
        ci.ScreenPercent(0.2,0.2,&ci.x,&ci.y);

        if(!app.createWindow(ci)){
            lg.error("Failed to create window,now exit...");
            exit(-1);
        }else win = *app.getWindow("TestWindow");
    }
    
    input.setWindow(*win);
    lg.info("CreateWindow:OK!");

    win->setWindowSizeCallback([&camera](Window & win,int nw,int nh){
        glViewport(0,0,nw,nh);
        camera.projector().setAspectRatio((float)nw,(float)nh);
    });

    //一定要有window才行
    // @TODO 因为IMGUI太吵了，所以暂时关了
    //app.setGLErrCallbackFunc();
    //app.setGLErrCallback(true);

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