#ifndef AGE_H_CAMERA
#define AGE_H_CAMERA
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
        comps::Transform * m_transform;
        comps::Viewer * m_viewer;
        comps::Projector * m_projector;

    public:
        glm::mat4 vp_matrix;

        inline Camera(EntityManager& iem):
        em{iem},
        cameraEntity{iem.createEntity(),iem}{
            m_transform = cameraEntity.add<comps::Transform>();
            m_viewer = cameraEntity.add<comps::Viewer>();
            m_projector = cameraEntity.add<comps::Projector>();

            m_transform->chain = (DirtyMarker*)this;
            m_projector->chain = (DirtyMarker*)this;
        }

        inline ~Camera(){
            cameraEntity.destroy();
        }

        inline glm::mat4& buildVPMatrix(){
            if(!dm_check())return vp_matrix;
            m_transform = cameraEntity.add<comps::Transform>();
            m_viewer = cameraEntity.add<comps::Viewer>();
            m_projector = cameraEntity.add<comps::Projector>();
            //std::cout << "Changed,so i updated" << std::endl;
            vp_matrix = m_projector->buildProjectionMatrix() * m_viewer->buildViewMatrix(*m_transform);
            return vp_matrix;
        }

        inline comps::Transform& transform(){return *m_transform;}
        inline comps::Viewer& viewer(){return *m_viewer;}
        inline comps::Projector& projector(){return *m_projector;}

        Camera(const Camera&) = delete;
        Camera& operator=(const Camera&) = delete;
    };
};

#endif
