/**
 * @file renderer.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 尝试把renderer写出来
 * @version 5.0
 * @date 2026-05-30
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef TEST_RENDERER
#define TEST_RENDERER
#include <AGE/VAO.h>
#include <AGE/Model.h>
#include <AGE/Texture.h>
#include "app_comp.h"

namespace my_comps {
    struct ObjectRender{
        using Dependency = alib5::ecs::ComponentStack<LightMVP,Transform>;

        // 如果选择VAO，优先级会更加高一点
        enum RenderMode {
            DrawModel,
            DrawArray
        };

        RenderMode mode { DrawModel };
        struct {
            age::Model * model { nullptr };
        } model;

        struct {
            age::VAO target { };
            age::PrimitiveType primitive_type { age::PrimitiveType::Triangles };
            size_t start_index { 0 };
            size_t count { 0 };
        } array;

        GLenum front_face { GL_CCW };
        bool visible { true };
        age::material::Material * material { nullptr };
        age::Texture * texture { nullptr };

        alib5::ecs::ref_t<LightMVP> light_mvp;
        alib5::ecs::ref_t<Transform> transform;
        void bind_dep(auto & tup){
            auto & [l_mvp,trs] = tup;

            light_mvp = l_mvp;
            transform = trs;
        }

        void apply(ObjectRender rn){
            mode = rn.mode;
            model = rn.model;
            array = rn.array;
            front_face = rn.front_face;
            visible = rn.visible;
            material = rn.material;
            texture = rn.texture;
        }
    };
}

struct RenderSystem{
    struct Shadow{
        age::Window & win;
        EntityManager & em;
        age::ShaderUniform & shadowMVP;
        LightComponent & light_component;
    };

    static void render(
        Shadow shadow
    );

    struct Object{
        age::Window & win;
        EntityManager & em;
        LightComponent & light_component;
        age::ShaderUniform & invMV;
        age::ShaderUniform & mv_matrix;
        age::ShaderUniform & shadowMVP2;
        age::material::MaterialBindings & bindings;

        glm::mat4 camera_viewer_matrix;
    };

    static void render(
        Object object
    );
};
#endif