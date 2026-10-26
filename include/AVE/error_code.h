/**
 * @file error_code.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief AVE 定长类型别名与 AVE 专有错误码
 * @version 5.5
 * @date 2026-10-26
 *
 * 错误上报设施（Error / ErrorWrapper / ErrorRecord / CodeWithLocation）已全量回移至
 * 头文件式 alib5（`include/alib5/aerror.h`，行为对齐 alib6 `alib6.core:error`）。
 * 本文件不再自带重复实现，只做两件事：
 *  1. 把 alib5 的错误类型以 ave 命名空间别名/薄转发的方式暴露出来，
 *     保持 AVE 既有公开调用点（`ave::Error` / `ave::ErrorWrapper` /
 *     `ave::CodeWithLocation` / `ave::ErrorRecord`）源码兼容。
 *  2. 保留 AVE 专有错误码枚举（原 ave.ecode 模块内容）。
 *
 * `ave::ErrorWrapper` 是一个薄转发层：存储与语义全部委托 alib5::Error，
 * 仅保留 AVE 既有的 `(message, code)` / `(code, fmt, args...)` 调用约定。
 */
#pragma once

#include <AVE/config.h>

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <format>
#include <source_location>
#include <utility>

#include <alib5/aerror.h> // panicf / vpanicf 作为无宿主 Error 时的后端

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

    /// 附带源码位置的错误码包装：直接复用 alib5 实现（删除 AVE 自有重复定义）。
    using alib5::CodeWithLocation;
    /// 单条错误记录：直接复用 alib5 实现。
    using alib5::ErrorRecord;
    /// 错误累积容器：直接复用 alib5 实现。
    using alib5::Error;

    /**
     * @brief 轻量错误上报包装（AVE 调用约定的薄转发层）。
     *
     * @details
     * 存储与累积语义完全来自 `alib5::Error`；这里只保留 AVE 既有的两个便捷重载：
     *  - `report(message, code)`
     *  - `report(code, fmt, args...)`
     * 以及 `operator bool()`。无宿主 Error 时的行为与迁移前一致：走 alib5 的
     * panic 后端（alib5 默认策略）。
     */
    struct ErrorWrapper {
        alib5::Error* err { nullptr };

        ErrorWrapper() = default;
        ErrorWrapper(alib5::Error& ctx) : err(&ctx) {}
        ErrorWrapper(alib5::Error&&) = delete("Rvalue is not allowed!");
        ErrorWrapper(const alib5::Error&) = delete("Const lvalue is not allowed!");

    private:
        void invoke(std::string_view str, i64 code) {
            // 注意：panicf 是宏，不能加 alib5:: 限定；这里直接调函数模板。
            alib5::Panicf("{}\nCode: {}", std::source_location::current(), str, code);
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
        void report(std::pmr::string&& str, CodeWithLocation cc = CodeWithLocation()) {
            __report(std::move(str), cc);
        }

        template<class T> requires requires(T v) { std::string_view(v); }
        void report(T value, CodeWithLocation cc = CodeWithLocation()) {
            __report(std::string_view(value), cc);
        }

        template<class... Args>
        void report(CodeWithLocation cc, const std::format_string<Args...>& fmt, Args&&... args) {
            std::pmr::string str(err ? err->get_allocator() : pmr::get_default_resource());
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
