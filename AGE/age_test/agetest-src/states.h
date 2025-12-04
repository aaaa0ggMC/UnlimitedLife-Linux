#ifndef AGETEST_STATES_H
#define AGETEST_STATES_H
#include <unordered_set>
#include <alib-g3/alogger.h>
#include <alib-g3/aecs.h>

#include <AGE/Input.h>
#include <AGE/Application.h>
#include <AGE/World/Camera.h>

using namespace age;
using namespace alib::g3;
using namespace age::world;
using namespace alib::g3::ecs;
using enum LogLevel;

struct States{
    /// @brief 当前程序是否处于pause状态
    bool playing {true};
    /// @brief 生成精度
    int precision { 18 };
    /// @brief 模型列表
    std::unordered_set<std::string> models; 
    /// @brief FPS
    float fps {0};
    /// @brief 当前的贴图设置
    int current_texture_id {0};
    /// @brief 预览大小
    int texture_preview_size {128};
    /// @brief 是否启用深度测试
    bool gl_depth {true};
    /// @brief 深度测试的索引
    int gl_depthfunc_index {0};
    /// @brief 其他索引
    int gl_polygon_face_index {0};
    int gl_polygon_mode_index {0};
    /// @brief 是否裁切
    bool gl_cull {true};
    /// @brief 鼠标灵敏度 [建议 1 - 1'000'000，运算的时候会除以1'000'000]
    float mouse_sensitivity { 1000 };
    /// @brief 点的大小
    float point_size {1.0f};

    /// Show
    bool show_cube {true};
    bool show_pyramid {true};
    bool show_model {true};
    bool show_platform {true};

    /// Models ///
    int current_model_index {0};


    //// IM Settings ////
    /// @brief IMCacheData
    void* im_cached {nullptr};
    /// @brief 窗口不透明度
    float im_winalpha { 0.8 };
    /// @brief UI不透明度
    float im_uialpha { 0.8 };
    
    //// Sampler ////
    glm::vec4 sampler_border_color {0,0,0,1};
    int sampler_wrap_r {0};
    int sampler_wrap_s {0};
    int sampler_wrap_t {0};
    float sampler_aniso { 2 };

    //// 播放进度 ////
    float progress { 0.0 };

    bool use_light_cam { false };
};

#endif