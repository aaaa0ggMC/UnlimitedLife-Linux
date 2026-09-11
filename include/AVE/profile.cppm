/**
 * @file profile.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 轻松连接 render和window
 * @version 5.0
 * @date 2026-09-10
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <AVE/config.h>

export module ave.profile;
export import :profile;
export import :report;

import ave.window;
import ave.render;
import ave.context;
import alib6;

import :profile;
import :report;

namespace ave{
    struct AVE_API RenderProfile {
        enum Type {
            SurfaceRenderer
        };
    private:
        Context & ctx;
        Type type;
        Window * window;
        ProfileWith with_data;
        RenderBuildReport * build_report { nullptr };
    public:
        RenderProfile(Context& ctx,Window & win)
        :ctx(ctx)
        ,type(SurfaceRenderer){
            window = &win;
        }

        inline Type get_type() { return type; }
        inline static RenderProfile from_window(Context & ctx, Window & window){ return RenderProfile(ctx, window); }
    
        [[nodiscard]] Renderer build(alib6::ErrorWrapper ew = {});

        
        inline RenderProfile & with(ProfileWith wd){
            with_data = std::move(wd);
            return *this;
        }

        inline RenderProfile & with_result(RenderBuildReport & result){
            build_report = std::addressof(result);
            return *this;
        }

    private: // build fns
        bool __vk_instance(Renderer & r, alib6::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_create_glfw_surface(Renderer & r, alib6::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_select_physical_device(Renderer & r, alib6::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_device(Renderer & r, alib6::ErrorWrapper ew, RenderBuildReport * result);
    };

}
