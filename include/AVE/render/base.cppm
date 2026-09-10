/**
 * @file instance.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Vulkan基础内容
 * @version 5.0
 * @date 2026-09-10
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.render:base;
import alib6;

export namespace ave{

    struct AVE_API Version{
        alib6::u16 major;
        alib6::u16 minor;
        alib6::u16 patch;

        inline constexpr Version(alib6::u16 major = 0, alib6::u16 minor = 1, alib6::u16 patch = 0)
        :major(major),minor(minor),patch(patch){}
        inline constexpr Version(alib6::u32 composed){ from(composed); }

        inline constexpr alib6::u32 to() const { return VK_MAKE_VERSION(major, minor, patch); }
        inline constexpr void from(alib6::u32 composed){
            major = static_cast<alib6::u16>(VK_VERSION_MAJOR(composed));
            minor = static_cast<alib6::u16>(VK_VERSION_MINOR(composed));
            patch = static_cast<alib6::u16>(VK_VERSION_PATCH(composed));
        }
    };

    struct AVE_API ApiVersion {
        alib6::u8 variant;
        alib6::u8 major;
        alib6::u16 minor;
        alib6::u16 patch;

        inline constexpr ApiVersion(
            alib6::u8 variant = 0,
            alib6::u8 major = 1,
            alib6::u16 minor = 0,
            alib6::u16 patch = 0
        )
        :variant(variant)
        ,major(major)
        ,minor(minor)
        ,patch(patch) {}

        inline constexpr ApiVersion(alib6::u32 composed) { from(composed); }
        inline constexpr alib6::u32 to() const {
            return VK_MAKE_API_VERSION(
                variant,
                major,
                minor,
                patch
            );
        }

        inline constexpr void from(alib6::u32 composed) {
            variant = static_cast<alib6::u8>(
                VK_API_VERSION_VARIANT(composed)
            );

            major = static_cast<alib6::u8>(
                VK_API_VERSION_MAJOR(composed)
            );

            minor = static_cast<alib6::u16>(
                VK_API_VERSION_MINOR(composed)
            );

            patch = static_cast<alib6::u16>(
                VK_API_VERSION_PATCH(composed)
            );
        }
    };

    inline constexpr ApiVersion ave_vk_1_0 = VK_API_VERSION_1_0;
    inline constexpr ApiVersion ave_vk_1_1 = VK_API_VERSION_1_1;
    inline constexpr ApiVersion ave_vk_1_2 = VK_API_VERSION_1_2;
    inline constexpr ApiVersion ave_vk_1_3 = VK_API_VERSION_1_3;
    inline constexpr ApiVersion ave_vk_1_4 = VK_API_VERSION_1_4;
}