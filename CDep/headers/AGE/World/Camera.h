#ifndef AGE_H_CAMERA
#define AGE_H_CAMERA
#include "Components.h"
#include <AGE/Base.h>
#include <AGE/World/EntityManager.h>
#include <AGE/World/Components.h>

namespace age::world{
    using namespace comps;
    struct Camera : public DirtyMarker,public NonCopyable{
#ifndef AGE_EM_DEBUG
    private:
#endif
        EntityManager& em;
        EntityWrapper cameraEntity;

        ComponentWrapper<comps::Transform> tran;
        ComponentWrapper<comps::Viewer> view;
        ComponentWrapper<comps::Projector> proj;

    public:
        glm::mat4 vp_matrix;

        inline Camera(EntityManager& iem):
        em{iem},
        cameraEntity{iem.createEntity(),iem}{
            cameraEntity.add<comps::Transform>();
            cameraEntity.add<comps::Viewer>();
            cameraEntity.add<comps::Projector>();

            tran.build(iem,cameraEntity.e.id);
            view.build(iem,cameraEntity.e.id);
            proj.build(iem,cameraEntity.e.id);

            tran->chain = (DirtyMarker*)this;
            proj->chain = (DirtyMarker*)this;

            vp_matrix = glm::mat4(1.0f);
        }

        inline ~Camera(){
            cameraEntity.destroy();
        }

        inline glm::mat4& buildVPMatrix(){
            if(!dm_check())return vp_matrix;
            vp_matrix = projector().buildProjectionMatrix() * viewer().buildViewMatrix(transform());
            return vp_matrix;
        }

        inline comps::Transform& transform(){return *tran;}
        inline comps::Viewer& viewer(){return *view;}
        inline comps::Projector& projector(){return *proj;}
    };
};

#endif
