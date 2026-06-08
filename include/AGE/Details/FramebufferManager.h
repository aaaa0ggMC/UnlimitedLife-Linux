#ifndef AGE_MFRAMEBUFFER_H_INCLUDED
#define AGE_MFRAMEBUFFER_H_INCLUDED
#include <AGE/Utils.h>
#include <AGE/Details/TextureManager.h>
#include <AGE/Details/SamplerManager.h>
#include <AGE/Details/CSBuffer.h>
#include <AGE/Framebuffer.h>

namespace age::manager{

    struct AGE_API FramebufferManager{
    private:
        detail::ConstantStringBuffer& csbuffer;
        TextureManager & tm;
        SamplerManager & sm;

        std::unordered_map<std::string_view,Framebuffer> framebuffers;
    public:
        FramebufferManager(detail::ConstantStringBuffer&cb,TextureManager& t,SamplerManager & s):
        csbuffer{cb},
        tm{t},
        sm{s}{}
        ~FramebufferManager();

        Framebuffer create(const CreateFramebufferInfo & info);
        std::optional<Framebuffer> get(std::string_view sid);
        bool destroy(std::string_view sid);

        inline bool has(std::string_view sid){
            return framebuffers.find(sid) != framebuffers.end();
        }

        inline bool destroy(Framebuffer & fb){
            return destroy(fb.sid);
        }

        /// @brief 创建一个阴影FBO，会顺便创建纹理，sid共用
        /// @param sid 共用的sid
        inline std::tuple<Framebuffer,Texture*,Sampler> createShadowMap(std::string_view sid,size_t width,size_t height,GLenum fmt = GL_DEPTH_COMPONENT32){
            panic_debug(!width || !height,"Width or height cant be zero!");
            auto f_it = framebuffers.find(sid);
            if(f_it != framebuffers.end() || tm.has(sid) || sm.has(sid)){
                panicf_debug(true,"Conflict sid[{}] was passed!",sid);
                Error::def.pushMessage({AGEE_CONFLICT_SID,"Conflict sid was passed!"});
                return std::make_tuple(Framebuffer(),nullptr,Sampler());
            }
            CreateTextureInfo t;
            t.source = t.CreateEmpty;
            t.sid = sid;
            t.empty.width = width;
            t.empty.height = height;
            t.internalFormat = fmt;
            t.genMipmap = false;
            t.uploadToOpenGL = true;
            t.format = GL_DEPTH_COMPONENT;
            auto texture = tm.create(t);
            [[unlikely]] if(!texture){
                Error::def.pushMessage({AGEE_OPENGL_CREATE_ERROR,"Cannot create relevant texture!"});
                return std::make_tuple(Framebuffer(),nullptr,Sampler());
            }
            auto sp = sm.create(sid);
            [[unlikely]] if(!sp){
                Error::def.pushMessage({AGEE_OPENGL_CREATE_ERROR,"Cannot create relevant sampler!"});
                tm.destroy(**texture);
                return std::make_tuple(Framebuffer(),nullptr,Sampler());
            }
            sp->magFilter(SamplerInfo::MagFilter::Linear);
            sp->minFilter(SamplerInfo::MinFilter::Linear);
            sp->compareMode(SamplerInfo::CompareMode::RefToTexture);
            sp->compareFunc(SamplerInfo::CompareFunc::LEqual);
            CreateFramebufferInfo f;
            f.sid = sid;
            if(texture)f.depth = **texture;
            return std::make_tuple(create(f),*texture,*sp);
        }
    };

};

#endif