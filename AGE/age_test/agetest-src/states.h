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
using namespace alib::g3::ecs;
using namespace alib::g3::world;
using enum LogLevel;

struct States{
    /// @brief 当前程序是否处于pause状态
    bool playing {true};
    /// @brief 生成精度
    size_t precision {1};
    /// @brief 模型列表
    std::unordered_set<std::string> models; 
    /// @brief FPS
    float fps {0};
};

#endif