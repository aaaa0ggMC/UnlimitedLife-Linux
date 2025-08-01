/**
 * @file Loader.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 加载模型
 * @version 0.1
 * @date 2025/07/25
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/07/20 
 */
#ifndef AGE_H_LOADER
#define AGE_H_LOADER
#include <AGE/Utils.h>
#include <AGE/Model.h>
#include <alib-g3/autil.h>

namespace age::model{
    template<class T> concept AFormat = requires(std::string_view data,ModelData & model,bool flipV,std::string_view path){
       {T::parse(data,model,flipV,path)};
    };
    
    // supports some common & easy formats for ModelData
    // if you want to load model for Model, use other functions(WIP) which uses assimp
    namespace fmt{
        struct AGE_API Obj{
            static void parse(std::string_view data,ModelData & model,bool flipV = false,std::string_view = "");
        };

        struct AGE_API Stl{
            static void parse(std::string_view data,ModelData & model,bool flipV = false,std::string_view = "");
        };

        struct AGE_API StlAscii{
            static void parse(std::string_view data,ModelData & model,bool flipV = false,std::string_view = "");
        };

        struct AGE_API StlBinary{
            static void parse(std::string_view data,ModelData & model,bool flipV = false,std::string_view = "");
        };

        struct AGE_API AutoDetect{
            static void parse(std::string_view data,ModelData & model,bool flipV = false,std::string_view filePath = "");
        };

        struct AGE_API Gltf{
            [[deprecated("GLTF is too complex, it will only be implemented for Model instead of ModelData")]]
            static void parse(std::string_view data,ModelData & model,bool flipV = false,std::string_view filePath = "");

            // loadScene = -1 means load the scene described in the gltf file to model.currentScene
            // static void parse(std::string_view data,Model & model,int loadScene = -1,bool flipV = false,std::string_view filePath = "");
        };
    };

    /// @brief 从内存中加载模型
    /// @tparam Format 
    /// @param data 内存数据
    /// @param model 模型输出
    /// @param flipV 是否反转v分量（默认false）
    /// @note 反转v分量是为了兼容OpenGL的纹理，一般来讲blender导出的不要翻转，其余的可能要翻转
    /// @note 目前只有 obj 会对翻转处理
    /// @note 类型以模板形式支持
    /// @see age::models::fmt
    template<AFormat Format> inline void loadModelFromMemory(std::string_view data,ModelData & model,bool flipV = false,std::string_view filePath = ""){
        Format::parse(data,model,flipV,filePath);
    }
    /// @return scene id, @todo 这个可以支持GLTF!!!
    template<AFormat Format> inline int loadModelFromMemory(std::string_view sdata,Model & model,
        int index,GLuint vloc = 0,GLuint vtloc = 1,GLuint vnloc = 2,bool flipV = false,std::string_view filePath = ""){
        if(sdata.empty())return -1;
        auto v = model.get(index);
        auto data = (!v)?model.add():(*v);
        ModelData& md = *data.first;
        Model::GraphRes& gr = *data.second;
        loadModelFromMemory<Format>(sdata,md,flipV,filePath);
        md.bind(gr.vao,gr.vbuffer,gr.ibuffer,gr.vtbuffer,gr.vnbuffer,vloc,vtloc,vnloc);
        //if(loadTheScene)model.loadScene(model.getSceneCount() - 1);
        return model.getSceneCount() - 1;
    }

    /// @brief 从文件加载模型
    /// @tparam Format
    /// @param filePath 文件路径
    /// @param model 模型输出
    /// @param flipV 是否反转v分量（默认false）
    /// @note 反转v分量是为了兼容OpenGL的纹理，一般来讲blender导出的不要翻转，其余的可能要翻转
    /// @note 目前只有 obj 会对翻转处理
    template<AFormat Format> inline void loadModelFromFile(std::string_view filePath,ModelData & model,bool flipV = false){
        std::string fp = "";
        fp += filePath;
        std::string data = "";
        int fsize = alib::g3::Util::io_readAll(fp,data);
        if(fsize == -1){
            Error::def.pushMessage({
                -1,
                "<-[alib error code]Cannot open file or fetch valid data!",
                ErrorLevel::Error
            });
            return;
        }
        loadModelFromMemory<Format>(data,model,flipV,filePath);
    }
    /// @return scene id @warning binding point doesnt work for GLTF
    template<AFormat Format> inline int loadModelFromFile(std::string_view filePath,Model & model,
        int index = 0,GLuint vloc = 0,GLuint vtloc = 1,GLuint vnloc = 2,bool flipV = false){
        std::string fp = "";
        fp += filePath;
        std::string data = "";
        int fsize = alib::g3::Util::io_readAll(fp,data);
        if(fsize == -1){
            Error::def.pushMessage({
                -1,
                "<-[alib error code]Cannot open file or fetch valid data!",
                ErrorLevel::Error
            });
            return -1;
        }
        return loadModelFromMemory<Format>(data,model,index,vloc,vtloc,vnloc,flipV,filePath);
    }
}

#endif