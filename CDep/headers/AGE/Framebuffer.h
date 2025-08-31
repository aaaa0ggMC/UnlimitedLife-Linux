/**
 * @file Framebuffer.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 帧缓冲区的创建
 * @version 0.1
 * @date 2025/08/31
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/08/31 
 */
#ifndef AGE_H_FB
#define AGE_H_FB
#include <AGE/Utils.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <AGE/Texture.h>

namespace age{
    struct AGE_API FrameBuffer{
    private:
        friend class Application;
        inline FrameBuffer(){} 
        GLuint framebuffer_id {0};
        std::optional<Texture*> texture {std::nullopt<Texture*>};
    public:
        inline void bind(){
            glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_id);
        }
        /// note that unbind doesnt mean that there no fb working,it just switched the active fb to the default one
        inline void unbind(){
            glBindFramebuffer(GL_FRAMEBUFFER,0);
        }
    };

    struct AGE_API CreateFrameBufferInfo{
        unsigned int width; ///< leave value 0 to use the width value of ur monitor
        unsigned int height; ///< leave value 0 to use the height value of ur monitor
        std::string sid;
        /// @TODO WIP DATA

        CreateFrameBufferInfo();
    };
};

#endif