#include "app.h"
#include "app_comp.h"

void MainApplication::draw(){
    Window & win = *m_window;
    Sampler & sampler = m_sampler;   
    win.clear();
    draw_pass_one();
    draw_callback();
    draw_pass_two();
}

void MainApplication::draw_pass_one(){
    Window & win = *m_window;
    Camera &cam = state.use_light_cam? e_light : camera;
    LightComponent & lc = e_light.get<LightComponent>()->get();

    glViewport(0,0,shadowTex->getTextureInfo().width,shadowTex->getTextureInfo().height);
    shadowMap.bind();
    shadowShader.bind();
    glDrawBuffer(GL_NONE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glm::vec4 pos = cam.viewer().buildViewMatrix(cam.transform()) * glm::vec4(e_light.transform().m_position,1.0);
    lb.position.safe_upload(glm::vec3(pos));

    if(state.show_cube){
        glFrontFace(GL_CCW);
        shadowMVP.uploadmat4((*cube.get<LightMVP>())->build_light_mvp(lc));
        win.draw<Model>(models["cube"]);
    }

    if(state.show_pyramid){
        vaos[1].bind();
        glFrontFace(GL_CCW);
        shadowMVP.uploadmat4((*pyramid.get<LightMVP>())->build_light_mvp(lc));
        win.drawArray(PrimitiveType::Triangles,0,36);
    }

    if(state.show_model){
        glFrontFace(GL_CCW);
        shadowMVP.uploadmat4((*invPar.get<LightMVP>())->build_light_mvp(lc));
        win.draw<Model>(*current_model);
    }

    if(state.show_model){
        glFrontFace(GL_CCW);
        shadowMVP.uploadmat4((*root.get<LightMVP>())->build_light_mvp(lc));
        win.draw<Model>(*current_model);
    }

    if(state.show_platform){
        glFrontFace(GL_CCW);
        shadowMVP.uploadmat4((*plane.get<LightMVP>())->build_light_mvp(lc));
        win.draw<Model>(m_plane);
    }

    shadowMap.unbind();
}

void MainApplication::draw_callback(){
    glViewport(0,0,shadowTexCallback->getTextureInfo().width,shadowTexCallback->getTextureInfo().height);
    shadowMapCallback.bind();
    callbackShader.bind();
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    shadowSampler.unbind(GL_TEXTURE1);
    shadowTex->bind(GL_TEXTURE1);

    m_window->drawArray(PrimitiveType::Triangles,0,6);
    shadowMapCallback.unbind();
    shadowSampler.unbind(GL_TEXTURE1);
}

void MainApplication::draw_pass_two(){
    Window & win = *m_window;
    LightComponent & lc = e_light.get<LightComponent>()->get();
    Camera &cam = state.use_light_cam? e_light : camera;
    
    glViewport(0,0,win.getFrameBufferSize().x,win.getFrameBufferSize().y);
    glDrawBuffer(GL_BACK);
    shader.bind();
    m_sampler.bind(GL_TEXTURE0);
    shadowSampler.bind(GL_TEXTURE1);
    shadowTex->bind(GL_TEXTURE1);
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
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f,4.0f);

    projectionMatrix.uploadmat4(cam.projector().buildProjectionMatrix());
    mat_gold.upload(mb);
    /// Cube
    if(state.show_cube){
        glFrontFace(GL_CCW);
        // vaos[0].bind(); 绘制model不需要bind vao
        auto lm = cam.viewer().buildViewMatrix(cam.transform()) * cube.transform().buildModelMatrix();
        invMV.uploadmat4(glm::transpose(glm::inverse(lm)));
        mv_matrix.uploadmat4(lm);
        shadowMVP2.uploadmat4((*cube.get<LightMVP>())->build_light_mvp(lc));

        win.draw<Model>(models["cube"]);
        // win.drawArray(PrimitiveType::Triangles,0,36,1);
    }

    /// Pyramid
    if(state.show_pyramid){
        vaos[1].bind();
        glFrontFace(GL_CCW);
        auto lm = cam.viewer().buildViewMatrix(cam.transform()) * pyramid.transform().buildModelMatrix();
        invMV.uploadmat4(glm::transpose(glm::inverse(lm)));
        mv_matrix.uploadmat4(lm);
        shadowMVP2.uploadmat4((*pyramid.get<LightMVP>())->build_light_mvp(lc));

        win.drawArray(PrimitiveType::Triangles,0,36);
    }

    /// Current Model
    if(state.show_model){
        glFrontFace(GL_CCW);
        auto lm = cam.viewer().buildViewMatrix(cam.transform()) * invPar.transform().buildModelMatrix();
        invMV.uploadmat4(glm::transpose(glm::inverse(lm)));
        mv_matrix.uploadmat4(lm);
        shadowMVP2.uploadmat4((*invPar.get<LightMVP>())->build_light_mvp(lc));

        win.draw<Model>(*current_model);
    }

    /// With Root
    if(state.show_model){
        glFrontFace(GL_CCW);
        auto lm = cam.viewer().buildViewMatrix(cam.transform()) * root.transform().buildModelMatrix();
        invMV.uploadmat4(glm::transpose(glm::inverse(lm)));
        mv_matrix.uploadmat4(lm);
        shadowMVP2.uploadmat4((*root.get<LightMVP>())->build_light_mvp(lc));

        win.draw<Model>(*current_model);
    }

    /// Plane
    (*app.textures.get("wall"))->bind(GL_TEXTURE0);
    mat_jade.upload(mb);
    if(state.show_platform){
        glFrontFace(GL_CCW);
        auto lm = cam.viewer().buildViewMatrix(cam.transform()) * plane.transform().buildModelMatrix();
        invMV.uploadmat4(glm::transpose(glm::inverse(lm)));
        mv_matrix.uploadmat4(lm);
        shadowMVP2.uploadmat4((*plane.get<LightMVP>())->build_light_mvp(lc));

        win.draw<Model>(m_plane);
    }

    m_sampler.unbind(GL_TEXTURE0);
    shadowSampler.unbind(GL_TEXTURE1);
}