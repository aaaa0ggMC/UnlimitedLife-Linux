#include "app.h"
#include <AGE/ModelLoader/PrefabGenerator.h>
#include <AGE/ModelLoader/Loader.h>

MainApplication::~MainApplication(){
    // 剩下的让application自己销毁吧。。。
    app.windows.destroy(*m_window);
}

void MainApplication::setup(){
    setup_logger();
    setup_window();
    if(cfg.gl_err_callback)setup_err_callback();
    setup_shader();
    setup_buffers();
    setup_sampler();
    load_textures();
    set_control_callbacks();
    resolve_bindings();
    init_world_objects();
    load_dynamic_models();
    load_static_models();
    load_materials();
    load_lights();
    load_uniforms();
    load_sounds();
}

void MainApplication::load_sounds(){
    snd1.loadFromFile(cfg.snd_file);
    lg(Info) << "LoadSounds:OK" << endlog;
}

void MainApplication::load_uniforms(){
    shader["gambient"].upload4f(0.5,0.5,0.5,1.0);
    projectionMatrix = shader["proj_matrix"];
    mv_matrix = shader["mv_matrix"];
    invMV = shader["invMV"];

    projectionMatrix.uploadmat4(camera.projector().buildProjectionMatrix());

    lg(Info) << "LoadUniforms:OK" << endlog;
}

void MainApplication::load_lights(){
    using namespace age::light::uploaders;
    light.ambient.fromRGBA(0.0,0.0,0.0,1.0);
    light.diffuse.fromRGBA(1.0,1.0,1.0,1.0);
    light.specular.fromRGBA(1.0,1.0,1.0,1.0);
    light.position = glm::vec3(0,4,0);

    lb.ambient = createUniformName<glm::vec4>(shader,"light.ambient")();
    lb.diffuse = createUniformName<glm::vec4>(shader,"light.diffuse")();
    lb.specular = createUniformName<glm::vec4>(shader,"light.specular")();
    lb.position = createUniformName<glm::vec3>(shader,"light.position")();

    light.upload(lb);
    lg(Info) << "LoadLight:OK" << endlog;
}

void MainApplication::load_materials(){
    using namespace age::light::uploaders;
    mat_gold.ambient.fromRGBA(0.2473f,0.1995f,0.0745f,1);
    mat_gold.diffuse.fromRGBA(0.7516f,0.6065f,0.2265f,1);
    mat_gold.specular.fromRGBA(0.6283f,0.5559f,0.3661f,1);
    mat_gold.shininess = 51.2f;

    mat_jade.ambient.fromRGBA(0.135f,0.2225f,0.1575f,0.95f);
    mat_jade.diffuse.fromRGBA(0.54f,0.89f,0.63f,0.95);
    mat_jade.specular.fromRGBA(0.3162f,0.3162f,0.3162f,0.95);
    mat_jade.shininess = 12.8f;

    mb.ambient = createUniformName<glm::vec4>(shader,"material.ambient")();
    mb.diffuse = createUniformName<glm::vec4>(shader,"material.diffuse")();
    mb.specular = createUniformName<glm::vec4>(shader,"material.specular")();
    mb.shininess = createUniformName<float>(shader,"material.shininess")();

    lg(Info) << "LoadMaterials:OK" << endlog;
}

void MainApplication::load_static_models(){
    Model * mdx;
    Clock gclk;
    for(auto & [k,fp] : cfg.models){
        state.models.emplace(k);
        mdx = &models[k];
        Clock clk;
        model::loadModelFromFile<model::fmt::AutoDetect>(fp,*mdx);
        lg(Info) << "Loaded model " << k << " in " << clk.getOffset() << "ms." << std::endl;
    }

    state.models.emplace("cube");
    mdx = &models["cube"];
    model::Prefab::cube(1,*mdx);
    if(!current_model)current_model = mdx;

    model::Prefab::box(100,0.2,100,m_plane,32); // 重复多次uv

    lg(Info) << "LoadStaticModel:OK in " << gclk.getOffset() << "ms" << endlog;
}

void MainApplication::load_dynamic_models(){
    Clock gclk;
    state.models.emplace("sphere");
    state.models.emplace("torus");

    Model * mdx = &models["sphere"];
    model::Prefab::sphere(state.precision,*mdx);

    mdx = &models["torus"];
    model::Prefab::torus(state.precision,1,0.5,*mdx);

    lg(Info) << "LoadDynamicModel:OK in " << gclk.getOffset() << "ms" << endlog;
}

