#ifndef AGETEST_CONFIG_H
#define AGETEST_CONFIG_H
#include <unordered_map>
#include <functional>

#include <alib-g3/alogger.h>
#include <alib-g3/aecs.h>

#include <AGE/Input.h>
#include <AGE/Application.h>
#include <AGE/World/Camera.h>

using namespace age;
using namespace alib::g3;
using namespace alib::g3::ecs;
using namespace alib::g3::world;
using enum LogLevel;

struct MainApplicationConfig{
    //// Logger ////
    LoggerConfig logger;
    LogFactoryConfig logfactory;
    lot::ConsoleConfig mod_console;
    std::string file_path {"logs/cube.log"};
    //// Input ////
    float framerate { 120 };
    //// Window & Graphics ////
    CreateWindowInfo ci;
    bool gl_err_callback {false};
    size_t vao_count {32};
    size_t vbo_count {16};
    float fpsCountTimeMs { 500 };

    //// Shaders ////
    std::string main_vert {"test_data/cube.vert"};
    std::string main_frag {"test_data/cube.frag"};

    //// Textures ////
    std::vector<std::string> texture_sids;
    std::vector<std::string> texture_paths;

    //// Models ////
    std::unordered_map<std::string,std::string> models;\

    //// Sounds ////
    std::string snd_file {"./test_data/test_music.flac"};

    MainApplicationConfig(){
        //// CreateWindowInfo ////
        ci.sid = "TestWindow";
        ci.windowTitle = "TestAGE-测试";
        // 真正设置大小在后面，并非真的800*600
        ci.width = 800;
        ci.height = 600;
        ci.x = 100;
        ci.y = 100;
        ci.style = WinStylePresetNormal;
        ci.fps = 0;
        ci.ScreenPercent(0.5,1,&ci.width,&ci.height);
        ci.KeepRatio(ci.width,ci.height,800,600);
        ci.ScreenPercent(0.2,0.2,&ci.x,&ci.y);

        //// Textures ////
        texture_sids = {
            "wall",
            "ice"
        };
        texture_paths = {
            "./test_data/imgs/wall.jpg",
            "./test_data/imgs/ice.png"
        };

        //// Models ////
        models["main.obj"] = "./test_data/main.model";
    }
};
#endif