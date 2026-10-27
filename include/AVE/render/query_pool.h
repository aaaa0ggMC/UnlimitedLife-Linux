#pragma once

#include <AVE/alib.h>
#include <AVE/render/device.h>

/**
 * @file query_pool.h
 * @brief Timestamp query pool wrapper hiding pool creation, reset/write/read boilerplate
 * @version 5.5
 * @date 2026-10-27
 */
#include <AVE/config.h>
#include <vulkan/vulkan.h>



namespace ave {
    struct AVE_API CreateTimestampQueryPoolInfo {
        std::shared_ptr<Device> device;
        /// 用于查询 timestampValidBits 的队列族（必须已由 Device 创建）。
        ave::u32 queue_family { 0 };
        ave::u32 count { 0 };
        mutable ave::ErrorWrapper ew {};
    };

    /// @brief GPU 时间戳查询池；get_valid_bits() == 0 表示该队列族不支持时间戳。
    class AVE_API TimestampQueryPool final {
    private:
        std::shared_ptr<Device> device;
        VkQueryPool pool { VK_NULL_HANDLE };
        ave::u32 count { 0 };
        ave::u32 valid_bits { 0 };
        TimestampQueryPool() = default;
        [[nodiscard]] bool initialize(CreateTimestampQueryPoolInfo ci);

    public:
        ~TimestampQueryPool();
        TimestampQueryPool(const TimestampQueryPool&) = delete;
        TimestampQueryPool& operator=(const TimestampQueryPool&) = delete;
        TimestampQueryPool(TimestampQueryPool&&) = delete;
        TimestampQueryPool& operator=(TimestampQueryPool&&) = delete;

        [[nodiscard]] static std::shared_ptr<TimestampQueryPool> create(
            CreateTimestampQueryPoolInfo ci
        );
        void destroy() noexcept;

        [[nodiscard]] VkQueryPool get_system_handle() const noexcept;
        [[nodiscard]] ave::u32 get_count() const noexcept;
        [[nodiscard]] ave::u32 get_valid_bits() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;

        /// @brief 命令缓冲内重置 [first, first + reset_count) 区间。
        void reset(VkCommandBuffer command_buffer, ave::u32 first, ave::u32 reset_count) const noexcept;
        /// @brief 命令缓冲内写入单个时间戳。
        void write(VkCommandBuffer command_buffer, VkPipelineStageFlagBits stage, ave::u32 index) const noexcept;

        /// @brief 读回 [first, first + out.size())；wait=false 时结果未就绪直接返回 false。
        bool read(
            std::span<std::uint64_t> out,
            ave::u32 first,
            bool wait = false
        ) const noexcept;
    };
}
