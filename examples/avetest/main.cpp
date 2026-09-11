#include <vulkan/vulkan.h>

import alib6;
import std;
import ave;

auto main() -> int {
    try{
        alib6::log::Logger logger;
        alib6::log::LogFactory lg(logger,"avetest");
        alib6::log::LogFactory vklg(logger,"Vulkan");
        logger.append_mod<alib6::log::Console>("console");

        ave::Context context;
        ave::Window window({
            .ctx = context,
            .title = "Hello from AVE!",
            .width = 1920,
            .height = 1080,
        });

        ave::ProfileWith with;
        with.configure_debug_messenger.emplace();
        with.configure_debug_messenger->on_message = [&vklg](
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT type,
            const VkDebugUtilsMessengerCallbackDataEXT& data
        ){
            vklg(ave::to_log_level(severity))
                << "[" << ave::to_message_type_name(type) << "]"
                << "[id="
                << (data.pMessageIdName ? data.pMessageIdName : "<no id name>")
                << ":" << data.messageIdNumber << "]"
                << "[objects=" << data.objectCount << "]"
                << "[queue_labels=" << data.queueLabelCount << "]"
                << "[cmd_labels=" << data.cmdBufLabelCount << "] \n"
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

        while(!window.should_close()){
            window.poll_events();

            alib6::Timer(1).wait();
        }
        
    }catch(...){
        // 已经有panic了，也是直接忽略
        return 1;
    }
    return 0;
}
