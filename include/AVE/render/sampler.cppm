/**
 * @file sampler.cppm
 * @brief Vulkan sampler RAII wrapper
 * @version 5.0
 * @date 2026-09-27
 *
 * @copyright Copyright (c) 2026
 *
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.render:sampler;

import std;
import alib6;
import ave.ecode;
import :device;

export namespace ave {

    struct AVE_API CreateSamplerInfo {
        std::shared_ptr<Device> device;

        VkFilter mag_filter { VK_FILTER_LINEAR };
        VkFilter min_filter { VK_FILTER_LINEAR };
        VkSamplerMipmapMode mipmap_mode { VK_SAMPLER_MIPMAP_MODE_LINEAR };

        VkSamplerAddressMode address_mode_u { VK_SAMPLER_ADDRESS_MODE_REPEAT };
        VkSamplerAddressMode address_mode_v { VK_SAMPLER_ADDRESS_MODE_REPEAT };
        VkSamplerAddressMode address_mode_w { VK_SAMPLER_ADDRESS_MODE_REPEAT };

        float mip_lod_bias { 0.0f };
        /// 启用各向异性过滤要求设备支持 samplerAnisotropy 特性。
        bool anisotropy_enable { false };
        float max_anisotropy { 1.0f };

        bool compare_enable { false };
        VkCompareOp compare_op { VK_COMPARE_OP_ALWAYS };

        float min_lod { 0.0f };
        /// 非 mipmap 纹理保持 0（只采样 mip 0）。
        float max_lod { 0.0f };

        VkBorderColor border_color { VK_BORDER_COLOR_INT_OPAQUE_BLACK };
        bool unnormalized_coordinates { false };

        const void* next { nullptr };
        VkSamplerCreateFlags flags { 0 };

        mutable alib6::ErrorWrapper ew {};
    };

    class AVE_API Sampler final {
    private:
        std::shared_ptr<Device> device;
        VkSampler sampler { VK_NULL_HANDLE };
        Sampler() = default;
        [[nodiscard]] bool initialize(CreateSamplerInfo ci);
    public:
        ~Sampler();
        Sampler(const Sampler&) = delete;
        Sampler& operator=(const Sampler&) = delete;
        Sampler(Sampler&&) = delete;
        Sampler& operator=(Sampler&&) = delete;

        [[nodiscard]] static std::shared_ptr<Sampler> create(
            CreateSamplerInfo ci
        );
        void destroy() noexcept;
        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept;
        [[nodiscard]] VkSampler get_system_handle() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;
    };

} // namespace ave