void MainApplication::init_world_objects(){
    camera.transform().move(1,0,10);
    camera.projector().set(std::numbers::pi/3.0f,m_window->getFrameBufferSize().x,m_window->getFrameBufferSize().y);
    camera.cameraEntity.add<comps::Tag>("camera");
    
    cube.transform().move(1,-3,6);
    cube.getEntityWrapper().add<comps::Tag>("cube");

    invPar.transform().move(0,0,4);
    invPar.getEntityWrapper().add<comps::Tag>("invPar");

    pyramid.transform().move(3,0,0);
    pyramid.getEntityWrapper().add<comps::Tag>("pyramid");

    plane.transform().move(0,-10,0);
    plane.getEntityWrapper().add<comps::Tag>("plane");

    root.getEntityWrapper().add<comps::Tag>("root");

    em.add_component<Parent>(pyramid.getEntity(),invPar.getEntity());
    em.add_component<Parent>(invPar.getEntity(),cube.getEntity());
    lg(Info) << "InitWorldObjects:OK" << endlog;
}

void MainApplication::resolve_bindings(){
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
    lg(Info) << "ResolveBindings:OK" << endlog;
}

void MainApplication::set_control_callbacks(){
    // size callback
    m_window->setWindowSizeCallback([this](Window & win,int w,int h,int ow,int oh){
        static int old_x = -1,old_y = -1;
        if(std::abs(old_x - ow) <= 1 && std::abs(old_y - oh) <= 1){
            return;
        }

        float fac_x = w / (float)ow;
        float fac_y = h / (float)oh;
        
        CreateWindowInfo::KeepRatio(ow,oh,this->cfg.ci.width,this->cfg.ci.height);
        old_x = ow;
        old_y = oh;

        win.setSize(ow,oh);

        w = ow * fac_x;
        h = oh * fac_y;

        glViewport(0,0,w,h);
        this->camera.projector().setAspectRatio((float)w,(float)h);
    });
    // key callback
    m_window->setKeyCallback([this](Window&win,KeyWrapper wp){
        if(wp.getKeyAction() == KeyAction::Release){
            if(wp.getKeyCode() == KeyCode::P){
                this->state.playing = !this->state.playing;
            }
        }
    });
    lg(Info) << "ControlCallbacks:OK" << endlog;
}

void MainApplication::load_textures(){
    CreateTextureInfo ci;
    ci.source = ci.FromFile;
    ci.channel_desired = 4;
    ci.uploadToOpenGL = true;
    ci.genMipmap = true;
    
    size_t i = 0;
    for(;i < cfg.texture_sids.size();++i){
        if(i >= cfg.texture_paths.size()){
            lg(Warn) << "Skip " << cfg.texture_sids.size() - i << " textures for the lack of texture paths!" << endlog;
            break;
        }
        ci.file.path = cfg.texture_paths[i];
        ci.sid = cfg.texture_sids[i];
        auto texture = app.textures.create(ci);
        if(texture){
            textures.emplace(cfg.texture_sids[i],*texture);
            lg(Info) << "Successfully loaded texture with sid[" << ci.sid << "] file_path[" << ci.file.path << "]!" << endlog;
        }else{
            lg(LogLevel::Error) << "Failed to load texture with sid[" << ci.sid << "] file_path[" << ci.file.path << "]!" << endlog;
        }
    }
    if(i > cfg.texture_sids.size()){
        lg(Warn) << "Skip " << cfg.texture_sids.size() - i << " textures for the lack of texture sids!" << endlog;
    }
    lg(Info) << "LoadTextures:OK" << endlog;
}

void MainApplication::setup_sampler(){
    auto t = app.samplers.create("main");
    if(!t){
        lg(Fatal) << "Failed to create sampler!" << endlog;
        std::exit(-1);
    }else m_sampler = t;
    lg(Info) << "CreateSampler:OK" << endlog;
}

void MainApplication::setup_buffers(){
    app.createVAOs(cfg.vao_count);
    app.checkOpenGLError();
    lg(Info) << "CreateVAOs:OK" << endlog;

    app.createVBOs(cfg.vbo_count);
    app.checkOpenGLError();
    lg(Info) << "CreateVBOs:OK" << endlog;

    /// Mapping
    /// 0  cube vertex data
    /// 1  pyramid vertex data
    /// 2  pyramid coord data
    upload_data();
}

void MainApplication::setup_shader(){
    shader = app.shaders.fromFile("main",cfg.main_vert,cfg.main_frag);
    lg(Info) << "CreateShader:OK" << endlog;
}

void MainApplication::setup_err_callback(){
    app.setGLErrCallbackFunc();
    app.setGLErrCallback(true);
    lg(Info) << "GLErrCallback:OK" << endlog;
}

void MainApplication::setup_logger(){
    logger.append_mod<lot::Console>("console",cfg.mod_console);
    logger.append_mod<lot::File>("file0",cfg.file_path);
    lg(Info) << "SetupLogger:OK" << endlog;
}

void MainApplication::setup_window(){
    auto t = app.windows.create(cfg.ci);
    if(!t){
        lg(Fatal) << "Failed to create window!" << endlog;
        std::exit(-1);
    }else m_window = *t;
    input.setWindow(*m_window);
    m_window->makeCurrent();
    lg(Info) << "CreateWindow:OK" << endlog;
}