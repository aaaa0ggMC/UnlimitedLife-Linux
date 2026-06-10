/**
 * @file Context.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 对OpenGL的各种状态设置进行包装，同时也包含了部分设置，包含这个文件等于包含了一个基础的图形环境
 * @version 5.0
 * @date 2026-06-09
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef AGE_CONTEXT_H
#define AGE_CONTEXT_H
#include <AGE/Utils.h>
#include <AGE/Details/ContextDetail.h>

namespace age {

    /// 如果Context和OpenGL本身的设置函数进行混用，那么造成的不同步问题就需要自己承受了
    /// 建议在正式使用Context之前手动进行一下sync
    struct AGE_API Context{
    private:
        std::vector<context::ViewportInfo> viewports;
        std::vector<context::DrawBuffer> drawbuffers;
        context::Depth m_depth;
        context::CullFace m_cull_face;
        context::FrontFaceDirection m_front_face;
        context::PolygonOffset m_polygon_offset;
        context::PolygonMode m_polygon_mode;
        GLfloat m_point_size;
        
    public:
        /// 构造函数
        Context(){}
        /// 从OpenGL那里尽可能拉取所有信息，开销比较大，不建议高频率使用
        void sync();

        Context& cull_face_test(bool value) {
            if(value != m_cull_face.enabled) {
                m_cull_face.enabled = value;
                if(value) glEnable(GL_CULL_FACE);
                else glDisable(GL_CULL_FACE);
            }
            return *this;
        }

        Context& cull_face_mode(context::CullFaceMode mode) {
            if(mode != m_cull_face.mode) {
                m_cull_face.mode = mode;
                glCullFace(static_cast<GLenum>(mode));
            }
            return *this;
        }

        Context& cull_face(context::CullFace c) {
            return cull_face_test(c.enabled).cull_face_mode(c.mode);
        }

        context::CullFace get_cull_face() const {
            return m_cull_face;
        }

        Context& front_face(context::FrontFaceDirection dir) {
            if(dir != m_front_face) {
                m_front_face = dir;
                glFrontFace(static_cast<GLenum>(dir));
            }
            return *this;
        }

        context::FrontFaceDirection get_front_face() const {
            return m_front_face;
        }

        Context& polygon_offset_fill(bool value) {
            if(value != m_polygon_offset.fill_enabled) {
                m_polygon_offset.fill_enabled = value;
                if(value) glEnable(GL_POLYGON_OFFSET_FILL);
                else glDisable(GL_POLYGON_OFFSET_FILL);
            }
            return *this;
        }

        Context& polygon_offset_line(bool value) {
            if(value != m_polygon_offset.line_enabled) {
                m_polygon_offset.line_enabled = value;
                if(value) glEnable(GL_POLYGON_OFFSET_LINE);
                else glDisable(GL_POLYGON_OFFSET_LINE);
            }
            return *this;
        }

        Context& polygon_offset_point(bool value) {
            if(value != m_polygon_offset.point_enabled) {
                m_polygon_offset.point_enabled = value;
                if(value) glEnable(GL_POLYGON_OFFSET_POINT);
                else glDisable(GL_POLYGON_OFFSET_POINT);
            }
            return *this;
        }

        Context& polygon_offset_params(GLfloat factor, GLfloat units) {
            if(factor != m_polygon_offset.factor || units != m_polygon_offset.units) {
                m_polygon_offset.factor = factor;
                m_polygon_offset.units = units;
                glPolygonOffset(factor, units);
            }
            return *this;
        }

        Context& polygon_offset(context::PolygonOffset po) {
            return polygon_offset_fill(po.fill_enabled)
                  .polygon_offset_line(po.line_enabled)
                  .polygon_offset_point(po.point_enabled)
                  .polygon_offset_params(po.factor, po.units);
        }

        context::PolygonOffset get_polygon_offset() const {
            return m_polygon_offset;
        }

        Context& polygon_mode(context::PolygonFace face, context::PolygonModeEnum mode) {
            bool need_update = false;

            if(face == context::PolygonFace::Front || face == context::PolygonFace::FrontAndBack) {
                if(m_polygon_mode.front != mode) {
                    m_polygon_mode.front = mode;
                    need_update = true;
                }
            }

            if(face == context::PolygonFace::Back || face == context::PolygonFace::FrontAndBack) {
                if(m_polygon_mode.back != mode) {
                    m_polygon_mode.back = mode;
                    need_update = true;
                }
            }

            if(need_update) {
                glPolygonMode(static_cast<GLenum>(face), static_cast<GLenum>(mode));
            }
            return *this;
        }

        /// 结构体版本，直接全量同步（自动优化组合）
        Context& polygon_mode(context::PolygonMode mode) {
            if (mode.front == mode.back) {
                // 如果两面模式相同，合并为一次 API 调用
                polygon_mode(context::PolygonFace::FrontAndBack, mode.front);
            } else {
                // 如果两面模式不同，拆分为两次 API 调用
                polygon_mode(context::PolygonFace::Front, mode.front);
                polygon_mode(context::PolygonFace::Back, mode.back);
            }
            return *this;
        }

        context::PolygonMode get_polygon_mode() const {
            return m_polygon_mode;
        }

        Context& point_size(GLfloat size) {
            if(size != m_point_size) {
                m_point_size = size;
                glPointSize(size);
            }
            return *this;
        }

        GLfloat get_point_size() const {
            return m_point_size;
        }

        Context& depth_test(bool value){
            if(value != m_depth.enabled){
                m_depth.enabled = value;

                if(value) glEnable(GL_DEPTH_TEST);
                else glDisable(GL_DEPTH_TEST);
            }
            return *this;
        }

        Context& depth_func(GLint func){
            if(func != m_depth.function){
                m_depth.function = func;
                glDepthFunc(func);
            }
            return *this;
        }

        Context& depth_mask(GLboolean mask){
            if(mask != m_depth.mask){
                m_depth.mask = mask;
                glDepthMask(mask);
            }
            return *this;
        }

        Context& depth(context::Depth d){
            return depth_test(d.enabled).depth_func(d.function).depth_mask(d.mask);
        }

        context::Depth get_depth() const {
            return m_depth;
        }

        context::ViewportInfo get_viewport(size_t index = 0) const {
            panicf_if(
                index >= viewports.size(),
                "Index {} out of range for viewports({}).",
                index,
                viewports.size()
            );
            return viewports[index];
        }

        context::DrawBuffer get_drawbuffer(size_t index) const {
            panicf_if(
                index >= drawbuffers.size(),
                "Index {} out of range for draw buffers({}).",
                index,
                drawbuffers.size()
            );
            return drawbuffers[index];
        }

        Context& viewport(int x,int y,size_t width,size_t height,size_t index = 0){
            auto cached_viewport = get_viewport(index);

            viewports[index] = {
                x,y,width,height
            };
            
            if(viewports[index] != cached_viewport){
                glViewportIndexedf(index,x,y,width,height);
            }
            return *this;
        }

        /// 仅仅支持设置全局drawbuffer属性，设置framebuffer的直接framebuffer.color .depth xxx就行
        Context& drawbuffer(context::DrawBuffer new_buffer,size_t index = 0){
            auto old = get_drawbuffer(index);

            if(new_buffer != old){
                drawbuffers[index] = new_buffer;
                glNamedFramebufferDrawBuffers(0,drawbuffers.size(),(const GLenum *)drawbuffers.data());
            }

            return *this;
        }

        Context& drawbuffer_override(std::vector<context::DrawBuffer> fully_override,size_t begin_index = 0){
            bool need_upload = false;
            for(size_t index = 0; index < fully_override.size() && index + begin_index < drawbuffers.size(); ++index){
                auto old = drawbuffers[index + begin_index];
                if(old != fully_override[index]){
                    drawbuffers[index + begin_index] = fully_override[index];
                    need_upload = true;
                }
            }

            if(need_upload){
                glNamedFramebufferDrawBuffers(0,drawbuffers.size(),(const GLenum *)drawbuffers.data());
            }
            return *this;
        }

    };

};
#endif