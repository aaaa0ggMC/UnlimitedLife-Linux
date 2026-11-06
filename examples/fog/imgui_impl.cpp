/**
 * @file imgui_impl.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief ImGui 经典侧：GLFW + Vulkan 后端 + 菜单式单窗口界面
 *
 * 本 TU 只 include imgui/GLFW 的经典头，**不** import ave/std —— 见 ui_bridge.h 顶部说明。
 * 文件拆分参考 examples/agetest 的 imgui.cpp / imgui_impl.cpp。
 * @version 5.0
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 * @parblock audit AI不能修改 
 * 1. OK 10-14 问题不大，这里使用了指针擦除数据类型来绕过 modules vs plain 的问题
 * @endparblock
 * 
 */
#include <cstdio>
#include <string_view>

#include <imgui.h>
#if defined(__ANDROID__)
#include <android/native_window.h>
#include <imgui_impl_android.h>
#else
#include <imgui_impl_glfw.h>
#endif
#include <imgui_impl_vulkan.h>

#include "ui_bridge.h"

namespace {

    void check_vk_result(VkResult err) {
        if(err == VK_SUCCESS) return;
        std::fprintf(stderr, "[ImGui] Vulkan backend error: VkResult=%d\n", static_cast<int>(err));
    }

    /// 主窗口里的面板（菜单栏上平铺切换，单窗口不堆叠）
    enum class Panel : int {
        Scene = 0,
        Stats,
        Fog,
        Lighting,
        Sky,
        Inspector,
    };

    /// 就绪标志：Init 可能中途失败，Shutdown 要能按走到哪一步回滚、且可重复调用
    bool g_context_ready { false };
    bool g_platform_ready { false };
    bool g_vulkan_ready { false };

    Panel g_panel { Panel::Stats };
    float g_scale { 1.0f };
    /// 界面不透明度（参考 agetest 的 im_winalpha / im_uialpha 两个旋钮）：
    /// 窗口背景（能透出后面的场景）与控件整体（文字/边框/控件一起淡）
    float g_window_bg_alpha { 0.94f };   ///< 初始化时取 ImGui 默认 WindowBg.a
    float g_ui_alpha { 1.0f };
    float g_base_scale { 1.0f };
    ImGuiStyle g_base_style {};

    bool in_groups(const char* group, const char* const* groups, std::size_t count) {
        if(group == nullptr) group = "";
        const std::string_view name { group };
        for(std::size_t i = 0; i < count; ++i) {
            if(name == groups[i]) return true;
        }
        return false;
    }

    /// 画字段表；groups 非空时只画属于这些分组的字段（按面板切分用）
    void draw_fields(const fog::ui::Field* fields, std::size_t count,
                     const char* const* groups, std::size_t group_count, bool& changed) {
        const char* current_group = nullptr;
        bool group_open = false;
        for(std::size_t i = 0; i < count; ++i) {
            const fog::ui::Field& field = fields[i];
            if(groups != nullptr && group_count > 0 && !in_groups(field.group, groups, group_count)) {
                continue;
            }

            const char* group = field.group == nullptr ? "" : field.group;
            if(current_group == nullptr || std::string_view(group) != current_group) {
                if(group_open) {
                    ImGui::TreePop();
                    group_open = false;
                }
                current_group = group;
                if(current_group[0] != '\0') {
                    if(group_count == 0) {
                        // 检查器：整棵实体树，分组折叠成一层
                        group_open = ImGui::TreeNode(current_group);
                        if(!group_open) continue;
                    } else {
                        // 面板已按分组切分：平铺时也要有小节标题，否则拆分不可见
                        ImGui::SeparatorText(current_group);
                    }
                }
            }

            // A collapsed component hides every field, not just its first one.
            if(group_count == 0 && current_group[0] != '\0' && !group_open) continue;
            ImGui::PushID(group); // Shared labels (position/color/enabled) need a section scope.
            switch(field.kind) {
                case fog::ui::Kind::Float: {
                    auto* value = static_cast<float*>(field.value);
                    if(ImGui::DragFloat(field.label, value, field.speed, field.lo, field.hi,
                                       "%.3f", field.clamp_value ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None)) changed = true;
                    break;
                }
                case fog::ui::Kind::Int: {
                    auto* value = static_cast<int*>(field.value);
                    if(ImGui::DragInt(field.label, value, field.speed, static_cast<int>(field.lo),
                                      static_cast<int>(field.hi), "%d",
                                      field.clamp_value ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None)) changed = true;
                    break;
                }
                case fog::ui::Kind::Bool: {
                    auto* value = static_cast<bool*>(field.value);
                    if(ImGui::Checkbox(field.label, value)) changed = true;
                    break;
                }
                case fog::ui::Kind::Color3: {
                    auto* value = static_cast<float*>(field.value);
                    if(ImGui::ColorEdit3(field.label, value)) changed = true;
                    break;
                }
                case fog::ui::Kind::Text: {
                    ImGui::TextDisabled("%s", field.text == nullptr ? "" : field.text);
                    break;
                }
            }
            ImGui::PopID();
        }
        if(group_open) ImGui::TreePop();
    }

