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

export namespace ave{
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

        static bool recreate_swapchain_from_window(
            Renderer & r,
            Window & window,
            RenderBuildReport * report = nullptr,
            ProfileWith with = {},
            alib6::ErrorWrapper ew = {}
        );

        inline static bool recreate_swapchain_from_window(
            Renderer & r,
            Window & window,
            ProfileWith with,
            alib6::ErrorWrapper ew = {}
        ){
            return recreate_swapchain_from_window(r, window, nullptr, std::move(with), ew);
        }

        inline static bool recreate_swapchain_from_window(
            Renderer & r,
            Window & window,
            RenderBuildReport * report,
            alib6::ErrorWrapper ew
        ){
            return recreate_swapchain_from_window(r, window, report, ProfileWith{}, ew);
        }
    
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
        bool __vk_swapchain(Renderer & r, alib6::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_images(Renderer & r, alib6::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_sync_objects(Renderer & r, alib6::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_legacy_render(Renderer & r, alib6::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_dynamic_render(Renderer & r, alib6::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_command_pool(Renderer & r, alib6::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_command_buffers(Renderer & r, alib6::ErrorWrapper ew, RenderBuildReport * result);
    };
    
    inline bool recreate_swapchain_from_window(
        Renderer & r,
        Window & window,
        RenderBuildReport * report = nullptr,
        ProfileWith with = {},
        alib6::ErrorWrapper ew = {}
    ){
        return RenderProfile::recreate_swapchain_from_window(r, window, report, std::move(with), ew);
    }

    inline bool recreate_swapchain_from_window(
        Renderer & r,
        Window & window,
        ProfileWith with,
        alib6::ErrorWrapper ew = {}
    ){
        return RenderProfile::recreate_swapchain_from_window(r, window, std::move(with), ew);
    }

    inline bool recreate_swapchain_from_window(
        Renderer & r,
        Window & window,
        RenderBuildReport * report,
        alib6::ErrorWrapper ew
    ){
        return RenderProfile::recreate_swapchain_from_window(r, window, report, ProfileWith{}, ew);
    }

}
