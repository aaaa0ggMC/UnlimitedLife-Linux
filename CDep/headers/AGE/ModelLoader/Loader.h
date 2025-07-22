/**
 * @file Loader.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 加载模型
 * @version 0.1
 * @date 2025/07/22
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
    template<class T> concept AFormat = requires(std::string_view data,ModelData & model,bool flipV){
       {T::parse(data,model,flipV)};
    };
    
    namespace fmt{
        struct AGE_API Obj{
            static void parse(std::string_view data,ModelData & model,bool flipV = false);
        };

        struct AGE_API Stl{
            static void parse(std::string_view data,ModelData & model,bool flipV = false);
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
    template<AFormat Format> inline void loadModelFromMemory(std::string_view data,ModelData & model,bool flipV = false){
        Format::parse(data,model,flipV);
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
        alib::g3::Util::io_readAll(fp,data);
        loadModelFromMemory<Format>(data,model,flipV);
    }
}

#endif