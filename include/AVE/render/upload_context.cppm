/**
 * @file upload_context.cppm
 * @brief Vulkan staging upload context and completion ticket
 * @version 5.0
 * @date 2026-09-15
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.render:upload_context;

import std;
import alib6;
import ave.ecode;
import :device;
import :command;

export namespace ave {

    class UploadContext;

    class AVE_API UploadTicket final {
    private:
        friend class UploadContext;
        struct State {
            std::shared_ptr<Device> device;
            VkCommandPool pool { VK_NULL_HANDLE };
            VkCommandBuffer command_buffer { VK_NULL_HANDLE };
            VkFence fence { VK_NULL_HANDLE };
            std::shared_ptr<const void> session_lifetime;
            std::shared_ptr<std::mutex> pool_mutex;
            ~State();
        };
        std::shared_ptr<State> state;

    public:
        UploadTicket() = default;
        explicit UploadTicket(std::shared_ptr<State> s) : state(std::move(s)) {}
        ~UploadTicket() = default;

        UploadTicket(const UploadTicket&) = default;
        UploadTicket& operator=(const UploadTicket&) = default;
        UploadTicket(UploadTicket&&) noexcept = default;
        UploadTicket& operator=(UploadTicket&&) noexcept = default;

        [[nodiscard]] bool wait(uint64_t timeout_ns = std::numeric_limits<uint64_t>::max(),
                                alib6::ErrorWrapper ew = {}) const;
        [[nodiscard]] bool is_ready() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept {
            return state != nullptr && state->fence != VK_NULL_HANDLE;
        }
    };

    struct AVE_API CreateUploadContextInfo {
        std::shared_ptr<Device> device;
        VkQueue queue { VK_NULL_HANDLE };
        alib6::u32 queue_family { 0 };
        mutable alib6::ErrorWrapper ew {};
    };

    class AVE_API UploadContext final {
    private:
        struct Impl;
        std::shared_ptr<Impl> impl;

    public:
        UploadContext() = default;
        explicit UploadContext(CreateUploadContextInfo ci);
        ~UploadContext() = default;

        [[nodiscard]] static std::shared_ptr<UploadContext> create(CreateUploadContextInfo ci);

        [[nodiscard]] const std::shared_ptr<Device>& get_device() const noexcept;
        [[nodiscard]] VkQueue get_queue() const noexcept;
        [[nodiscard]] alib6::u32 get_queue_family() const noexcept;

        /// 录制并提交单次 Buffer 复制命令
        [[nodiscard]] UploadTicket submit_copy(
            VkBuffer src, VkDeviceSize src_offset,
            VkBuffer dst, VkDeviceSize dst_offset,
            VkDeviceSize size,
            std::shared_ptr<const void> session_lifetime = nullptr,
            alib6::ErrorWrapper ew = {}
        );

        [[nodiscard]] explicit operator bool() const noexcept;
    };

} // namespace ave
