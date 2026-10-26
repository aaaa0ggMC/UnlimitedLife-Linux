/**
 * @file error_code.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief AVE 定长类型别名、错误码与自包含错误报告设施（去 modules 迁移）
 * @version 5.0
 * @date 2026-10-26
 *
 * 原 ave.ecode 模块 + 原先经 alib6 提供的定长类型与 ErrorWrapper/Error。
 * alib5（头文件式、panic 错误模型）不提供这些符号；为「保留 AVE 公开 API 与
 * 返回值 + report 语义」，在 ave 命名空间内以 std::pmr + alib5::panicf 重建。
 * 这是最小兼容设施：不造假 alib6 命名空间、不搬基础库、不改成到处 panic。
 */
#pragma once

#include <AVE/config.h>

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <memory_resource>
#include <format>
#include <source_location>
#include <optional>
#include <type_traits>
#include <utility>
#include <algorithm>
#include <iterator>

#include <alib5/adebug.h> // panicf / vpanicf 作为无宿主 Error 时的后端

namespace ave {

    // 定长基本数值类型（原先经 alib6::core:types 提供，现由 AVE 自带）。
    using i8  = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;
    using u8  = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;
    using usize = std::size_t;
    using f32 = float;
    using f64 = double;

    namespace pmr = std::pmr;

    /// 附带源码位置的错误码包装（对齐原 alib6::CodeWithLocation 行为）。
    struct CodeWithLocation {
        i64 code;
        std::source_location location;

        CodeWithLocation(i64 c = 0, std::source_location loc = std::source_location::current()) noexcept
            : code(c), location(loc) {}
    };

    /// 单条错误记录。
    struct ErrorRecord {
        pmr::string message;
        i64 code;
        std::source_location location;

        ErrorRecord(pmr::string&& message, i64 code,
                    std::source_location loc = std::source_location::current())
            : message(std::move(message)), code(code), location(loc) {}
    };

    /// 错误累积容器（不收缩，语义对齐原 alib6::Error）。
    struct Error {
        pmr::vector<ErrorRecord> messages;
        usize cursor { 0 };

        Error(pmr::memory_resource* mem = pmr::get_default_resource()) : messages(mem) {}

        [[nodiscard]] pmr::memory_resource* get_allocator() const noexcept {
            return messages.get_allocator().resource();
        }

        void clear() noexcept { cursor = 0; }

        void report(pmr::string&& str, CodeWithLocation cc = CodeWithLocation()) {
            if(messages.size() > cursor) {
                messages[cursor] = ErrorRecord(std::move(str), cc.code, cc.location);
            } else {
                messages.emplace_back(std::move(str), cc.code, cc.location);
            }
            ++cursor;
        }

        void report(std::string_view str, CodeWithLocation cc = CodeWithLocation()) {
            report(pmr::string(str, get_allocator()), cc);
        }

        template<class... Args>
        void report(CodeWithLocation cc, const std::format_string<Args...>& fmt, Args&&... args) {
            pmr::string str(get_allocator());
            std::format_to(std::back_inserter(str), fmt, std::forward<Args>(args)...);
            report(std::move(str), cc);
        }

        [[nodiscard]] bool has_error() const noexcept { return cursor > 0; }
        [[nodiscard]] explicit operator bool() const noexcept { return has_error(); }
        [[nodiscard]] usize size() const noexcept { return cursor; }

        [[nodiscard]] const ErrorRecord& operator[](usize idx) const { return messages[idx]; }
        [[nodiscard]] ErrorRecord& operator[](usize idx) { return messages[idx]; }

        auto begin() const noexcept { return messages.begin(); }
        auto end() const noexcept { return messages.begin() + cursor; }
    };

    /// 轻量错误上报包装：有宿主 Error 则累积，否则 panic（对齐 ALIB6_ERROR_USE_PANIC）。
    struct ErrorWrapper {
        Error* err { nullptr };

        ErrorWrapper() = default;
        ErrorWrapper(Error& ctx) : err(&ctx) {}
        ErrorWrapper(Error&&) = delete("Rvalue is not allowed!");
        ErrorWrapper(const Error&) = delete("Const lvalue is not allowed!");

    private:
        void invoke(std::string_view str, i64 code) {
            panicf("{}\nCode: {}", str, code);
        }
        template<class T>
        void __report(T&& str, CodeWithLocation cc) {
            if(err) {
                err->report(std::forward<T>(str), cc);
            } else {
                invoke(std::string_view(str), cc.code);
            }
        }

    public:
        void report(pmr::string&& str, CodeWithLocation cc = CodeWithLocation()) {
            __report(std::move(str), cc);
        }

        template<class T> requires requires(T v) { std::string_view(v); }
        void report(T value, CodeWithLocation cc = CodeWithLocation()) {
            __report(std::string_view(value), cc);
        }

        template<class... Args>
        void report(CodeWithLocation cc, const std::format_string<Args...>& fmt, Args&&... args) {
            pmr::string str(err ? err->get_allocator() : pmr::get_default_resource());
            std::format_to(std::back_inserter(str), fmt, std::forward<Args>(args)...);
            __report(std::move(str), cc);
        }

        [[nodiscard]] explicit operator bool() const noexcept { return err != nullptr; }
    };

} // namespace ave

// AVE 专有错误码（原 ave.ecode 模块内容）。
namespace ave {
    enum ErrorCode : i64 {
        ave_success            = 0,
        ave_not_in_main_thread = 100'001,
        ave_bad_glfw           = 100'002,
        ave_already_created    = 100'003,
        ave_vk_create_instance = 100'004,
        ave_vk_create_debug_messenger = 100'005,
        ave_vk_create_surface = 100'006,
        ave_vk_select_physical_device = 100'007,
        ave_vk_create_device = 100'008,
        ave_vk_create_swapchain = 100'009,
        ave_vk_create_sync_objects = 100'010,
        ave_vk_create_render_pass = 100'011,
        ave_vk_create_framebuffer = 100'012,
        ave_vk_create_pipeline_layout = 100'013,
        ave_vk_create_graphics_pipeline = 100'014,
        ave_vk_create_shader_module = 100'015,
        ave_vk_create_command_pool = 100'016,
        ave_vk_allocate_command_buffers = 100'017,
        ave_vk_draw_frame = 100'018,
        ave_vk_create_image = 100'019,
        ave_vk_create_buffer = 100'020,
        ave_vk_map_buffer = 100'021,
        ave_invalid_vertex_layout = 100'022,
        ave_invalid_constant_layout = 100'023,
        ave_vk_create_descriptor_pool = 100'024,
        ave_vk_create_image_view = 100'025,
        ave_vk_image_upload = 100'026,
        ave_vk_create_sampler = 100'027,
        ave_shape_load_failed = 100'028,
        ave_vk_create_compute_pipeline = 100'029
    };
}
