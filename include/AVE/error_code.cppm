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

#define BASE 100'000 
#define NEW_ECODE(X) constexpr alib6::i64 ave_##X = BASE + __LINE__ 

export namespace ave{
    constexpr alib6::i64 ave_success = 0;

    NEW_ECODE(not_in_main_thread);
    NEW_ECODE(bad_glfw);
    NEW_ECODE(window_already_created);
};

#undef BASE
#undef NEW_ECODE