    void draw_stats(const fog::ui::Frame& frame) {
        const ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("ImGui %.1f FPS   (%.2f ms)", io.Framerate,
                    1000.0f / (io.Framerate > 1.0f ? io.Framerate : 1.0f));
        ImGui::Text("Entities %zu    Instances %u", frame.total_entities, frame.instance_count);
        if(frame.volume_stats) ImGui::TextWrapped("%s", frame.volume_stats);
        if(frame.gpu_stats) ImGui::TextWrapped("%s", frame.gpu_stats);
        ImGui::Text("Active local fog volumes: %u / 16", frame.local_fog_count);

        ImGui::SeparatorText("界面");
        ImGui::DragFloat("窗口背景不透明度", &g_window_bg_alpha, 0.005f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("控件不透明度", &g_ui_alpha, 0.005f, 0.2f, 1.0f, "%.2f");
        float scale = g_scale;
        if(ImGui::DragFloat("界面缩放", &scale, 0.01f, 0.8f, 2.5f, "%.2f")) {
            // 立即生效：style 基于"未缩放"的副本重算，字体用 FontGlobalScale 补偿（避免累积）
            g_scale = scale;
            ImGuiStyle& style = ImGui::GetStyle();
            style = g_base_style;
            style.ScaleAllSizes(g_scale);
            ImGui::GetIO().FontGlobalScale = g_scale / g_base_scale;
        }
    }

    void draw_inspector(const fog::ui::Frame& frame, bool& changed) {
        if(frame.filter != nullptr) {
            ImGui::InputTextWithHint("##filter", "按 tag 过滤...", frame.filter, frame.filter_size);
            ImGui::SameLine();
        }
        ImGui::TextDisabled("%zu / %zu", frame.entity_count, frame.total_entities);
        if(ImGui::BeginChild("entities")) {
            for(std::size_t i = 0; i < frame.entity_count; ++i) {
                const fog::ui::Entity& entity = frame.entities[i];
                if(frame.filter != nullptr && frame.filter[0] != '\0'
                   && std::string_view(entity.tag).find(frame.filter) == std::string_view::npos) {
                    continue;
                }
                ImGui::PushID(static_cast<int>(entity.id));
                if(ImGui::TreeNode("##entity", "%s", entity.tag)) {
                    if(entity.local_fog && frame.local_fog_action && frame.local_fog_target) {
                        const auto action = [&](const char* label, int value) {
                            if(ImGui::Button(label)) {
                                *frame.local_fog_target = entity.id;
                                *frame.local_fog_action = value;
                            }
                        };
                        action("复制", 4); ImGui::SameLine();
                        action("移到视线前", 6); ImGui::SameLine();
                        action("相机移近", 7);
                        if(!entity.control_field) action("添加控制点雾组件", 15);
                        else {
                            ImGui::Text("控制点 %zu / 32（0 点为空场）", entity.control_point_count);
                            ImGui::BeginDisabled(entity.control_point_count >= 32);
                            action("添加控制点", 16);
                            ImGui::EndDisabled();
                            if(frame.control_point_index) for(std::size_t p = 0; p < entity.control_point_count; ++p) {
                                ImGui::PushID(static_cast<int>(p));
                                ImGui::Text("控制点 %zu", p); ImGui::SameLine();
                                if(ImGui::Button("删除点")) {
                                    *frame.local_fog_target = entity.id;
                                    *frame.local_fog_action = 17;
                                    *frame.control_point_index = static_cast<int>(p);
                                }
                                ImGui::PopID();
                            }
                        }
                        if(ImGui::Button("删除...")) ImGui::OpenPopup("删除局部雾");
                        if(ImGui::BeginPopup("删除局部雾")) {
                            ImGui::TextUnformatted("只删除这个局部雾实体？");
                            action("确认删除", 5);
                            if(*frame.local_fog_action == 5) ImGui::CloseCurrentPopup();
                            ImGui::SameLine();
                            if(ImGui::Button("取消")) ImGui::CloseCurrentPopup();
                            ImGui::EndPopup();
                        }
                    }
                    bool entity_changed = false;
                    draw_fields(entity.fields, entity.field_count, nullptr, 0, entity_changed);
                    if(entity_changed) {
                        changed = true;
                        if(frame.inspector_dirty) *frame.inspector_dirty = true;
                        if(entity.instanced && frame.instances_dirty) *frame.instances_dirty = true;
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
    }

} // namespace

namespace fog::ui {

    bool check_fields() {
        if(ImGui::GetCurrentContext()) return false;
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(800, 600);
        io.DeltaTime = 1.0f / 60.0f;
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        ImGui::NewFrame();
        ImGui::Begin("field regression");
        float a = 1, b = 2;
        const Field first {.group = "spot", .label = "value", .value = &a};
        const Field second {.group = "point", .label = "value", .value = &b};
        const char* groups[] = {"spot", "point"};
        bool changed = false;
        draw_fields(&first, 1, groups, 2, changed);
        const auto first_id = ImGui::GetItemID();
        draw_fields(&second, 1, groups, 2, changed);
        const bool unique_ids = first_id != ImGui::GetItemID();
        const Field closed[] = {
            {.group = "closed", .label = "first", .value = &a},
            {.group = "closed", .label = "second", .value = &b},
        };
        const auto tree_id = ImGui::GetID("closed");
        ImGui::SetNextItemOpen(false, ImGuiCond_Always);
        draw_fields(closed, 2, nullptr, 0, changed);
        const bool hidden_fields = ImGui::GetItemID() == tree_id;
        ImGui::End();
        ImGui::EndFrame();
        ImGui::DestroyContext();
        return unique_ids && hidden_fields && !changed;
    }

    bool backend_init(const InitDesc& desc) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        g_context_ready = true;

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        // 不开多视口：否则 ImGui 会另开系统窗口
        io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
        ImGui::StyleColorsDark();

        g_scale = desc.scale > 0.0f ? desc.scale : 1.0f;

        if(desc.font_path != nullptr && desc.font_path[0] != '\0') {
            if(ImFont* font = io.Fonts->AddFontFromFileTTF(
                   desc.font_path, 16.0f * g_scale, nullptr,
                   io.Fonts->GetGlyphRangesChineseSimplifiedCommon())) {
                io.FontDefault = font;
            } else {
                std::fprintf(stderr, "[ImGui] font missing: %s (CJK glyphs will be blank)\n", desc.font_path);
            }
        }
        // 字体以外的内边距/控件高度一起放大，否则只是字变大、布局还是旧的
        ImGui::GetStyle().ScaleAllSizes(g_scale);
        g_base_style = ImGui::GetStyle();   // 供运行时改缩放时重算
        g_base_scale = g_scale;
        g_window_bg_alpha = ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w;

        if(!desc.platform_window) {
            std::fprintf(stderr, "[ImGui] platform window missing\n");
            return false;
        }
#if defined(__ANDROID__)
        // 输入由宿主在渲染线程注入 ImGui IO，不安装 GLFW 或第二套触摸回调。
        if(!ImGui_ImplAndroid_Init(static_cast<ANativeWindow*>(desc.platform_window))) {
            std::fprintf(stderr, "[ImGui] Android backend init failed\n");
            return false;
        }
#else
        if(!ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow*>(desc.platform_window), true)) {
            std::fprintf(stderr, "[ImGui] GLFW backend init failed\n");
            return false;
        }
#endif
        g_platform_ready = true;

        const auto color_format = static_cast<VkFormat>(desc.color_format);
        VkPipelineRenderingCreateInfo rendering_info {};
        rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachmentFormats = &color_format;
        // 雾场景的 dynamic rendering 带 D24S8 深度/模板附件，ImGui 的 pipeline
        // 必须声明相同的格式，否则每帧 draw 都会被校验层报错
        rendering_info.depthAttachmentFormat = static_cast<VkFormat>(desc.depth_format);
        rendering_info.stencilAttachmentFormat = static_cast<VkFormat>(desc.stencil_format);

        ImGui_ImplVulkan_InitInfo info {};
#if defined(__ANDROID__)
        info.ApiVersion = VK_API_VERSION_1_1;
#else
        info.ApiVersion = VK_API_VERSION_1_3;
#endif
        info.Instance = static_cast<VkInstance>(desc.instance);
        info.PhysicalDevice = static_cast<VkPhysicalDevice>(desc.physical_device);
        info.Device = static_cast<VkDevice>(desc.device);
        info.QueueFamily = desc.queue_family;
        info.Queue = static_cast<VkQueue>(desc.queue);
        info.DescriptorPoolSize = 32;  // 让后端自建描述符池
        info.MinImageCount = 2;
        info.ImageCount = desc.image_count < 2 ? 2 : desc.image_count;
        info.UseDynamicRendering = desc.dynamic_rendering;
        info.Allocator = static_cast<const VkAllocationCallbacks*>(desc.allocator);
        info.CheckVkResultFn = check_vk_result;
        info.MinAllocationSize = 1024 * 1024;
        if(desc.dynamic_rendering) {
            info.PipelineInfoMain.PipelineRenderingCreateInfo = rendering_info;
        } else {
            info.PipelineInfoMain.RenderPass = static_cast<VkRenderPass>(desc.legacy_render_pass);
        }

        if(!ImGui_ImplVulkan_Init(&info)) {
            std::fprintf(stderr, "[ImGui] Vulkan backend init failed\n");
            return false;
        }
        g_vulkan_ready = true;
        return true;
    }

    void backend_shutdown() {
        // 顺序与 Init 相反，且只回滚已经成功的那几步（Init 失败时也能安全调用）
        if(g_vulkan_ready) {
            ImGui_ImplVulkan_Shutdown();
            g_vulkan_ready = false;
        }
        if(g_platform_ready) {
#if defined(__ANDROID__)
            ImGui_ImplAndroid_Shutdown();
#else
            ImGui_ImplGlfw_Shutdown();
#endif
            g_platform_ready = false;
        }
        if(g_context_ready) {
            ImGui::DestroyContext();
            g_context_ready = false;
        }
    }

    void backend_on_swapchain_recreated(std::uint32_t image_count) {
        if(!g_vulkan_ready) return;
        ImGui_ImplVulkan_SetMinImageCount(image_count < 2 ? 2 : image_count);
    }

    void backend_new_frame() {
        if(!g_vulkan_ready) return;
        ImGui_ImplVulkan_NewFrame();
#if defined(__ANDROID__)
        // Android 后端每帧从 ANativeWindow 获取尺寸；横屏由宿主 SurfaceView 决定。
        ImGui_ImplAndroid_NewFrame();
#else
        ImGui_ImplGlfw_NewFrame();
#endif
        ImGui::NewFrame();
    }

    void backend_render(void* command_buffer) {
        if(!g_vulkan_ready) return;
        ImGui::Render();
        if(ImDrawData* draw_data = ImGui::GetDrawData()) {
            ImGui_ImplVulkan_RenderDrawData(draw_data, static_cast<VkCommandBuffer>(command_buffer));
        }
    }

    bool backend_needs_entities() {
        return g_context_ready && g_panel == Panel::Inspector;
    }

    bool backend_wants_mouse() {
        if(!g_context_ready) return false;
        const ImGuiIO& io = ImGui::GetIO();
        return io.WantCaptureMouse || io.WantCaptureKeyboard;
    }

    void backend_draw(const Frame& frame) {
        if(!g_vulkan_ready) return;
        static const char* const fog_groups[] = { "雾密度场", "雾", "体积雾·网格", "体积雾·时域",
                                                  "体积雾·散射", "体积雾·天气", "体积雾·边界", "体积雾·显示" };
        static const char* const lighting_groups[] = { "光照", "阴影", "表面", "点光源" };
        static const char* const sky_groups[] = { "天空" };

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_ui_alpha);
        ImGui::SetNextWindowSize(ImVec2(470.0f * g_scale, 520.0f * g_scale), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(24.0f, 72.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(g_window_bg_alpha);

        if(ImGui::Begin("Fog", nullptr, ImGuiWindowFlags_MenuBar)) {
            if(ImGui::BeginMenuBar()) {
                if(ImGui::MenuItem("场景", nullptr, g_panel == Panel::Scene)) g_panel = Panel::Scene;
                if(ImGui::MenuItem("统计", nullptr, g_panel == Panel::Stats)) g_panel = Panel::Stats;
                if(ImGui::MenuItem("雾", nullptr, g_panel == Panel::Fog)) g_panel = Panel::Fog;
                if(ImGui::MenuItem("光照", nullptr, g_panel == Panel::Lighting)) g_panel = Panel::Lighting;
                if(ImGui::MenuItem("天空", nullptr, g_panel == Panel::Sky)) g_panel = Panel::Sky;
                if(ImGui::MenuItem("检查器", nullptr, g_panel == Panel::Inspector)) g_panel = Panel::Inspector;
                ImGui::EndMenuBar();
            }

            if(g_panel == Panel::Scene && frame.scene_preset_request) {
                ImGui::TextWrapped("切换场景会替换雾、光照、天空和局部雾；场景几何与相机位置保持不变，便于同视角比较。");
                ImGui::Separator();
                for(int i = 0; i < frame.scene_preset_count; ++i) {
                    ImGui::PushID(i);
                    const auto* name = frame.scene_preset_names ? frame.scene_preset_names[i] : "?";
                    if(ImGui::Selectable(name, i == frame.scene_preset))
                        *frame.scene_preset_request = i;
                    ImGui::PopID();
                }
                if(frame.editor_status) {
                    ImGui::Separator();
                    ImGui::TextWrapped("%s", frame.editor_status);
                }
            }

            bool changed = false;
            switch(g_panel) {
                case Panel::Scene:
                    break;
                case Panel::Stats:
                    draw_stats(frame);
                    break;
                case Panel::Fog:
                    if(frame.local_fog_action) {
                        if(ImGui::Button("添加盒形局部雾")) *frame.local_fog_action = 1;
                        ImGui::SameLine();
                        if(ImGui::Button("添加纹理椭球雾")) *frame.local_fog_action = 2;
                        if(ImGui::Button("添加控制点雾")) *frame.local_fog_action = 14;
                        if(ImGui::Button("重载局部雾 3D 纹理")) *frame.local_fog_action = 3;
                        if(ImGui::Button("保存局部雾")) *frame.local_fog_action = 8;
                        ImGui::SameLine();
                        if(ImGui::Button("载入局部雾...")) ImGui::OpenPopup("载入局部雾");
                        if(ImGui::BeginPopup("载入局部雾")) {
                            ImGui::TextUnformatted("将替换当前全部局部雾（先保存可保留当前布局）。");
                            if(ImGui::Button("确认载入")) {
                                *frame.local_fog_action = 9;
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndPopup();
                        }
                        if(frame.editor_status) ImGui::TextWrapped("%s", frame.editor_status);
                        if(ImGui::Button("性能档")) *frame.local_fog_action = 10;
                        ImGui::SameLine();
                        if(ImGui::Button("均衡档")) *frame.local_fog_action = 11;
                        ImGui::SameLine();
                        if(ImGui::Button("精细档")) *frame.local_fog_action = 12;
                        if(ImGui::Button("动画单步 (1/120s)")) *frame.local_fog_action = 13;
                    }
                    ImGui::PushItemWidth(240.0f * g_scale);
                    draw_fields(frame.globals, frame.global_count, fog_groups,
                                std::size(fog_groups), changed);
                    ImGui::PopItemWidth();
                    break;
                case Panel::Lighting:
                    ImGui::PushItemWidth(240.0f * g_scale);
                    draw_fields(frame.globals, frame.global_count, lighting_groups,
                                std::size(lighting_groups), changed);
                    ImGui::PopItemWidth();
                    break;
                case Panel::Sky:
                    ImGui::PushItemWidth(240.0f * g_scale);
                    draw_fields(frame.globals, frame.global_count, sky_groups,
                                std::size(sky_groups), changed);
                    ImGui::PopItemWidth();
                    break;
                case Panel::Inspector:
                    draw_inspector(frame, changed);
                    break;
            }

        }
        ImGui::End();
        ImGui::PopStyleVar();
    }
}
