#ifndef AGETEST_APP_H
#define AGETEST_APP_H
#include <unordered_map>
#include <alib-g3/alogger.h>
#include <alib-g3/aecs.h>

#include <AGE/Input.h>
#include <AGE/Model.h>
#include <AGE/Light.h>
#include <AGE/Audio.h>
#include <AGE/Texture.h>
#include <AGE/Material.h>
#include <AGE/Application.h>
#include <AGE/World/Camera.h>
#include <AGE/World/Object.h>
#include <AGE/World/Systems.h>

#include "config.h"
#include "states.h"

using namespace age;
using namespace alib::g3;
using namespace age::world;
using namespace age::material;
using namespace alib::g3::ecs;
using enum LogLevel;

struct MainApplication{
    glm::mat4 shadow_bias;

    /// Stored Config
    MainApplicationConfig cfg;
    
    /// IMGUI Injectors ///
    std::function<void(MainApplication & app)> imgui_ui_injector;
    std::function<void(MainApplication & app)> imgui_draw_injector;
    std::function<void(MainApplication & app)> imgui_camera_rot_injector;

    //// Logger ////
    Logger logger;
    LogFactory lg;
    
    //// Basic Components ////
    Input input;

    //// World & ECS ////
    EntityManager em;
    Camera camera {em};
    Camera e_light { em };
    Object cube {em};
    Object invPar {em};
    Object pyramid {em};
    Object root {em};
    Object plane {em};
    /// Systems
    systems::ParentSystem parent_system {em};
    systems::DirtySystem<comps::Transform> marker {em};

    //// Graphics ////
    Application app {em};
    VAOManager & vaos;
    VBOManager & vbos;
    /// "Borrowed" from Application
    Window * m_window {nullptr};
    Texture * shadowTex;
    Sampler m_sampler;
    Sampler shadowSampler;
    std::unordered_map<std::string,Texture*> textures;

    //// Models ///
    std::unordered_map<std::string,Model> models;
    Model * current_model {nullptr};
    Model m_plane;

    //// States ////
    States state;

    //// Material ////
    material::Material mat_gold;
    material::Material mat_jade;

    //// Lights ////
    light::PositionalLight light;

    //// Shaders ////
    Shader shader {Shader::null()};
    Shader shadowShader { Shader::null() };
    Shader callbackShader { Shader::null() };
    ShaderUniform mv_matrix;
    ShaderUniform invMV;
    ShaderUniform projectionMatrix;
    ShaderUniform shadowMVP;
    ShaderUniform shadowMVP2;

    material::MaterialBindings mb;
    light::LightBindings lb;

    //// Sounds ////
    audio::Sound snd1;
    
    //// Framebuffer ////
    Framebuffer shadowMap;

    //// Callback ////
    Framebuffer shadowMapCallback;
    Texture * shadowTexCallback;

    //// Construct Section ////
    MainApplication(MainApplicationConfig cfg)
    :logger(cfg.logger),lg(logger,cfg.logfactory)
    ,input(cfg.framerate),vaos(app.vaos),vbos(app.vbos){
        this->cfg = cfg;
    }

    ~MainApplication();

    void run();
    void handle_input(float elapse_seconds);
    void draw();
    void draw_pass_one();
    void draw_pass_two();
    void draw_callback();
    void world_update(float ep_milliseconds);

    //// Setup sections ////
    void setup();

    void setup_logger();
    void setup_window();
    void setup_err_callback();
    void setup_shader();
    void setup_buffers();
    void setup_sampler();
    void load_textures();
    void setup_framebuffers();
    void set_control_callbacks();
    void resolve_bindings();
    void init_world_objects();
    void load_dynamic_models();
    void load_static_models();
    void load_materials();
    void load_lights();
    void load_uniforms();
    void load_sounds();


    //// Data ////
    void upload_data();
};

#endif