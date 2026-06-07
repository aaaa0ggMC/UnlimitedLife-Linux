#include "renderer.h"
#include <AGE/Model.h>

using namespace age;

void RenderSystem::render(
    RenderSystem::Object object
){
    static ComponentPool<my_comps::ObjectRender> & pool = *object.em.add_component_pool<my_comps::ObjectRender>();

    material::Material * latest_material = nullptr;
    age::Texture * latest_texture = nullptr;
    GLenum caching_front_face = 0;
    glm::mat4 lm;

    pool.data.for_each(
        [&](my_comps::ObjectRender & render) {
            if(render.visible){
                // 材质上传
                if(render.material && render.material != latest_material){
                    render.material->upload(object.bindings);
                    latest_material = render.material;
                }
                // 纹理上传
                if(render.texture && render.texture != latest_texture){
                    render.texture->bind(GL_TEXTURE0);
                    latest_texture = render.texture;
                }
                // 面元配置
                if(render.front_face != caching_front_face){
                    glFrontFace(render.front_face);
                    caching_front_face = render.front_face;
                }

                lm = object.camera_viewer_matrix * render.transform->buildModelMatrix();

                object.invMV.uploadmat4(glm::transpose(glm::inverse(lm)));
                object.mv_matrix.uploadmat4(lm);
                object.shadowMVP2.uploadmat4(render.light_mvp->build_light_mvp(object.light_component));

                if(render.mode == render.DrawModel && render.model.model ){
                    object.win.draw<age::Model>(*render.model.model);
                }else{
                    render.array.target.bind();
                    object.win.drawArray(
                        render.array.primitive_type,
                        render.array.start_index,
                        render.array.count
                    );
                }
            }
        }
    );
}

void RenderSystem::render(
    RenderSystem::Shadow shadow
){
    static ComponentPool<my_comps::ObjectRender> & pool = *shadow.em.add_component_pool<my_comps::ObjectRender>();

    GLenum caching_front_face = 0;
    pool.data.for_each(
        [&](my_comps::ObjectRender & render){
            if(render.visible){
                if(render.front_face != caching_front_face){
                    glFrontFace(render.front_face);
                    caching_front_face = render.front_face;
                }

                shadow.shadowMVP.uploadmat4(
                render.light_mvp->build_light_mvp(
                    shadow.light_component
                ));


                if(render.mode == render.DrawModel && render.model.model ){
                    shadow.win.draw<age::Model>(*render.model.model);
                }else{
                    render.array.target.bind();
                    shadow.win.drawArray(
                        render.array.primitive_type,
                        render.array.start_index,
                        render.array.count
                    );
                }
            }
        }
    );
}