/**
 * @file Texture.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 纹理
 * @version 0.1
 * @date 2025/12/01
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/06/20 （左右） 
 */
#ifndef AGE_H_TEXTURE
#define AGE_H_TEXTURE
#include <AGE/Utils.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <AGE/Query.h>

namespace age::manager{
    class TextureManager;
    class SamplerManager;
}

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
        bool hasbits;
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
        friend class age::manager::SamplerManager;
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
            glSamplerParameterfv(sampler_id,GL_TEXTURE_BORDER_COLOR,glm::value_ptr(info->borderColor));
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

    /// @todo 让我斟酌一下要不要搞noncopyable,Application那里先默认copy吧
    struct AGE_API Texture : public NonCopyable{
    private:
        friend class age::manager::TextureManager;

        GLuint texture_id {0};
        TextureInfo * textureInfo; ///< 这里我是假设(事实上unordered_map九十的)textureInfo对应的内存地址不会变动
        /// 绑定点信息
        GLuint binding_point;

        ///Forbid User-Define
        inline Texture(){}
    public:
        /// 其实我(Texture)不知道纹理的sid是什么hhh (为了代价更小的拷贝,sid在Application那里集中处理)
        std::string_view sid;

        /// @brief bind the texture to the current vbo
        /// @param channel 
        inline void bind(GLuint channel){
            if(!texture_id)return;
            glActiveTexture(channel);
            glBindTexture(binding_point,texture_id);
        }

        inline GLuint getId(){
            return texture_id;
        }

        struct AGE_API TexImageParams{
            // GLuint m_target;
            GLint m_level;
            GLint m_internalformat;
            GLsizei m_width;
            GLsizei m_height; //< works for 2D,3D
            GLsizei m_depth; ///< works only for 3D
            GLint m_border;
            GLenum m_format;
            GLenum m_type;
            const void* m_data;

            inline TexImageParams& border(GLint a = 0){
                m_border = a;
                return *this;
            }

            inline TexImageParams& type(GLenum a){
                m_type = a;
                return *this;
            }

            inline TexImageParams& format(GLenum a){
                m_format = a;
                return *this;
            }

            /// Specifies a pointer to the image data in memory.
            inline TexImageParams& data(const void * a){
                m_data = a;
                return *this;
            }

            /*inline TexImageParams& target(GLuint a){
                m_target = a;
                return *this;
            }*/

            inline TexImageParams& level(GLint a){
                m_level = a;
                return *this;
            }

            inline TexImageParams& internalformat(GLint a){
                m_internalformat = a;
                return *this;
            }

            inline TexImageParams& width(GLsizei a){
                m_width = a;
                return *this;
            }

            inline TexImageParams& height(GLsizei a){
                m_height = a;
                return *this;
            }

            inline TexImageParams& depth(GLsizei a){
                m_depth = a;
                return *this;
            }

            /// @brief default values: width=height=depth=0 internalformat=format=GL_RGBA type=GL_UNSIGNED_INT border=level=data=0
            inline TexImageParams(){
                width(0).height(0).depth(0);
                internalformat(GL_RGBA).type(GL_UNSIGNED_BYTE).border(0);
                level(0).data(0).format(GL_RGBA);
            }
        };

        inline void texImage2D(const TexImageParams & data){
            ScopedGLState<GL_TEXTURE_2D> scope(texture_id);
            binding_point = GL_TEXTURE_2D;
            glTexImage2D(GL_TEXTURE_2D,data.m_level,data.m_internalformat,data.m_width,data.m_height,data.m_border,data.m_format,data.m_type,data.m_data);
        }

        inline void texImage1D(const TexImageParams & data){
            ScopedGLState<GL_TEXTURE_1D> scope(texture_id);
            binding_point = GL_TEXTURE_1D;
            glTexImage1D(GL_TEXTURE_1D,data.m_level,data.m_internalformat,data.m_width,data.m_border,data.m_format,data.m_type,data.m_data);
        }

        inline void texImage3D(const TexImageParams & data){
            ScopedGLState<GL_TEXTURE_3D> scope(texture_id);
            binding_point = GL_TEXTURE_3D;
            glTexImage3D(GL_TEXTURE_3D,data.m_level,data.m_internalformat,data.m_width,data.m_height,data.m_depth,data.m_border,data.m_format,data.m_type,data.m_data);
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
            FromVector,
            CreateEmpty
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
        bool uploadToOpenGL; ///< if this value equals to true,make sure that you are creating an image rather than something eg. shadow map
        bool genMipmap;
        ///SamplerSettings

        CreateTextureInfo();
    };
}

#endif