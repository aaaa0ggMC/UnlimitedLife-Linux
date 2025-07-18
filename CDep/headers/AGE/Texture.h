#ifndef AGE_H_TEXTURE
#define AGE_H_TEXTURE
#include <AGE/Utils.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <AGE/Query.h>

namespace age{
    struct AGE_API TextureInfo{
        std::string filePath; ///< empty if loaded from memory
        bool loadedFromMemory;

        //图片属性
        int width;
        int height;
        int channels;
        bool mipmap;

        //上传属性
        bool uploaded;
        unsigned char * bits; //要是uploaded为true,保证为nullptr
    };

    struct AGE_API SamplerInfo{
        enum class WrapMethod : GLenum {
            Repeat = GL_REPEAT,
            MirroredRepeat = GL_MIRRORED_REPEAT,
            ClampToEdge = GL_CLAMP_TO_EDGE,
            ClampToBorder = GL_CLAMP_TO_BORDER
        };
        enum class MinFilter : GLenum{
            Nearest = GL_NEAREST,
            Linear = GL_LINEAR,
            Nearest_Mipmap_Nearest = GL_NEAREST_MIPMAP_NEAREST,
            Nearest_Mipmap_Linear = GL_NEAREST_MIPMAP_LINEAR,
            Linear_Mipmap_Nearest = GL_LINEAR_MIPMAP_NEAREST,
            Linear_Mipmap_Linear = GL_LINEAR_MIPMAP_LINEAR
        };
        enum class MagFilter : GLenum{
            Nearest = GL_NEAREST,
            Linear = GL_LINEAR
        };

        WrapMethod warp_s;
        WrapMethod warp_r;
        WrapMethod warp_t;
        glm::vec4 borderColor;
        MinFilter minFilter;
        MagFilter magFilter;
        float anisotropicLevel;

        inline SamplerInfo(){
            warp_s = warp_r = warp_t = WrapMethod::Repeat;
            borderColor = glm::vec4(0,0,0,0);
        }
    };

    struct AGE_API Sampler{
    private:
        friend class Tetxure;
        friend class Application;
        GLuint sampler_id;
        SamplerInfo * info;

        Sampler(){}
    public:
        std::string_view sid;

        inline void bind(GLuint channel){
            glBindSampler(channel - GL_TEXTURE0,sampler_id);
        }

        inline const SamplerInfo& getInfo(){
            return *info;
        }

        inline Sampler& wrapS(SamplerInfo::WrapMethod method){
            info->warp_s = method;
            glSamplerParameteri(sampler_id,GL_TEXTURE_WRAP_S,static_cast<GLenum>(method));
            return *this;
        }

        inline Sampler& wrapR(SamplerInfo::WrapMethod method){
            info->warp_r = method;
            glSamplerParameteri(sampler_id,GL_TEXTURE_WRAP_R,static_cast<GLenum>(method));
            return *this;
        }

        inline Sampler& wrapT(SamplerInfo::WrapMethod method){
            info->warp_t = method;
            glSamplerParameteri(sampler_id,GL_TEXTURE_WRAP_T,static_cast<GLenum>(method));
            return *this;
        }

        inline Sampler& borderColor(const glm::vec4& color){
            info->borderColor = color;
            glSamplerParameterfv(sampler_id,GL_TEXTURE_BORDER_COLOR,glm::value_ptr(color));
            return *this;
        }

        inline Sampler& minFilter(SamplerInfo::MinFilter met){
            info->minFilter = met;
            glSamplerParameteri(sampler_id,GL_TEXTURE_MIN_FILTER,static_cast<GLenum>(met));
            return *this;
        }

        inline Sampler& magFilter(SamplerInfo::MagFilter met){
            info->magFilter = met;
            glSamplerParameteri(sampler_id,GL_TEXTURE_MAG_FILTER,static_cast<GLenum>(met));
            return *this;
        }

        inline Sampler& try_anisotropy(float value){
            auto [support,val] = Queryer().anisotropicFiltering();
            if(support){
                glSamplerParameterf(sampler_id,GL_TEXTURE_MAX_ANISOTROPY_EXT,std::min(val,value));
            }// do nothing if doesnt support
            return *this;
        }
    };

    class Application;

    /// @todo 让我斟酌一下要不要搞noncopyable,Application那里先默认copy吧
    struct AGE_API Texture : public NonCopyable{
    private:
        GLuint texture_id {0};
        //其实我(Texture)不知道纹理的sid是什么hhh (为了代价更小的拷贝,sid在Application那里集中处理)
        friend class Application;
        //Application * parentApp; 待定
        TextureInfo * textureInfo; ///< 这里我是假设(事实上unordered_map九十的)textureInfo对应的内存地址不会变动

        ///Forbid User-Define
        inline Texture(){}
    public:
        std::string_view sid;

        /// @brief bind the texture to the current vbo
        /// @param channel 
        inline void bind(GLuint channel){
            if(!texture_id)return;
            glActiveTexture(channel);
            glBindTexture(GL_TEXTURE_2D,texture_id);
        }

        //由于Application无法包含否则循环include,所以这里似乎需要抛弃inline
        inline const TextureInfo & getTextureInfo(){
            return *textureInfo;
        }
    };

    ///@todo for fun: 搞个 TextureInfoEx提供更加细节的配置什么什么的

    struct AGE_API CreateTextureInfo{
        enum Source{
            FromFile,
            FromBuffer,
            FromVector
        };

        std::string sid;
        Source source;
        struct { 
            std::string path;
        } file; ///< leave empty if you don't want to load texture from file
        struct {
            GLchar * data;
            size_t eleCount; ///< not the size of the buffer!!!
        } buffer;
        struct {
            std::vector<GLchar>* data;
        } vec;
        unsigned int channel_desired;
        bool uploadToOpenGL;
        bool genMipmap;
        ///SamplerSettings

        CreateTextureInfo();
    };
}

#endif