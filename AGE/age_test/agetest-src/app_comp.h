#ifndef AECS_APP_COMP_H
#define AECS_APP_COMP_H
#include <alib-g3/aecs.h>
#include <AGE/World/Components.h>
#include <AGE/World/Camera.h>
#include <AGE/Shader.h>
#include <AGE/Window.h>
#include <AGE/Model.h>

using namespace alib::g3::ecs;
using namespace age::world;

struct LightComponent{
    std::vector<glm::mat4> light_mvp_usage;
    EntityManager::ref_t<Camera> camera;

    LightMVP(EntityManager::ref_t<Camera> cam):camera{cam}{}

    glm::mat4& get(size_t index){
        if(index >= light_mvp_usage.size())light_mvp_usage.resize(index + 1);
        return light_mvp_usage[index];
    }
}; 

struct LightMVP : public ISlotComponent,public age::DirtyMarker{
    using Dependency = ComponentStack<comps::Transform>;
    EntityManager::ref_t<Transform> transform;

    LightMVP(EntityManager::ref_t<Transform> trans):transform{trans}{
        trans->chain = (age::DirtyMarker *)this;
    }

    const glm::mat4& build_light_mvp(Camera & light_cam,LightComponent & light){
        glm::mat4 & val = light.get(get_slot());
        if(!dm_check())return val;
        val = light_cam.buildVPMatrix() * transform->buildModelMatrix();
        return val;
    }
};

struct Render{
    using Dependency = ComponentStack<comps::Transform>;
    age::Model * model {nullptr};

};

struct LightSystem{


};

#endif