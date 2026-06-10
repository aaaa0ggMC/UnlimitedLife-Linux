#include "app.h"
#include "app_comp.h"
#include "renderer.h"
using namespace age::context;

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

    context
        .viewport(0,0,shadowTex->getTextureInfo().width,shadowTex->getTextureInfo().height)
        .cull_face_test(true)
        .cull_face_mode(CullFaceMode::Front)
        .depth_test(true)
        .depth_func(GL_LEQUAL);
    
    shadowMap.bind();
    glClear(GL_DEPTH_BUFFER_BIT);

    shadowShader.bind();
    
    RenderSystem::render(RenderSystem::Shadow{
        .win = win,
        .em = em,
        .shadowMVP = shadowMVP,
        .light_component = lc,
    });

    shadowMap.unbind();
}

void MainApplication::draw_callback(){
    context
        .viewport(0,0,shadowTexCallback->getTextureInfo().width,shadowTexCallback->getTextureInfo().height)
        .cull_face_test(false)
        .depth_test(true)
        .depth_func(GL_LEQUAL);

    shadowMapCallback.bind();

    callbackShader.bind();
    shadowSampler.bind(GL_TEXTURE1);
    shadowTex->bind(GL_TEXTURE1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_window->drawArray(PrimitiveType::Triangles,0,6);

    shadowMapCallback.unbind();
    shadowSampler.unbind(GL_TEXTURE1);
}

void MainApplication::draw_pass_two(){
    Window & win = *m_window;
    LightComponent & lc = e_light.get<LightComponent>()->get();
    Camera &cam = state.use_light_cam? e_light : camera;
    
    context
        .viewport(0,0,win.getFrameBufferSize().x,win.getFrameBufferSize().y)
        .drawbuffer(DrawBuffer::Back)
        .depth_test(state.gl_depth)
        .depth_func(gl_depthfunc_enums[state.gl_depthfunc_index])
        .cull_face_test(state.gl_cull)
        .cull_face_mode(CullFaceMode::Back)
        .point_size(state.point_size)
        .polygon_mode((context::PolygonFace)gl_polygon_face_enums[state.gl_polygon_face_index],(context::PolygonModeEnum)gl_polygon_mode_enums[state.gl_polygon_mode_index])
        .polygon_offset_fill(true)
        .polygon_offset_params(2.0f,4.0f);
    
    shader.bind();
    m_sampler.bind(GL_TEXTURE0);
    shadowSampler.bind(GL_TEXTURE1);
    shadowTex->bind(GL_TEXTURE1);
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