/**
 * @file instance.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Instance的一些配置选项
 * @version 5.0
 * @date 2026-09-10
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <AVE/config.h>

export module ave.profile:profile;

import ave.window;
import ave.render;
import std;

namespace ave::detail{
    std::vector<std::string> get_window_required_extensions(Window * window){
        if(window){
            return
                GLFWManager::get_required_instance_extensions() |
                std::views::transform([](const char * p){
                    return std::string(p);
                }) |
                std::ranges::to<std::vector>();
        }

        return {};
    }
};

export namespace ave{
    
    struct AVE_API WithGlobalInput {
    private:
        friend class RenderProfile;
        Window * window;

        WithGlobalInput(Window * w):window(w){}
    public:
        std::vector<std::string> get_required_extensions(){
            if(window){
                return
                    GLFWManager::get_required_instance_extensions() |
                    std::views::transform([](const char * p){
                        return std::string(p);
                    }) |
                    std::ranges::to<std::vector>();
            }
            return {};
        }
    };

    struct AVE_API ProfileWith {
        std::shared_ptr<Instance> instance { nullptr };
        std::function<void(WithGlobalInput &, CreateInstanceInfo & ci)> configure_instance { nullptr };
    };
};