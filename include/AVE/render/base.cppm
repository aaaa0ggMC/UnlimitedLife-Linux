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
#include <cstring>

export module ave.render.base;
import alib6;
import std;

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

    /// 扩展属性
    struct AVE_API ExtensionProperties{
        std::string name { "" };
        alib6::u32 version { 0 };

        inline ExtensionProperties(const VkExtensionProperties & prop){ from(prop); }
        inline ExtensionProperties(std::string_view name = "", alib6::u32 version = 0)
        :name(name),version(version){}

        inline void from(const VkExtensionProperties & prop){
            name = std::string(prop.extensionName, ::strnlen(prop.extensionName, VK_MAX_EXTENSION_NAME_SIZE));
            version = prop.specVersion;
        }

        inline VkExtensionProperties to() const {
            VkExtensionProperties prop = {};

            prop.specVersion = version;
            std::memcpy(
                prop.extensionName,
                name.c_str(),
                std::min<std::size_t>(VK_MAX_EXTENSION_NAME_SIZE - 1, name.size())
            );
            return prop;
        }
    };

    /// 层属性
    struct AVE_API LayerProperties {
        std::string name { "" };
        ApiVersion  specVersion { 0, 1, 0, 0 };
        alib6::u32  implementationVersion { 1 };
        std::string description { "" };

        inline LayerProperties() = default;

        inline LayerProperties(const VkLayerProperties & prop) {
            from(prop);
        }

        inline LayerProperties(
            std::string_view name,
            ApiVersion specVersion = { 0, 1, 0, 0 },
            alib6::u32 implementationVersion = 1,
            std::string_view description = ""
        )
        : name(name)
        , specVersion(specVersion)
        , implementationVersion(implementationVersion)
        , description(description) {}

        inline void from(const VkLayerProperties & prop) {
            name = std::string(prop.layerName, ::strnlen(prop.layerName, VK_MAX_EXTENSION_NAME_SIZE));
            specVersion.from(prop.specVersion);
            implementationVersion = prop.implementationVersion;
            description = prop.description;
        }

        inline VkLayerProperties to() const {
            VkLayerProperties prop = {};

            prop.specVersion = specVersion.to();
            prop.implementationVersion = implementationVersion;

            std::memcpy(
                prop.layerName,
                name.c_str(),
                std::min<std::size_t>(VK_MAX_EXTENSION_NAME_SIZE - 1, name.size())
            );

            std::memcpy(
                prop.description,
                description.c_str(),
                std::min<std::size_t>(VK_MAX_DESCRIPTION_SIZE - 1, description.size())
            );

            return prop;
        }
    };


    template<class Target>
    void write_to_log(Target& target, const Version& version) {
        std::format_to(
            std::back_inserter(target),
            "{}.{}.{}",
            version.major,
            version.minor,
            version.patch
        );
    }

    template<class Target>
    void write_to_log(Target& target, const ApiVersion& version) {
        std::format_to(
            std::back_inserter(target),
            "{}.{}.{}.{}",
            static_cast<unsigned int>(version.variant),
            static_cast<unsigned int>(version.major),
            version.minor,
            version.patch
        );
    }

    template<class Target>
    void write_to_log(Target& target, const ExtensionProperties& properties) {
        std::format_to(
            std::back_inserter(target),
            "{}@{}",
            properties.name,
            properties.version
        );
    }

    template<class Target>
    void write_to_log(Target& target, const LayerProperties& properties) {
        std::format_to(
            std::back_inserter(target),
            "{}@{}.{}.{}.{}.{}",
            properties.name,
            static_cast<unsigned int>(properties.specVersion.variant),
            static_cast<unsigned int>(properties.specVersion.major),
            properties.specVersion.minor,
            properties.specVersion.patch,
            properties.implementationVersion
        );
    }

    inline constexpr ApiVersion ave_vk_1_0 = VK_API_VERSION_1_0;
    inline constexpr ApiVersion ave_vk_1_1 = VK_API_VERSION_1_1;
    inline constexpr ApiVersion ave_vk_1_2 = VK_API_VERSION_1_2;
    inline constexpr ApiVersion ave_vk_1_3 = VK_API_VERSION_1_3;
    inline constexpr ApiVersion ave_vk_1_4 = VK_API_VERSION_1_4;

    /// @brief 包装好的顶点绑定描述，带默认值
    struct AVE_API VertexBinding {
        alib6::u32 binding { 0 };
        alib6::u32 stride { 0 };
        VkVertexInputRate input_rate { VK_VERTEX_INPUT_RATE_VERTEX };

        constexpr operator VkVertexInputBindingDescription() const noexcept {
            return VkVertexInputBindingDescription{
                .binding = binding,
                .stride = stride,
                .inputRate = input_rate
            };
        }
    };

    /// @brief 包装好的顶点属性描述，带默认值
    struct AVE_API VertexAttribute {
        alib6::u32 location { 0 };
        alib6::u32 binding { 0 };
        VkFormat format { VK_FORMAT_R32G32B32A32_SFLOAT };
        alib6::u32 offset { 0 };

        constexpr operator VkVertexInputAttributeDescription() const noexcept {
            return VkVertexInputAttributeDescription{
                .location = location,
                .binding = binding,
                .format = format,
                .offset = offset
            };
        }
    };

    /// @brief Push Constant 成员属性描述
    struct AVE_API ConstantAttribute {
        std::string name {};
        alib6::u32 offset { 0 };
        alib6::u32 size { 0 };
        VkShaderStageFlags stage_flags { VK_SHADER_STAGE_ALL_GRAPHICS };

        constexpr operator VkPushConstantRange() const noexcept {
            return VkPushConstantRange{
                .stageFlags = stage_flags,
                .offset = offset,
                .size = size
            };
        }
    };
}

