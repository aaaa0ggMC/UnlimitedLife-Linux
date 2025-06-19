#ifndef AGE_H_CAMERA
#define AGE_H_CAMERA
#include "Components.h"
#include <AGE/Base.h>
#include <AGE/World/EntityManager.h>
#include <AGE/World/Components.h>

namespace age::world{
    struct Camera : public DirtyMarker{
#ifndef AGE_EM_DEBUG
    private:
#endif
        EntityManager& em;
        EntityWrapper cameraEntity;
        std::vector<comps::Transform>* ts;
        std::vector<comps::Viewer>* vs ;
        std::vector<comps::Projector>* ps;

        size_t mp_t,mp_v,mp_p;

    public:
        glm::mat4 vp_matrix;

        inline Camera(EntityManager& iem):
        em{iem},
        cameraEntity{iem.createEntity(),iem}{
            cameraEntity.add<comps::Transform>();
            cameraEntity.add<comps::Viewer>();
            cameraEntity.add<comps::Projector>();
            auto t = (*iem.getComponentPool<comps::Transform>());
            auto v = (*iem.getComponentPool<comps::Viewer>());
            auto p = (*iem.getComponentPool<comps::Projector>());

            ts = &(t->data);
            vs = &(v->data);
            ps = &(p->data);

            mp_t = t->mapper[cameraEntity.e.id];
            mp_v = v->mapper[cameraEntity.e.id];
            mp_p = p->mapper[cameraEntity.e.id];

            ((*ts)[mp_t]).chain = (DirtyMarker*)this;
            ((*ps)[mp_p]).chain = (DirtyMarker*)this;
        }

        inline ~Camera(){
            cameraEntity.destroy();
        }

        inline glm::mat4& buildVPMatrix(){
            if(!dm_check())return vp_matrix;
            //std::cout << "Changed,so i updated" << std::endl;
            vp_matrix = projector().buildProjectionMatrix() * viewer().buildViewMatrix(transform());
            return vp_matrix;
        }

        inline comps::Transform& transform(){return (*ts)[mp_t];}
        inline comps::Viewer& viewer(){return (*vs)[mp_v];}
        inline comps::Projector& projector(){return (*ps)[mp_p];}

        Camera(const Camera&) = delete;
        Camera& operator=(const Camera&) = delete;
    };
};

#endif
