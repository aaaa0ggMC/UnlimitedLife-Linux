#ifndef AECS_APP_COMP_H
#define AECS_APP_COMP_H
#include <alib5/aecs.h>
#include <AGE/World/Components.h>
#include <AGE/World/Camera.h>
#include <AGE/Shader.h>
#include <AGE/Window.h>
#include <AGE/Model.h>

using namespace alib5::ecs;
using namespace age::world;

struct LightComponent{

    Camera* camera;
    std::vector<glm::mat4> light_mvp_usage;

    LightComponent(Camera & cam):camera(&cam){}

    glm::mat4& get(size_t index){
        if(index >= light_mvp_usage.size())light_mvp_usage.resize(index + 1);
        return light_mvp_usage[index];
    }
}; 

struct LightMVP : public ISlotComponent,public age::DirtyMarker{
    using Dependency = ComponentStack<comps::Transform>;
    EntityManager::ref_t<Transform> transform;

    void bind_dep(const Dependency::deptup_t & tup){
        auto& [t_tran] = tup;
        transform = t_tran;
    }
    
    const glm::mat4& build_light_mvp(LightComponent & light){
        glm::mat4 & val = light.get(get_slot());
        if(!dm_check())return val;
        val = light.camera->buildVPMatrix() * transform->buildModelMatrix();
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