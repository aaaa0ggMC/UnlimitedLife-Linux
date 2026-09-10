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
 module;
#include <AVE/config.h>
export module ave.context;

import std;
import alib6;

export namespace ave{

    struct AVE_API Context{
    private:
        alib6::memory_resource * mem_res;
        alib6::str::StringPool<> pool;

    public:    
        Context(alib6::memory_resource * res = alib6::get_default_resource())
        :pool(res){
            this->mem_res = res;
        }

        /// 长期驻留字符串
        std::string_view intern(std::string_view ctx){
            return pool.get(ctx);
        }
    };

}