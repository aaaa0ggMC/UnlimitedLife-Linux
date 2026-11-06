/**
 * @file ui_bridge.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief ImGui 界面的"数据桥"：只含纯 POD 与不透明句柄
 *
 * 原先用于隔离 C++ modules 与 ImGui/GLFW 头的声明冲突；去 modules 后保留此桥，
 * 继续把场景/ECS 字段登记与 ImGui 后端解耦，两侧均为传统 C++ 翻译单元：
 *
 *  - 后端侧（imgui_impl.cpp，包含 imgui/GLFW 头）：
 *    负责 ImGui 后端初始化、起帧、录制，并把 Frame 里的字段画成界面；
 *  - 场景侧（app_ui.cpp，包含 AVE/alib5 头）：遍历 ECS，把要显示的字段
 *    以 Field/Entity 描述表填进 Frame，改动直接写回组件的真实内存。
 *
 * 本头不 include std/imgui/vulkan，两侧都能安全包含。
 * @version 5.0
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 * @parblock audit AI不准修改
 * 1. OK 10-14 纯数据，没啥大问题
 * 2. OK 10-27 没啥大的改动
 * @endparblock
 * 
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace fog::ui {

    /// 字段类型：决定用哪种 ImGui 控件
    enum class Kind : int {
        Float,
        Int,
        Bool,
        Color3,
        Text,
    };

    /**
     * @brief 一个可编辑字段
     * @note value 直接指向组件的真实成员（float / int / bool / float[3]），改完立刻生效
     */
    struct Field {
        const char* group { nullptr };  ///< 分组名（同一实体内折叠成一层 tree）
        const char* label { nullptr };
        void* value { nullptr };
        const char* text { nullptr };   ///< Kind::Text 时显示的只读文本
        float speed { 0.01f };
        float lo { 0.0f };
        float hi { 0.0f };
        Kind kind { Kind::Float };
        bool clamp_value { false };
    };

    /// 一个实体：tag + 字段表
    struct Entity {
        const char* tag { nullptr };
        std::uint64_t id { 0 };
        const Field* fields { nullptr };
        std::size_t field_count { 0 };
        bool instanced { false };
        bool local_fog { false };
        bool control_field { false };
        std::size_t control_point_count { 0 };
    };

    /// 一帧界面所需的数据（全部由模块侧填好，生命周期覆盖本帧的绘制）
    struct Frame {
        const Field* globals { nullptr };
        std::size_t global_count { 0 };
        const Entity* entities { nullptr };
        std::size_t entity_count { 0 };
        std::size_t total_entities { 0 };
        std::uint32_t instance_count { 0 };
        const char* volume_stats { nullptr };
        const char* gpu_stats { nullptr };
        std::uint32_t local_fog_count { 0 };
        int* local_fog_action { nullptr };
        std::uint64_t* local_fog_target { nullptr };
        // Actions 14=create field volume, 15=attach field, 16=append point, 17=erase point.
        int* control_point_index { nullptr };
        int scene_preset { 0 };
        int* scene_preset_request { nullptr };
        const char* const* scene_preset_names { nullptr };
        int scene_preset_count { 0 };
        const char* editor_status { nullptr };
        bool* inspector_dirty { nullptr };
        /// 界面里改了实例/材质 → 置真，App 下一帧重建实例批
        bool* instances_dirty { nullptr };
        char* filter { nullptr };
        std::size_t filter_size { 0 };
    };

    /// 后端初始化参数（全部用不透明句柄/基本类型，避免引入 Vulkan/GLFW 头）
    struct InitDesc {
        /// Android: ANativeWindow*；桌面: GLFWwindow*。由 Window 持有，须活到后端关闭。
        void* platform_window { nullptr };
        void* instance { nullptr };
        void* physical_device { nullptr };
        void* device { nullptr };
        std::uint32_t queue_family { 0 };
        void* queue { nullptr };
        const void* allocator { nullptr };
        std::int32_t color_format { 0 };
        /// dynamic rendering 下必须与当前 VkRenderingInfo 的深度/模板附件一致
        /// （否则 vkCmdDrawIndexed 报 VUID-...-dynamicRenderingUnusedAttachments-08914/08917）
        std::int32_t depth_format { 0 };
        std::int32_t stencil_format { 0 };
        std::uint32_t image_count { 2 };
        bool dynamic_rendering { true };
        void* legacy_render_pass { nullptr };
        const char* font_path { nullptr };
        /// 界面整体缩放（字体大小与 ImGui style 尺寸一起乘）
        float scale { 1.0f };
    };

    /// 初始化 ImGui 上下文与 Android 或 GLFW 平台后端及 Vulkan 后端；失败返回 false（不抛）
    bool backend_init(const InitDesc& desc);
    /// 释放 ImGui 后端资源（pipeline / 描述符池 / 字体纹理 / 每帧顶点缓冲）
    /// @note 调用方须保证：设备仍有效，且 GPU 已不再使用这些资源
    ///       （本项目约定销毁不隐式 vkDeviceWaitIdle，参考 AVE 的 buffer.cppm）
    void backend_shutdown();

    /// 交换链重建后同步 image count
    void backend_on_swapchain_recreated(std::uint32_t image_count);

    /// 每渲染帧：起帧 → backend_draw(frame) → backend_render(command_buffer)
    void backend_new_frame();
    void backend_draw(const Frame& frame);
    void backend_render(void* command_buffer);

    /// 鼠标/键盘是否被界面占用（此时不要驱动相机）
    bool backend_wants_mouse();
    /// Previous/current panel state; a newly opened inspector is filled on the next frame.
    bool backend_needs_entities();
    // Headless regression: grouping IDs and closed component trees.
    bool check_fields(); 
}
