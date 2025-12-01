#ifndef AECS_APP_COMP_H
#define AECS_APP_COMP_H
#include <alib-g3/aecs.h>
#include <AGE/World/Components.h>
#include <AGE/Shader.h>
#include <AGE/Window.h>
#include <AGE/Model.h>

using namespace alib::g3::ecs;
using namespace age::world;

struct Render{
    using Dependency = ComponentStack<comps::Transform>;

    std::optional<EntityManager::ref_t<comps::Transform>> transform;
    EntityManager em;
    age::Model * model {nullptr};
    int model_index { 0 }
    

    void reset(){}
    Render(EntityManager & bem,age::Model * model = nullptr):em{bem}{
        this->model = model;
    }

    void bind(const Entity & e){
        if(e.id){
            transform.emplace(em.add_component<comps::Transform>());
        }else{
            transform = std::nullopt;
        }
    }

    void update(age::Window & win){
        if(transform && model){
            win.draw<age::Model>(*model);
        }
    }
};

#endif