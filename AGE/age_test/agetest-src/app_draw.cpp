#include "app.h"

void MainApplication::draw(){
    Window & win = *m_window;
    Sampler & sampler = m_sampler;   

    win.clear();
    shader.bind();
    sampler.bind(GL_TEXTURE0);
    // 绑定首选纹理
    (*app.textures.get(cfg.texture_sids[state.current_texture_id]))->bind(GL_TEXTURE0);
    // GL statuses //
    if(state.gl_depth){
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(cfg.gl_depthfunc_enums[state.gl_depthfunc_index]);
    }else glDisable(GL_DEPTH_TEST);
    if(state.gl_cull)glEnable(GL_CULL_FACE);
    else glDisable(GL_CULL_FACE);
    glPointSize(state.point_size);
    glPolygonMode(cfg.gl_polygon_face_enums[state.gl_polygon_face_index],cfg.gl_polygon_mode_enums[state.gl_polygon_mode_index]);

    mat_gold.upload(mb);
    /// Cube
    if(state.show_cube){
        glFrontFace(GL_CCW);
        // vaos[0].bind(); 绘制model不需要bind vao
        auto lm = camera.viewer().buildViewMatrix(camera.transform()) * cube.transform().buildModelMatrix();
        invMV.uploadmat4(glm::transpose(glm::inverse(lm)));
        mv_matrix.uploadmat4(lm);

        win.draw<Model>(models["cube"]);
        // win.drawArray(PrimitiveType::Triangles,0,36,1);
    }

    /// Pyramid
    if(state.show_pyramid){
        vaos[1].bind();
        glFrontFace(GL_CCW);
        auto lm = camera.viewer().buildViewMatrix(camera.transform()) * pyramid.transform().buildModelMatrix();
        invMV.uploadmat4(glm::transpose(glm::inverse(lm)));
        mv_matrix.uploadmat4(lm);
        win.drawArray(PrimitiveType::Triangles,0,36);
    }

    /// Current Model
    if(state.show_model){
        glFrontFace(GL_CCW);
        auto lm = camera.viewer().buildViewMatrix(camera.transform()) * invPar.transform().buildModelMatrix();
        invMV.uploadmat4(glm::transpose(glm::inverse(lm)));
        mv_matrix.uploadmat4(lm);
        win.draw<Model>(*current_model);
    }

    /// With Root
    if(state.show_model){
        glFrontFace(GL_CCW);
        auto lm = camera.viewer().buildViewMatrix(camera.transform()) * root.transform().buildModelMatrix();
        invMV.uploadmat4(glm::transpose(glm::inverse(lm)));
        mv_matrix.uploadmat4(lm);
        win.draw<Model>(*current_model);
    }

    /// Plane
    (*app.textures.get("wall"))->bind(GL_TEXTURE0);
    mat_jade.upload(mb);
    if(state.show_platform){
        glFrontFace(GL_CCW);
        auto lm = camera.viewer().buildViewMatrix(camera.transform()) * plane.transform().buildModelMatrix();
        invMV.uploadmat4(glm::transpose(glm::inverse(lm)));
        mv_matrix.uploadmat4(lm);
        win.draw<Model>(m_plane);
    }
}