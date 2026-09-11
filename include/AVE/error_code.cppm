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
        ave_vk_create_debug_messenger = 100'005
    };
};
