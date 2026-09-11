/**
 * @file error_code.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 错误码
 * @version 5.0
 * @date 2026-09-10
 * 
 * @copyright Copyright (c) 2026
 * 
 */
 module;
#include <AVE/config.h>
export module ave.ecode;

import std;
import alib6;

export namespace ave{
    enum ErrorCode : alib6::i64 {
        ave_success            = 0,
        ave_not_in_main_thread = 100'001,
        ave_bad_glfw           = 100'002,
        ave_already_created    = 100'003,
        ave_vk_create_instance = 100'004,
        ave_vk_create_debug_messenger = 100'005,
        ave_vk_create_surface = 100'006,
        ave_vk_select_physical_device = 100'007,
        ave_vk_create_device = 100'008,
        ave_vk_create_swapchain = 100'009,
        ave_vk_create_sync_objects = 100'010,
        ave_vk_create_render_pass = 100'011,
        ave_vk_create_framebuffer = 100'012,
        ave_vk_create_pipeline_layout = 100'013,
        ave_vk_create_graphics_pipeline = 100'014,
        ave_vk_create_shader_module = 100'015,
        ave_vk_create_command_pool = 100'016,
        ave_vk_allocate_command_buffers = 100'017,
        ave_vk_draw_frame = 100'018
    };
};
