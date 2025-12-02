#include <AGE/Details/TextureManager.h>
#include <stb/stb_image.h>

using namespace age;
using namespace age::manager;

TextureManager::~TextureManager(){
    for(auto& [_,tex] : textures){
        if(tex->texture_id){
            glDeleteTextures(1,&(tex->texture_id));
        }
    }
    for(auto& [_,info] : texturesInfo){
        if(!info.uploaded && info.bits){
            stbi_image_free(info.bits);
        }
    }
}

std::optional<Texture*> TextureManager::create(const CreateTextureInfo & info){
    panic_debug(info.sid.empty(),"Cannot pass empty sid to create function!");
    auto tex_it = textures.find(info.sid);
    if(tex_it != textures.end()){
        panicf_debug(true,"Conflict texture sid[{}]!",info.sid);
        Error::def.pushMessage({AGEE_CONFLICT_SID,"Texture SID conflicts!!!"});
        return { &(*(tex_it->second)) };
    }

    TextureInfo tinfo;
    const GLchar * buf = info.buffer.data;
    size_t eleC = info.buffer.eleCount;
    stbi_uc* bits;

    tinfo.hasbits = true;
    switch(info.source){
    case CreateTextureInfo::FromVector:
        buf = info.vec.data->data();
        eleC = info.vec.data->size();
    case CreateTextureInfo::FromBuffer:
        if(!buf){
            Error::def.pushMessage({AGEE_EMPTY_DATA,"Buffer passed to createTexture is empty!"});
            return std::nullopt;
        }
        bits = stbi_load_from_memory((const stbi_uc*)buf,eleC,&tinfo.width,&tinfo.height,&tinfo.bits_channels,0);
        break;
    case CreateTextureInfo::FromFile:
        if(!info.file.path.compare("")){
            Error::def.pushMessage({AGEE_EMPTY_DATA,"The file path provided is empty!"});
            return std::nullopt;
        }
        bits = stbi_load(info.file.path.c_str(),&tinfo.width,&tinfo.height,&tinfo.bits_channels,0);
        break;
    case CreateTextureInfo::CreateEmpty:
        bits = nullptr;
        tinfo.hasbits = false;
        tinfo.width = info.empty.width;
        tinfo.height = info.empty.height;
        break;
    default:{
            std::string em = "The source enum passed to createTexture is invalid: ";
            em += std::to_string((int)info.source);
            Error::def.pushMessage({AGEE_WRONG_ENUM,em.c_str()});
            break;
        }
    }

    tinfo.internal_format = info.internalFormat;
    tinfo.bits = bits;

    auto sid = csbuffer.get(info.sid);
    auto tex = textures.emplace(sid,new Texture());
    auto& tex_info = texturesInfo.emplace(sid,tinfo).first->second;
    Texture* texture = tex.first->second;
    texture->sid = sid;
    texture->textureInfo = &tex_info;
    tex_info.uploaded = false;
    tex_info.mipmap = info.genMipmap;

    if(info.uploadToOpenGL){
        //上传到OpenGL
        uploadImageToGL(*texture);
    }

    return texture;
}

std::optional<Texture*> TextureManager::get(std::string_view sid){
    panic_debug(sid.empty(),"Cannot pass empty sid to function!");
    auto tex = textures.find(sid);
    if(tex == textures.end()){
        Error::def.pushMessage({AGEE_CANT_FIND_SID,"Cannot find required texture!"});
        return std::nullopt;
    }
    return tex->second;
}

bool TextureManager::destroy(std::string_view sid){
    panic_debug(sid.empty(),"Cannot pass empty sid to function!");
    auto tex = textures.find(sid);
    if(tex == textures.end()){
        Error::def.pushMessage({AGEE_CANT_FIND_SID,"Cannot find required texture to delete!"});
        return false;
    }
    glDeleteTextures(1,&(tex->second->texture_id));
    textures.erase(tex);
    auto tinfo =  texturesInfo.find(sid);
    if(tinfo != texturesInfo.end())texturesInfo.erase(tinfo);
    return true;
}

/// Will set GL_TEXTURE_2D to 0 after call
Texture& TextureManager::uploadImageToGL(Texture & texture){
    if(texture.textureInfo->uploaded){
        Error::def.pushMessage({AGEE_TEXTURE_LOADED,"The texture has already uploaded to OpenGL."});
        return texture;
    }
    glGenTextures(1,&texture.texture_id);
    if(!texture.texture_id){
        Error::def.pushMessage({AGEE_OPENGL_CREATE_ERROR,"Cant create a new texture!"});
        return texture;
    }
    if(!texture.textureInfo->hasbits){
        ScopedGLState<GL_TEXTURE_2D> scope(texture.texture_id);

        // 其余的值为空就行
        texture.texImage2D(
            Texture::TexImageParams().
            internalformat(texture.textureInfo->internal_format).
            width(texture.textureInfo->width).
            height(texture.textureInfo->height)
        );

        return texture;
    }
    //  由于hasbits的加入，这里要往后，逻辑才通顺
    if(!(texture.textureInfo->bits)){
        Error::def.pushMessage({AGEE_EMPTY_DATA,"The texture data is empty!"});
        return texture;
    }

    texture.textureInfo->uploaded = true;
    ScopedGLState<GL_TEXTURE_2D> scope(texture.texture_id);
    int bits_fmt = GL_RGB;
    switch(texture.textureInfo->bits_channels){
    case 1:
        bits_fmt = GL_RED;
        break;
    case 2:
        bits_fmt = GL_RG;
        break;
    case 4:
        bits_fmt = GL_RGBA;
        break;
    case 3:
        break;
    default:
        Error::def.pushMessage({AGEE_FEATURE_NOT_SUPPORTED,"File with channel count bigger than 4 or lower than 1 is unsupported!"});
        glBindTexture(GL_TEXTURE_2D,0);
        return texture;
    }
    texture.texImage2D(
        Texture::TexImageParams().
        internalformat(texture.textureInfo->internal_format).format(bits_fmt).
        width(texture.textureInfo->width).height(texture.textureInfo->height).
        data(texture.textureInfo->bits)
    );
    if(texture.textureInfo->mipmap){
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    stbi_image_free(texture.textureInfo->bits);
    texture.textureInfo->bits = nullptr;

    return texture;
}