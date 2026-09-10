/**
 * @file config.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 一些配置
 * @version 5.0
 * @date 2026-09-10
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#pragma once

/// 兼容Windows DLL构建
#ifdef _WIN32

#ifndef AVE_API
#ifdef AVE_BUILD_DLL
    #define AVE_API __declspec(dllexport)
#else
    #define AVE_API __declspec(dllimport)
#endif
#endif

#else 

#define AVE_API

#endif