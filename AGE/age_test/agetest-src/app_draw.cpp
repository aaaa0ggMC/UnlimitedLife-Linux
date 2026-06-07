#include "app.h"
#include "app_comp.h"
#include "renderer.h"

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
    glCullFace(GL_FRONT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    RenderSystem::render(RenderSystem::Shadow{
        .win = win,
        .em = em,
        .shadowMVP = shadowMVP,
        .light_component = lc,
    });

    shadowMap.unbind();

    glCullFace(GL_BACK);
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
    // GL statuses //
    if(state.gl_depth){
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(gl_depthfunc_enums[state.gl_depthfunc_index]);
    }else glDisable(GL_DEPTH_TEST);
    
    if(state.gl_cull)glEnable(GL_CULL_FACE);
    else glDisable(GL_CULL_FACE);
    
    glPointSize(state.point_size);
    glPolygonMode(gl_polygon_face_enums[state.gl_polygon_face_index],gl_polygon_mode_enums[state.gl_polygon_mode_index]);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f,4.0f);
    projectionMatrix.uploadmat4(cam.projector().buildProjectionMatrix());

    RenderSystem::render(RenderSystem::Object{
        .win = win,
        .em = em,
        .light_component = lc,
        .invMV = invMV,
        .mv_matrix = mv_matrix,
        .shadowMVP2 = shadowMVP2,
        .bindings = mb,
        .camera_viewer_matrix = cam.viewer().buildViewMatrix(cam.transform()),
    });

    m_sampler.unbind(GL_TEXTURE0);
    shadowSampler.unbind(GL_TEXTURE1);
}