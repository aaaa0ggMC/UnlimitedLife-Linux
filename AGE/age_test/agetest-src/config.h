#ifndef AGETEST_CONFIG_H
#define AGETEST_CONFIG_H
#include <unordered_map>

#include <alib5/alogger.h>
#include <alib5/adata.h>
#include <alib5/aecs.h>

#include <AGE/Input.h>
#include <AGE/Application.h>
#include <AGE/World/Camera.h>
#include <glm/glm.hpp>

using namespace age;
using namespace alib5;
using namespace age::world;
using namespace alib5::ecs;
using enum LogLevel;

namespace detail_data{
    constexpr GLenum gl_depthfunc_enums[] {
        GL_LEQUAL, GL_LESS, GL_GREATER, GL_EQUAL,
        GL_GEQUAL, GL_NOTEQUAL, GL_ALWAYS, GL_NEVER
    };
    constexpr const char* gl_depthfunc_desc[] = {
        "LEqual (≤)", "Less (<)", "Greater (>)", "Equal (=)", 
        "GEqual (≥)", "NotEqual (!=)", "Always", "Never"
    };
    constexpr GLenum gl_polygon_face_enums[] = {GL_FRONT,GL_BACK,GL_FRONT_AND_BACK};
    constexpr const char* gl_polygon_face_desc[] = {"Front","Back","Front And Back"};
    constexpr GLenum gl_polygon_mode_enums[] = {GL_FILL,GL_LINE,GL_POINT};
    constexpr const char* gl_polygon_mode_desc[] = {"Fill","Line","Point"};
}
constexpr std::span<const GLenum> gl_depthfunc_enums (detail_data::gl_depthfunc_enums);
constexpr std::span<const GLenum> gl_polygon_face_enums (detail_data::gl_polygon_face_enums);
constexpr std::span<const GLenum> gl_polygon_mode_enums (detail_data::gl_polygon_mode_enums);
constexpr std::span<const char* const> gl_depthfunc_desc (detail_data::gl_depthfunc_desc);
constexpr std::span<const char* const> gl_polygon_face_desc (detail_data::gl_polygon_face_desc);
constexpr std::span<const char* const> gl_polygon_mode_desc (detail_data::gl_polygon_mode_desc);

struct MainApplicationConfig {
    /// Subs
    struct Logger{
        int consumer_count = 1;
        /// 文件日志写入位置
        std::string file_path = "logs/cube.log";
    };
    struct Window{
        std::string title = "TestAGE - 测试";
        float framerate = 120;
    };
    struct GL {
        bool err_callback = true;

        [[=alib5::attr::rename<"reserve_vaos">()]]
        size_t vao_count = 32;
        
        [[=alib5::attr::rename<"reserve_vbos">()]]
        size_t vbo_count = 16;
    };
    struct UI {
        float fps_count_elapse_ms = 500;
        float refresh_rate = 100;
    };
    struct World{
        float update_elapse_ms = 200;
    };
    struct Shader{
        std::string main_vert = "test_data/cube.vert";
        std::string main_frag = "test_data/cube.frag";
        std::string sh_vert = "test_data/shadow.vert";
        std::string sh_frag = "test_data/shadow.frag";
        std::string shc_vert = "test_data/shadow_cl.vert";
        std::string shc_frag = "test_data/shadow_cl.frag";
    };
    struct FileInfo{
        std::string sid;
        std::string path;
    };
    struct Camera{
        struct V2{
            float x;
            float y;
        };

        V2 rotation {1,1};
        float speed {3};
    };
    struct Internal{
        /// 这个是用于给imgui选择而生成的列表
        std::vector<const char *> texture_sids;
        std::vector<const char *> texture_paths;
        /// 创建info
        CreateWindowInfo ci;
        /// 日志配置
        LoggerConfig logger;
        LogFactoryConfig logfactory;
        lot::ConsoleConfig mod_console;
    };


    /// 日志系统配置
    Logger logger { };
    /// GL设置
    GL gl;
    /// UI设置
    UI ui;
    /// 世界设置
    World world;
    /// 窗口设置
    Window window;
    /// 着色器
    Shader shader;
    /// 贴图
    std::vector<FileInfo> textures {
        {"wall","./test_data/imgs/wall.jpg"},
        {"ice","./test_data/imgs/ice.png"}
    };
    /// 模型
    std::vector<FileInfo> models {
        {"main.obj","./test_data/main.model"}
    };
    /// 音频文件地址
    std::string snd_file { "./test_data/test_mp3.mp3" };
    /// 相机配置
    Camera camera;

    [[=alib5::attr::skip()]]
    Internal i;

    void build_internal(){
        /// 窗口配置
        i.ci.sid = "TestWindow";
        i.ci.windowTitle = window.title;
        // 真正设置大小在后面，并非真的800*600
        i.ci.width = 800;
        i.ci.height = 600;
        i.ci.x = 100;
        i.ci.y = 100;
        i.ci.style = WinStylePresetNormal;
        i.ci.fps = window.framerate;
        i.ci.ScreenPercent(0.5,1,&i.ci.width,&i.ci.height);
        i.ci.KeepRatio(i.ci.width,i.ci.height,800,600);
        i.ci.ScreenPercent(0.2,0.2,&i.ci.x,&i.ci.y);
        /// textures表
        for(auto & target : textures){
            i.texture_sids.emplace_back(target.sid.c_str());
            i.texture_paths.emplace_back(target.path.c_str());
        }
        /// Logger
        i.logger.consumer_count = logger.consumer_count;

    }
};

#endif