#pragma once

#include <AVE/alib.h>

/**
 * @file context.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 内存方面的设定
 * @version 5.0
 * @date 2026-09-10
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include <AVE/config.h>
#include <vulkan/vulkan.h>



namespace ave{

    struct AVE_API Context{
    private:
        VkAllocationCallbacks * allocator { nullptr };
        std::pmr::memory_resource * mem_res;
        alib5::str::StringPool<std::pmr::string> pool;
    public:    
        inline Context(VkAllocationCallbacks * allocator = nullptr,std::pmr::memory_resource * res = std::pmr::get_default_resource())
        :allocator(allocator)
        ,pool(res){
            this->mem_res = res;
        }

        /// 长期驻留字符串
        inline std::string_view intern(std::string_view ctx){
            return pool.get(ctx);
        }

        /// 获取Vulkan内存分配器
        inline VkAllocationCallbacks* get_vk_allocator(){
            return allocator;
        }
    };

}