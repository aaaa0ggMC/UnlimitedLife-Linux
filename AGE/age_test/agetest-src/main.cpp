/** @file cube.cpp
 * @brief cubic
 * @author aaaa0ggmc
 * @copyright Copyright(c) 2025 aaaa0ggmc
 * @date 2025/07/19
 */
#include <AGE/Application.h>
#include <AGE/World/Components.h>
#include <AGE/World/Camera.h>
#include <AGE/World/Object.h>
#include <AGE/Input.h>
#include <AGE/World/Systems.h>
#include <AGE/Model.h>
#include <AGE/ModelLoader/PrefabGenerator.h>

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
constexpr int startup_vbo_count = 16;
constexpr int startup_vao_count = 32;
constexpr float fpsCountTimeMS = 500;

void upload_data(VBOManager & vbos,Application &);
Window* setup(Logger & logger,LogFactory& lg,Application & app,Input & input,Shader & shader,Camera & cam);
void dealInput(Input & input,glm::vec3& veloDir,Camera & cam,float mul);

// for my poor knowledge reason,these vars must be global
std::vector<const char *> texture_sids = {"wall","ice"};
std::vector<const char *> texture_paths = {"./test_data/imgs/wall.jpg","./test_data/imgs/ice.png"};

//// Global States ////
bool playing = true;

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
    std::unordered_map<std::string_view,ModelData> models;

    ////ShaderArgs////
    ShaderUniform mvp = shader["mvp_matrix"];
    ////Clocks////
    Clock elapse (false);
    Clock fpsCounter (false);

    uint64_t frame_count = 0;

    //// Textures & Samplers ////
    Texture & wall = **app.getTexture("wall");
    Sampler sampler = *app.getSampler("main");


    ////Bindings////
    vaos[0].bind();
    vbos[0].bind();
    vaos[0].setAttribute(vbos[0],0,3,GL_FLOAT);
    vaos[0].setAttribStatus(0,true);

    vaos[1].bind();
    vbos[1].bind();
    vaos[1].setAttribute(vbos[1],0,3,GL_FLOAT);
    vaos[1].setAttribStatus(0,true);
    vbos[2].bind();
    vaos[1].setAttribute(vbos[2],1,2,GL_FLOAT);
    vaos[1].setAttribStatus(1,true);

    ////World Objects////
    Object cube (em);
    Object invPar (em);
    Object pyramid (em);

    ////InitWorld////
    camera.transform().move(1,0,10);
    camera.projector().set(std::numbers::pi/3.0f,win->getFrameBufferSize().x,win->getFrameBufferSize().y);
    cube.transform().move(1,-2,1);
    invPar.transform().move(0,0,4);
    pyramid.transform().move(3,0,0);

    em.addComponent<Parent>(pyramid.getEntity(),invPar.getEntity());
    em.addComponent<Parent>(invPar.getEntity(),cube.getEntity());

    ////Systems////
    systems::ParentSystem parent_system (em);
    systems::DirtySystem<comps::Transform> marker (em);

    //// States ////

    ////IMGUI Settings////
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& im_io = ImGui::GetIO(); (void)im_io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win->getSystemHandle(),true);
    ImGui_ImplOpenGL3_Init("#version 150");
    Clock imgui_clock (false);
    Trigger im_trigger(imgui_clock,10); // 100fps
    ImDrawData * im_cached = nullptr;
    im_io.FontGlobalScale = getMonitorScale() * 1.2;
    float im_showfps = 0;
    int im_menu = 0;
    glm::vec4 im_border_color = glm::vec4(0,0,0,0);
    const char * im_wrapItems[] = {"Repeat","MirroredRepeat","ClampToEdge","ClampToBorder"};
    int im_wrapR = 0,im_wrapS = 0,im_wrapT = 0;
    float im_winalpha = 0.8f;
    float im_uialpha = 0.8f;
    int im_textureID = 0;
    float im_maxAnisotrpy = Queryer().anisotropicFiltering().second;
    float im_aniso = 0;
    int im_prec_old = 48,im_prec = 48;
    int im_polyf = 0,im_polym = 0;
    std::vector<const char*> im_spolyf = {"Front","Back","Front And Back"};
    std::vector<GLenum> im_dpolyf = {GL_FRONT,GL_BACK,GL_FRONT_AND_BACK};
    std::vector<const char*> im_spolym = {"Fill","Line","Point"};
    std::vector<GLenum> im_dpolym = {GL_FILL,GL_LINE,GL_POINT};
    bool im_glcull = true,im_gldepth = true;
    int im_gldfunc = 0;
    std::vector<const char*> im_sgldfunc = {
        "LEqual (≤)", "Less (<)", "Greater (>)", "Equal (=)", 
        "GEqual (≥)", "NotEqual (!=)", "Always", "Never"
    };
    std::vector<GLenum> im_dgldfunc = {
        GL_LEQUAL, GL_LESS, GL_GREATER, GL_EQUAL,
        GL_GEQUAL, GL_NOTEQUAL, GL_ALWAYS, GL_NEVER
    };
    std::vector<const char*> im_models = {
        "sphere",
        "torus"
    };
    int im_model = 0;
    bool ims_cube,ims_pyramid,ims_model;
    ims_cube = ims_pyramid = ims_model = true;
    int im_preview = 256;


    //// Model Data///
    auto loadModel = [&]{

        ModelData * mdx = &models["sphere"];
        model::Prefab::sphere(im_prec,mdx->vertices,mdx->indices,mdx->normals,mdx->coords);
        vaos[2].bind();
        mdx->bind(vaos[2],vbos[3],vbos[4],vbos[5]);
        
        mdx = &models["torus"];
        model::Prefab::torus(im_prec,1,0.5,mdx->vertices,mdx->indices,mdx->normals,mdx->coords);
        vaos[3].bind();
        mdx->bind(vaos[3],vbos[6],vbos[7],vbos[8]);
    };
    {
        lg.info("Loading Model...");
        Clock clk;
        loadModel();
        lg(LOG_INFO) << "LoadModel:OK! [" << clk.getOffset() << "ms]" << std::endl;
    }
    ModelData * md = &models[im_models[im_model]];

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
            ImGui::SetNextWindowBgAlpha(im_winalpha);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha,im_uialpha);
            ImGui::Begin("Controller", nullptr , ImGuiWindowFlags_MenuBar);
            if(ImGui::BeginMenuBar()){
                if(ImGui::MenuItem("Inspector")){
                    im_menu = 0;
                }
                if(ImGui::MenuItem("Sampler")){
                    im_menu = 1;
                }
                if(ImGui::MenuItem("Texture")){
                    im_menu = 2;
                }
                if(ImGui::MenuItem("Models")){
                    im_menu = 3;
                }
                if(ImGui::MenuItem("GL")){
                    im_menu = 4;
                }
                if(ImGui::MenuItem("Render")){
                    im_menu = 5;
                }
                ImGui::EndMenuBar();
            }
            

            switch(im_menu){
            case 1:{//Sampler Settings
                static auto wrapFn = [](int id)->SamplerInfo::WrapMethod {
                    switch(id){
                    case 1:
                        return SamplerInfo::WrapMethod::MirroredRepeat;
                    case 2:
                        return SamplerInfo::WrapMethod::ClampToEdge;
                    case 3:
                        return SamplerInfo::WrapMethod::ClampToBorder;
                    default:
                        return SamplerInfo::WrapMethod::Repeat;
                    }
                };
                ImGui::Text("Sampler:");
                ImGui::DragFloat4("BorderColor",glm::value_ptr(im_border_color),0.01F,0.0F,1.0F);
                if(ImGui::CollapsingHeader("Wrap Settings",ImGuiTreeNodeFlags_DefaultOpen)){
                    ImGui::ListBox("R(Useless for 2DTexture)",&im_wrapR,im_wrapItems,4);
                    ImGui::ListBox("S",&im_wrapS,im_wrapItems,4);
                    ImGui::ListBox("T",&im_wrapT,im_wrapItems,4);
                }
                if(im_maxAnisotrpy)ImGui::DragFloat("Anisotropy",&im_aniso,0.01F,0.0F,im_maxAnisotrpy);

                //update sampler settings
                sampler.wrapR(wrapFn(im_wrapR)).wrapS(wrapFn(im_wrapS)).wrapT(wrapFn(im_wrapT));
                if(im_maxAnisotrpy)sampler.try_anisotropy(im_aniso);
                sampler.borderColor(im_border_color);
                break;
            }
            case 2:{
                ImGui::Text("Texture:");
                ImGui::ListBox("Binding",&im_textureID,texture_sids.data(),texture_sids.size());
                ImGui::DragInt("Preview Size (X)",&im_preview,0.5F,0,1024);
                auto tex = unwrap(app.getTexture(texture_sids[im_textureID]));
                ImGui::Image((ImTextureID)(intptr_t)tex->getId(), ImVec2(im_preview,im_preview * tex->getTextureInfo().height / (float)tex->getTextureInfo().width));
                break;
            }
            case 3:
                ImGui::Text("Models:");
                ImGui::DragInt("Sphere Precision",&im_prec,0.1F,0);
                ImGui::ListBox("ModelSelection",&im_model,im_models.data(),im_models.size(),4);

                if(im_prec != im_prec_old){
                    /// IMGUI的限制有点聪明.... 0就是没有限制是吧
                    if(im_prec < 0)im_prec_old = im_prec = 0;
                    else im_prec_old = im_prec;
                    //reload sphere
                    loadModel();
                }
                md = &models[im_models[im_model]];
                break;
            case 4:
                ImGui::Text("GL Settings:");
                if(ImGui::CollapsingHeader("PolygonMode",ImGuiTreeNodeFlags_DefaultOpen)){
                    ImGui::ListBox("Faces",&im_polyf,im_spolyf.data(),im_spolyf.size());
                    ImGui::ListBox("Modes",&im_polym,im_spolym.data(),im_spolym.size());
                }
                ImGui::Checkbox("Cull Faces",&im_glcull);
                ImGui::Checkbox("Depth Test",&im_gldepth);
                ImGui::ListBox("Depth Func",&im_gldfunc,im_sgldfunc.data(),im_sgldfunc.size(),4);
                break;
            case 5:
                ImGui::Text("Render:");
                ImGui::Checkbox("Cube",&ims_cube);
                ImGui::Checkbox("Pyramid",&ims_pyramid);
                ImGui::Checkbox("Model",&ims_model);
                break;
            default:
                ImGui::Text("Main:");
                ImGui::Text("ImGuiFPS: %.2f ", im_io.Framerate);
                ImGui::Text("FPS: %.2f" , im_showfps);
                ImGui::DragFloat("Window Aplha",&im_winalpha,0.008F,0.0F,1.0F);
                ImGui::DragFloat("UI Aplha",&im_uialpha,0.008F,0.2F,1.0F);
                if(ImGui::CollapsingHeader("Transforms",ImGuiTreeNodeFlags_DefaultOpen)){
                    ImGui::DragFloat3("Cube Position",glm::value_ptr(cube.transform().m_position),0.05F);
                    ImGui::DragFloat4("Cube Rotation",glm::value_ptr(cube.transform().m_rotation.get_mutable_unnorm()),0.05F);
                    ImGui::DragFloat3("-Invisi Local Position",glm::value_ptr(invPar.transform().m_position),0.05F);
                    ImGui::DragFloat4("-Invisi Rotation",glm::value_ptr(invPar.transform().m_rotation.get_mutable_unnorm()),0.05F);
                    ImGui::DragFloat3("--Pyramid Local Position",glm::value_ptr(pyramid.transform().m_position),0.05F);
                    ImGui::DragFloat4("--Pyramid Rotation",glm::value_ptr(pyramid.transform().m_rotation.get_mutable_unnorm()),0.05F);
                    ImGui::DragFloat3("Camera Position",glm::value_ptr(camera.transform().m_position),0.05F);
                    ImGui::DragFloat4("Camera Rotation",glm::value_ptr(camera.transform().m_rotation.get_mutable_unnorm()),0.05F);
                }
                break;
            }
            ImGui::PopStyleVar();
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
            if(playing){
                marker.update();
                //post-input-update
                cube.transform().rotate(glm::vec3(1.0f,0.0f,0.0f),1 * p);
                invPar.transform().rotate(glm::vec3(0.0f,1.0f,1.0f),2 * p);
                pyramid.transform().rotate(glm::vec3(0.0f,1.0f,0.0f),1.5 * p);
            }
            em.update<comps::Transform>(elapse.getOffset(),true);
            parent_system.update();

            elapse.clearOffset();
        }

        ////draw phase////
        win->clear();
        shader.bind();
        sampler.bind(GL_TEXTURE0);
        // 动态纹理切换测试
        unwrap(app.getTexture(texture_sids[im_textureID]))->bind(GL_TEXTURE0);
        //GL statuses//
        if(im_gldepth){
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(im_dgldfunc[im_gldfunc]);
        }else glDisable(GL_DEPTH_TEST);
        if(im_glcull)glEnable(GL_CULL_FACE);
        else glDisable(GL_CULL_FACE);
        glPolygonMode(im_dpolyf[im_polyf],im_dpolym[im_polym]);

        ///Cube
        if(ims_cube){
            glFrontFace(GL_CW);
            vaos[0].bind();
            mvp.uploadmat4(camera.buildVPMatrix() * cube.transform().buildModelMatrix());
            win->drawArray(PrimitiveType::Triangles,0,36,1);
        }

        ///Pyramid
        if(ims_pyramid){
            vaos[1].bind();
            glFrontFace(GL_CCW);
            mvp.uploadmat4(camera.buildVPMatrix() * pyramid.transform().buildModelMatrix());
            win->drawArray(PrimitiveType::Triangles,0,36);
        }

        ///Try A Circle
        if(ims_model){
            vaos[2 + im_model].bind();
            mvp.uploadmat4(camera.buildVPMatrix() * invPar.transform().buildModelMatrix());
            win->drawElements(PrimitiveType::Triangles,md->getIndiceCount());
        }
        
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
    app.createVAOs(startup_vao_count);
    app.checkOpenGLError();
    lg.info("VAO:OK!");

    app.createVBOs(startup_vbo_count);
    app.checkOpenGLError();
    lg.info("VBO:OK!");

    ////VBO data upload////
    lg.info("Uploading data...");
    upload_data(app.vbos,app);
    lg.info("DataUpload:OK!");

    lg.info("Create Sampler...");
    app.createSampler("main");
    lg.info("CreateSampler:OK!");

    lg.info("CreateTexture Wall");
    {
        CreateTextureInfo ci;
        ci.source = ci.FromFile;
        ci.channel_desired = 4;
        ci.uploadToOpenGL = true;
        ci.genMipmap = true;
        for(size_t i = 0;i < texture_sids.size();++i){
            ci.file.path = texture_paths[i];
            ci.sid = texture_sids[i];
            app.createTexture(ci);
            lg(LOG_INFO) << "Created " << ci.sid << " " << ci.file.path << std::endl;
        }
    }
    lg.info("CreateTexture:OK!");

    win->setKeyCallback([](Window&win,KeyWrapper wp){
        if(wp.getKeyAction() == KeyAction::Release){
            if(wp.getKeyCode() == KeyCode::P){
                playing = !playing;
            }
        }
    });

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

    std::vector<float> pyramid_coord = {
        0,0,1,0,.5,1, 0,0,1,0,.5,1,
        0,0,1,0,.5,1, 0,0,1,0,.5,1,
        0,0,1,1,0 ,1, 1,1,0,0,1 ,0
	};
    
    vbos[0].bufferData<float>(cube_data);
    vbos[1].bufferData<float>(pyramid_data);
    // 基于方便性考虑vao和vbo会自动扩展,分配逻辑也从Application里面迁移到了VBO和VAO本身，Application作为调用方
    vbos[2].bufferData<float>(pyramid_coord);

    app.checkOpenGLError();
}