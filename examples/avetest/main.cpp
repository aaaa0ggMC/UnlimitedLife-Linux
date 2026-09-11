#include <vulkan/vulkan.h>

import alib6;
import std;
import ave;

auto main() -> int {
    try{
        alib6::Logger logger;
        alib6::LogFactory lg(logger,"avetest");
        alib6::LogFactory vklg(logger,"Vulkan");
        logger.append_mod<alib6::lot::Console>("console");

        ave::Context context;
        ave::Window window({
            .ctx = context,
            .title = "Hello from AVE!",
            .width = 800,
            .height = 600,
        });

        ave::ProfileWith with;
        with.configure_debug_messenger.emplace();
        with.configure_debug_messenger->on_message = [&vklg](
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT type,
            const VkDebugUtilsMessengerCallbackDataEXT& data
        ){
            vklg(ave::to_log_level(severity))
                << (data.pMessage ? data.pMessage : "<no message>")
                << std::endl;
            return false;
        };

        ave::RenderBuildReport build_result;
        ave::Renderer renderer =
            ave::RenderProfile::from_window(context, window)
            .with(std::move(with))
            .with_result(build_result)
            .build()
        ;
        lg << alib6::to_adata(build_result) << std::endl;

        auto pipeline = renderer.legacy_render->create_graphics_pipeline(
            "avetest/shaders/simple-vert.spv",
            "avetest/shaders/simple-frag.spv"
        );
        if(!pipeline) return 1;
        int stage = 0;

        alib6::u64 frames = 0;
        alib6::Clock clock;
        while(!window.should_close()){
            window.poll_events();
            auto graphics = renderer.acquire_context();

            graphics.begin();
            graphics.bind_pipeline(*pipeline);
            graphics.draw(3);
            graphics.end();
        
            ++frames;
        }

        lg << frames / clock.get_all() * 1000 << std::endl;
    }catch(...){
        // 已经有panic了，也是直接忽略
        return 1;
    }
    return 0;
}
