#include <AGE/Context.h>
using namespace age;

void Context::sync(){
    // viewports 
    {   
        int viewport_count = 0;
        glGetIntegerv(GL_MAX_VIEWPORTS, &viewport_count);
        viewports.resize(viewport_count);

        int data[4] = {0};
        for(size_t index = 0;index < viewports.size();++index){
            glGetIntegeri_v(GL_VIEWPORT,(GLint)index,data);

            viewports[index] = {
                data[0],
                data[1],
                (size_t)data[2],
                (size_t)data[3]
            };
        }
    }
    // draw buffer
    {
        GLint max_db = 1;
        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &max_db);
        drawbuffers.resize(max_db);

        // 获取当前绑定的是哪个 FBO
        GLint current_fbo = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &current_fbo);

        // 如果当前绑定的不是 FBO 0，则临时切回 FBO 0
        if(current_fbo != 0){
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        }

        // 现在可以安全地拉取 FBO 0 的数据了
        for(size_t index = 0; index < max_db; ++index){
            GLint buffer_val = GL_NONE;
            // 查询
            glGetIntegerv(GL_DRAW_BUFFER0 + index, &buffer_val);
            // 安全的类型转换并写入缓存
            drawbuffers[index] = static_cast<context::DrawBuffer>(buffer_val);
        }

        // 恢复使用者原本绑定的 FBO (恢复现场)
        if(current_fbo != 0){
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current_fbo);
        }
    }
    m_depth.enabled = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
    glGetIntegerv(GL_DEPTH_FUNC, &m_depth.function);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &m_depth.mask);

    m_cull_face.enabled = glIsEnabled(GL_CULL_FACE) == GL_TRUE;
    GLint cull_mode = GL_BACK;
    glGetIntegerv(GL_CULL_FACE_MODE, &cull_mode);
    m_cull_face.mode = static_cast<context::CullFaceMode>(cull_mode);

    GLint front_face_val = GL_CCW;
    glGetIntegerv(GL_FRONT_FACE, &front_face_val);
    m_front_face = static_cast<context::FrontFaceDirection>(front_face_val);

    m_polygon_offset.fill_enabled = glIsEnabled(GL_POLYGON_OFFSET_FILL) == GL_TRUE;
    m_polygon_offset.line_enabled = glIsEnabled(GL_POLYGON_OFFSET_LINE) == GL_TRUE;
    m_polygon_offset.point_enabled = glIsEnabled(GL_POLYGON_OFFSET_POINT) == GL_TRUE;
    glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &m_polygon_offset.factor);
    glGetFloatv(GL_POLYGON_OFFSET_UNITS, &m_polygon_offset.units);

    GLint poly_mode[2] = { GL_FILL, GL_FILL };
    glGetIntegerv(GL_POLYGON_MODE, poly_mode);
    m_polygon_mode.front = static_cast<context::PolygonModeEnum>(poly_mode[0]);
    m_polygon_mode.back  = static_cast<context::PolygonModeEnum>(poly_mode[1]);

    glGetFloatv(GL_POINT_SIZE, &m_point_size);
}