/**
 * @file Framebuffer.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 帧缓冲区的创建
 * @version 0.1
 * @date 2025/12/02
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/08/31 
 */
#ifndef AGE_H_FB
#define AGE_H_FB
#include <variant>
#include <AGE/Utils.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <AGE/Texture.h>

namespace age::manager{
    class FramebufferManager;
}
namespace age{
    struct AGE_API FBOAttachment{
        std::variant<
            std::monostate,
            Texture*
        > data;

        FBOAttachment& operator=(Texture & tex){
            data = (&tex);
            return *this;
        }

        FBOAttachment& operator=(const std::monostate & ){data = std::monostate{}; return *this;}
        FBOAttachment& operator=(const std::monostate &&){data = std::monostate{}; return *this;}

        /// @brief 传入的参数不需要你自己再补上指针
        template<class T> bool has() const{
            static_assert(!std::is_pointer_v<T>);
            return std::get_if<T*>(&data) != nullptr;
        }

        /// @brief 传入的参数不需要你自己再补上指针
        template<class T> T* get() const{
            static_assert(!std::is_pointer_v<T>);
            if constexpr(std::is_same_v<T,std::monostate>){
                return empty();
            }else return *std::get_if<T*>(&data);
        }

        bool empty() const{
            return data.index() == 0;
        }
    };

    struct AGE_API Framebuffer{
    private:
        friend class age::manager::FramebufferManager;
        GLuint framebuffer_id {0};
        std::string_view sid { "" };
        
        std::vector<FBOAttachment> m_colors;
        FBOAttachment m_depth;
        FBOAttachment m_stencil;

        inline void set_attachment(FBOAttachment attachment,GLenum target,GLuint index = 0,GLuint level = 0){
            if(!attachment.empty()){
                Texture* texture_attach = attachment.get<Texture>();
                if(texture_attach){
                    glNamedFramebufferTexture(
                        framebuffer_id,target + index,
                        texture_attach->getId(),level);
                }// else if(renderbuffer_attach)
                else{
                    panic_debug(true, "FBOAttachment contains unsupported or null type!");
                    glNamedFramebufferTexture(
                        framebuffer_id,target + index,0,0);
                }
            }else {
                glNamedFramebufferTexture(
                    framebuffer_id,target + index,0,0);
            }
        }

    public:
        inline Framebuffer(){} 
        inline bool valid(){
            return framebuffer_id == 0;
        }

        std::string_view get_sid(){
            return sid;
        }

        std::vector<FBOAttachment> get_colors(){
            return m_colors;
        }

        FBOAttachment get_depth(){
            return m_depth;
        }

        FBOAttachment get_stencil(){
            return m_stencil;
        }

        inline void color(const FBOAttachment attachment,GLuint index = 0,GLuint level = 0,bool auto_update = true){
            panic_debug(framebuffer_id == 0,"Invalid framebuffer id!");
            if(index >= m_colors.size()){
                m_colors.resize(index + 1);
            }
            m_colors[index] = attachment;
            set_attachment(attachment,GL_COLOR_ATTACHMENT0,index,level);
            if(auto_update)updateDrawBuffers();
        }

        inline void colors(std::span<const FBOAttachment> attachments,GLuint beg_index = 0,GLuint level = 0,bool auto_update = true){
            panic_debug(framebuffer_id == 0,"Invalid framebuffer id!");
            if(beg_index + attachments.size() >= m_colors.size()){
                m_colors.resize(beg_index + attachments.size() + 1);
            }
            for(size_t i = 0;i < attachments.size();++i){
                m_colors[i + beg_index] = attachments[i];
                set_attachment(attachments[i],GL_COLOR_ATTACHMENT0 + beg_index,i,level);
            }
            if(auto_update)updateDrawBuffers();
        }

        inline void depth(const FBOAttachment i_dep,GLuint level = 0){
            panic_debug(framebuffer_id == 0,"Invalid framebuffer id!");
            m_depth = i_dep;
            set_attachment(i_dep,GL_DEPTH_ATTACHMENT,0,level);
        }

        inline void stencil(FBOAttachment i_stencil,GLuint level = 0){
            panic_debug(framebuffer_id == 0,"Invalid framebuffer id!");
            m_stencil = i_stencil;
            set_attachment(i_stencil,GL_STENCIL_ATTACHMENT,0,level);
        }

        inline void updateDrawBuffers(){
            std::vector<GLenum> buffers;
            // color
            for(size_t i = 0;i < m_colors.size();++i){
                if(!m_colors[i].empty())buffers.emplace_back(GL_COLOR_ATTACHMENT0 + i);
            }
            glNamedFramebufferDrawBuffers(framebuffer_id,buffers.size(),buffers.data());
        }

        inline void bind(){
            panic_debug(framebuffer_id == 0,"Invalid framebuffer id!");
            glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_id);
        }
        /// note that unbind doesnt mean that there no fb working,it just switched the active fb to the default one
        inline void unbind(){
            panic_debug(framebuffer_id == 0,"Invalid framebuffer id!");
            glBindFramebuffer(GL_FRAMEBUFFER,0);
        }
    };

    struct AGE_API CreateFramebufferInfo{        
        std::string sid { "" };

        std::vector<FBOAttachment> colors;
        GLuint base_index { 0 };
        FBOAttachment depth;
        FBOAttachment stencil;
    };
};

#endif