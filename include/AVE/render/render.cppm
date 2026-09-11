/**
 * @file render.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 提供profile来创建一个完整的Render
 * @version 5.0
 * @date 2026-09-10
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.render:render;
import ave.context;
import alib6;

import :base;
import :instance;
import :debug_messenger;
import :surface;
import :physical_device;
import :device;

export namespace ave{
    struct AVE_API Renderer {
        std::shared_ptr<Instance> instance;
        std::shared_ptr<DebugMessenger> debug_messenger;
        std::shared_ptr<Surface> surface;
        std::optional<PhysicalDeviceInfo> physical_device;
        std::shared_ptr<Device> device;
    };
};
