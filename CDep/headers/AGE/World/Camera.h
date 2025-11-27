/**
 * @file Camera.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 相机，对组件的一个包装
 * @version 0.1
 * @date 2025/11/27
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/06/24 
 */
#ifndef AGE_H_CAMERA
#define AGE_H_CAMERA
#include <AGE/Utils.h>
#include <AGE/World/Components.h>

namespace age::world{
    using namespace alib::g3::ecs;
    using namespace comps;

    struct Camera : public DirtyMarker,public NonCopyable{
#ifndef AGE_EM_DEBUG
    private:
#endif
        // 确保初始化顺序！
        EntityManager& em;
        EntityWrapper cameraEntity;
        
        ref_t<comps::Transform> tran;
        ref_t<comps::Viewer> view;
        ref_t<comps::Projector> proj;

    public:
        glm::mat4 vp_matrix;

        inline Camera(EntityManager& iem)
        :em{iem}
        ,cameraEntity{iem}
        ,tran{cameraEntity.add<comps::Transform>()}
        ,view{cameraEntity.add<comps::Viewer>()}
        ,proj{cameraEntity.add<comps::Projector>()}{
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

        inline comps::Transform& transform(){return tran.get();}
        inline comps::Viewer& viewer(){return view.get();}
        inline comps::Projector& projector(){return proj.get();}
    };
};

#endif
