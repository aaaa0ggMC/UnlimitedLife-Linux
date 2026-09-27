/**
 * @file descriptor_pool.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Vulkan Descriptor Pool 及描述符集分配/写入的 RAII 封装
 * @version 5.0
 * @date 2026-09-27
 *
 * @copyright Copyright (c) 2026
 *
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.render:descriptor;

import std;
import alib6;
import ave.ecode;
import :device;

export namespace ave {

    /// @brief DescriptorPool 创建阶段的只读上下文（由 RenderProfile 构造）
    class AVE_API WithDescriptorPoolInput {
    private:
        std::shared_ptr<Device> device;
        alib6::u32 frame_count { 1 };

    public:
        WithDescriptorPoolInput(
            std::shared_ptr<Device> target_device,
            alib6::u32 target_frame_count = 1
        );
        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept;
        [[nodiscard]] alib6::u32 get_frame_count() const noexcept;
    };

    /// @brief 描述符池容量声明，带默认值与工厂方法
    struct AVE_API DescriptorPoolSize {
        VkDescriptorType type { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };
        alib6::u32 count { 1 };

        constexpr operator VkDescriptorPoolSize() const noexcept {
            return VkDescriptorPoolSize{
                .type = type,
                .descriptorCount = count
            };
        }

        static constexpr DescriptorPoolSize uniform_buffer(alib6::u32 count) noexcept {
            return DescriptorPoolSize{
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .count = count
            };
        }

        static constexpr DescriptorPoolSize combined_image_sampler(alib6::u32 count) noexcept {
            return DescriptorPoolSize{
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .count = count
            };
        }

        static constexpr DescriptorPoolSize storage_buffer(alib6::u32 count) noexcept {
            return DescriptorPoolSize{
                .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .count = count
            };
        }
    };

    struct AVE_API CreateDescriptorPoolInfo {
        std::shared_ptr<Device> device;
        std::vector<DescriptorPoolSize> pool_sizes {};
        alib6::u32 max_sets { 0 };
        VkDescriptorPoolCreateFlags flags { 0 };
        mutable alib6::ErrorWrapper ew {};
    };

    /// @brief 默认配置：每帧一个 uniform buffer 描述符集。
    AVE_API void default_configure_descriptor_pool(
        WithDescriptorPoolInput& input,
        CreateDescriptorPoolInfo& ci
    );

    class AVE_API DescriptorPool final {
    private:
        std::shared_ptr<Device> device;
        VkDescriptorPool pool { VK_NULL_HANDLE };
        DescriptorPool() = default;
        [[nodiscard]] bool initialize(CreateDescriptorPoolInfo ci);
    public:
        ~DescriptorPool();
        DescriptorPool(const DescriptorPool&) = delete;
        DescriptorPool& operator=(const DescriptorPool&) = delete;
        DescriptorPool(DescriptorPool&&) = delete;
        DescriptorPool& operator=(DescriptorPool&&) = delete;

        [[nodiscard]] static std::shared_ptr<DescriptorPool> create(
            CreateDescriptorPoolInfo ci
        );
        void destroy() noexcept;
        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept;
        [[nodiscard]] VkDescriptorPool get_system_handle() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;

        /// @brief 从池中分配一个 descriptor set，失败返回 VK_NULL_HANDLE。
        [[nodiscard]] VkDescriptorSet allocate_descriptor_set(
            VkDescriptorSetLayout layout,
            alib6::ErrorWrapper ew = {}
        );

        /// @brief 按给定顺序批量分配 descriptor set。
        [[nodiscard]] std::vector<VkDescriptorSet> allocate_descriptor_sets(
            std::span<const VkDescriptorSetLayout> layouts,
            alib6::ErrorWrapper ew = {}
        );

        /// @brief 向已有 descriptor set 写入一个 buffer 描述符。
        void write_buffer(
            VkDescriptorSet dst_set,
            alib6::u32 dst_binding,
            VkDescriptorType descriptor_type,
            VkBuffer buffer,
            VkDeviceSize offset = 0,
            VkDeviceSize range = VK_WHOLE_SIZE,
            alib6::u32 dst_array_element = 0
        );

        /// @brief 向已有 descriptor set 写入一个图像（sampler + view）描述符。
        void write_image(
            VkDescriptorSet dst_set,
            alib6::u32 dst_binding,
            VkSampler sampler,
            VkImageView view,
            VkImageLayout image_layout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            alib6::u32 dst_array_element = 0
        );
    };

} // namespace ave
