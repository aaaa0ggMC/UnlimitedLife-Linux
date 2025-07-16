#ifndef AGE_H_TEXTURE
#define AGE_H_TEXTURE
#include <AGE/Utils.h>
#include <GL/glew.h>

namespace age{
    struct AGE_API TextureInfo{
        std::string filePath; ///< empty if loaded from memory
        bool loadedFromMemory;

        //图片属性
        int width;
        int height;
        int channels;
    };

    class Application;

    /// @todo 让我斟酌一下要不要搞noncopyable,Application那里先默认copy吧
    struct AGE_API Texture{
    private:
        GLuint texture_id {0};
        //其实我(Texture)不知道纹理的sid是什么hhh (为了代价更小的拷贝,sid在Application那里集中处理)
        friend class Application;
        //Application * parentApp; 待定
        TextureInfo * textureInfo; ///< 这里我是假设(事实上unordered_map九十的)textureInfo对应的内存地址不会变动

        ///Forbid User-Define
        inline Texture(Application * app){
            parentApp = app;
        }
    public:
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
        enum class Source{
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
            std::vector<GLuchar>* data;
        } vec;

        unsigned int channel_desired;

        CreateShaderInfo();
    };
}

#endif