/**
 * @file Loader.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 加载模型
 * @version 0.1
 * @date 2025/07/20
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
    template<class T> concept AFormat = requires(std::string_view data,ModelData & model){
       {T::parse(data,model)};
    };
    
    namespace fmt{
        struct AGE_API Obj{
            static void parse(std::string_view data,ModelData & model);
        };

        struct AGE_API Stl{
            static zvoid parse(std::string_view data,ModelData & model);
        };
    };

    template<AFormat Format> inline void loadModelFromFile(std::string_view filePath,ModelData & model){
        std::string fp = "";
        fp += filePath;
        std::string data = "";
        alib::g3::Util::io_readAll(fp,data);
        loadModelFromMemory<Format>(data,model);
    }

    template<AFormat Format> inline void loadModelFromMemory(std::string_view data,ModelData & model){
        Format::parse(data,model);
    }
}

#endif