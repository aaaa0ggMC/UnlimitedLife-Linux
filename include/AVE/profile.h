#pragma once

#include <AVE/profile/profile.h>
#include <AVE/profile/report.h>
#include <AVE/window.h>
#include <AVE/render.h>
#include <AVE/context.h>
#include <AVE/alib.h>

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
#include <AVE/config.h>




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

        static bool recreate_swapchain_from_window(
            Context & ctx,
            Renderer & r,
            Window & window,
            RenderBuildReport * report = nullptr,
            ProfileWith with = {},
            ave::ErrorWrapper ew = {}
        );

        inline static bool recreate_swapchain_from_window(
            Context & ctx,
            Renderer & r,
            Window & window,
            ProfileWith with,
            ave::ErrorWrapper ew = {}
        ){
            return recreate_swapchain_from_window(ctx, r, window, nullptr, std::move(with), ew);
        }

        inline static bool recreate_swapchain_from_window(
            Context & ctx,
            Renderer & r,
            Window & window,
            RenderBuildReport * report,
            ave::ErrorWrapper ew
        ){
            return recreate_swapchain_from_window(ctx, r, window, report, ProfileWith{}, ew);
        }

        static bool recreate_swapchain_from_window(
            Renderer & r,
            Window & window,
            RenderBuildReport * report = nullptr,
            ProfileWith with = {},
            ave::ErrorWrapper ew = {}
        );

        inline static bool recreate_swapchain_from_window(
            Renderer & r,
            Window & window,
            ProfileWith with,
            ave::ErrorWrapper ew = {}
        ){
            return recreate_swapchain_from_window(r, window, nullptr, std::move(with), ew);
        }

        inline static bool recreate_swapchain_from_window(
            Renderer & r,
            Window & window,
            RenderBuildReport * report,
            ave::ErrorWrapper ew
        ){
            return recreate_swapchain_from_window(r, window, report, ProfileWith{}, ew);
        }
    
        [[nodiscard]] Renderer build(ave::ErrorWrapper ew = {});

        
        inline RenderProfile & with(ProfileWith wd){
            with_data = std::move(wd);
            return *this;
        }

        inline RenderProfile & with_result(RenderBuildReport & result){
            build_report = std::addressof(result);
            return *this;
        }

    private: // build fns
        bool __vk_instance(Renderer & r, ave::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_create_glfw_surface(Renderer & r, ave::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_select_physical_device(Renderer & r, ave::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_device(Renderer & r, ave::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_swapchain(Renderer & r, ave::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_images(Renderer & r, ave::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_sync_objects(Renderer & r, ave::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_legacy_render(Renderer & r, ave::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_dynamic_render(Renderer & r, ave::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_command_pool(Renderer & r, ave::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_command_buffers(Renderer & r, ave::ErrorWrapper ew, RenderBuildReport * result);
        bool __vk_descriptor_pool(Renderer & r, ave::ErrorWrapper ew, RenderBuildReport * result);
    };
    
    inline bool recreate_swapchain_from_window(
        Context & ctx,
        Renderer & r,
        Window & window,
        RenderBuildReport * report = nullptr,
        ProfileWith with = {},
        ave::ErrorWrapper ew = {}
    ){
        return RenderProfile::recreate_swapchain_from_window(ctx, r, window, report, std::move(with), ew);
    }

    inline bool recreate_swapchain_from_window(
        Context & ctx,
        Renderer & r,
        Window & window,
        ProfileWith with,
        ave::ErrorWrapper ew = {}
    ){
        return RenderProfile::recreate_swapchain_from_window(ctx, r, window, std::move(with), ew);
    }

    inline bool recreate_swapchain_from_window(
        Context & ctx,
        Renderer & r,
        Window & window,
        RenderBuildReport * report,
        ave::ErrorWrapper ew
    ){
        return RenderProfile::recreate_swapchain_from_window(ctx, r, window, report, ProfileWith{}, ew);
    }

    inline bool recreate_swapchain_from_window(
        Renderer & r,
        Window & window,
        RenderBuildReport * report = nullptr,
        ProfileWith with = {},
        ave::ErrorWrapper ew = {}
    ){
        return RenderProfile::recreate_swapchain_from_window(r, window, report, std::move(with), ew);
    }

    inline bool recreate_swapchain_from_window(
        Renderer & r,
        Window & window,
        ProfileWith with,
        ave::ErrorWrapper ew = {}
    ){
        return RenderProfile::recreate_swapchain_from_window(r, window, std::move(with), ew);
    }

    inline bool recreate_swapchain_from_window(
        Renderer & r,
        Window & window,
        RenderBuildReport * report,
        ave::ErrorWrapper ew
    ){
        return RenderProfile::recreate_swapchain_from_window(r, window, report, ProfileWith{}, ew);
    }

}