export namespace std {
    template<>
    struct formatter<ave::Version, char> {
        constexpr auto parse(format_parse_context& ctx) {
            return ctx.begin();
        }

        auto format(const ave::Version& version, format_context& ctx) const {
            return std::format_to(
                ctx.out(),
                "{}.{}.{}",
                version.major,
                version.minor,
                version.patch
            );
        }
    };

    template<>
    struct formatter<ave::ApiVersion, char> {
        constexpr auto parse(format_parse_context& ctx) {
            return ctx.begin();
        }

        auto format(const ave::ApiVersion& version, format_context& ctx) const {
            return std::format_to(
                ctx.out(),
                "{}.{}.{}.{}",
                static_cast<unsigned int>(version.variant),
                static_cast<unsigned int>(version.major),
                version.minor,
                version.patch
            );
        }
    };

    template<>
    struct formatter<ave::ExtensionProperties, char> {
        constexpr auto parse(format_parse_context& ctx) {
            return ctx.begin();
        }

        auto format(const ave::ExtensionProperties& properties, format_context& ctx) const {
            return std::format_to(
                ctx.out(),
                "{}@{}",
                properties.name,
                properties.version
            );
        }
    };

    template<>
    struct formatter<ave::LayerProperties, char> {
        constexpr auto parse(format_parse_context& ctx) {
            return ctx.begin();
        }

        auto format(const ave::LayerProperties& properties, format_context& ctx) const {
            return std::format_to(
                ctx.out(),
                "{}@{}.{}",
                properties.name,
                properties.specVersion,
                properties.implementationVersion
            );
        }
    };

    template<>
    struct formatter<std::vector<ave::ExtensionProperties>, char> {
        constexpr auto parse(format_parse_context& ctx) {
            return ctx.begin();
        }

        auto format(
            const std::vector<ave::ExtensionProperties>& extensions,
            format_context& ctx
        ) const {
            auto out = ctx.out();
            *out++ = '[';

            bool first = true;
            for (const auto& extension : extensions) {
                if (!first) {
                    out = std::format_to(out, ", ");
                }
                first = false;
                out = std::format_to(out, "{}", extension);
            }

            *out++ = ']';
            return out;
        }
    };

    template<>
    struct formatter<std::vector<ave::LayerProperties>, char> {
        constexpr auto parse(format_parse_context& ctx) {
            return ctx.begin();
        }

        auto format(
            const std::vector<ave::LayerProperties>& layers,
            format_context& ctx
        ) const {
            auto out = ctx.out();
            *out++ = '[';

            bool first = true;
            for (const auto& layer : layers) {
                if (!first) {
                    out = std::format_to(out, ", ");
                }
                first = false;
                out = std::format_to(out, "{}", layer);
            }

            *out++ = ']';
            return out;
        }
    };
}
