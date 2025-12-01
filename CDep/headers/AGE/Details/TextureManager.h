#ifndef AGE_MTEXTURE_H_INCLUDED
#define AGE_MTEXTURE_H_INCLUDED
#include <AGE/Texture.h>
#include <AGE/Details/CSBuffer.h>

namespace age::manager{
    struct AGE_API TextureManager{
    private:
        friend class Application;
        /// 纹理表
        std::unordered_map<std::string_view,Texture*> textures;
        std::unordered_map<std::string_view,TextureInfo> texturesInfo;

        detail::ConstantStringBuffer& csbuffer;
    public:
        TextureManager(detail::ConstantStringBuffer & cb):csbuffer{cb}{}
        ~TextureManager();

        std::optional<Texture*> create(const CreateTextureInfo & info);
        std::optional<Texture*> get(std::string_view sid);
        bool destroy(std::string_view sid);
        /// Will set GL_TEXTURE_2D to 0 after call
        Texture& uploadImageToGL(Texture & texure);
        
        inline bool destroy(Texture& tex){
            return destroy(tex.sid);
        }
    };   
}


#endif