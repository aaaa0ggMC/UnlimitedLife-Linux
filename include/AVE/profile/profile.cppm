/**
 * @file instance.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Instance的一些配置选项
 * @version 5.0
 * @date 2026-09-10
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.profile:profile;

import ave.window;
import ave.render;
import alib6;
import std;

export namespace ave{
    inline constexpr std::string_view vulkan_validation_layer_name = "VK_LAYER_KHRONOS_validation";

    enum class RenderBuildStageId {
        instance_extensions,
        instance_layers,
        create_instance,
        debug_messenger,
        // 预留后续渲染阶段
        create_surface,
        select_physical_device,
        create_device,
        create_swapchain
    };

    enum class RenderBuildStageStatus {
        not_run,
        succeeded,
        partial,
        failed,
        skipped
    };

    struct AVE_API RenderBuildStage {
        RenderBuildStageId id { RenderBuildStageId::instance_extensions };
        RenderBuildStageStatus status { RenderBuildStageStatus::not_run };
        alib6::AData content;

        RenderBuildStage() = default;
        explicit RenderBuildStage(RenderBuildStageId stage_id)
        :id(stage_id){}

        inline RenderBuildStage& set_status(RenderBuildStageStatus s) noexcept {
            status = s;
            return *this;
        }

        inline RenderBuildStage& succeed() noexcept {
            status = RenderBuildStageStatus::succeeded;
            return *this;
        }

        inline RenderBuildStage& fail(std::string_view reason = {}) {
            status = RenderBuildStageStatus::failed;
            if(!reason.empty()) content["error"] = reason;
            return *this;
        }

        inline RenderBuildStage& skip(std::string_view reason = {}) {
            status = RenderBuildStageStatus::skipped;
            if(!reason.empty()) content["reason"] = reason;
            return *this;
        }

        inline RenderBuildStage& partial(std::string_view reason = {}) {
            status = RenderBuildStageStatus::partial;
            if(!reason.empty()) content["note"] = reason;
            return *this;
        }

        inline alib6::AData& operator[](std::string_view key) {
            return content[key];
        }

        inline const alib6::AData& operator[](std::string_view key) const {
            return content[key];
        }

        [[nodiscard]] inline bool succeeded() const noexcept {
            return status == RenderBuildStageStatus::succeeded;
        }

        [[nodiscard]] inline bool failed() const noexcept {
            return status == RenderBuildStageStatus::failed;
        }

        [[nodiscard]] inline bool skipped() const noexcept {
            return status == RenderBuildStageStatus::skipped;
        }

        [[nodiscard]] inline bool partial() const noexcept {
            return status == RenderBuildStageStatus::partial;
        }
    };

    struct AVE_API RenderBuildReport {
        using StageId = RenderBuildStageId;
        using StageStatus = RenderBuildStageStatus;

        inline static constexpr std::array stage_order {
            RenderBuildStageId::instance_extensions,
            RenderBuildStageId::instance_layers,
            RenderBuildStageId::create_instance,
            RenderBuildStageId::debug_messenger
        };

        RenderBuildStageStatus status { RenderBuildStageStatus::not_run };
        std::vector<RenderBuildStage> stages;

        RenderBuildReport(){ reset(); }

        void reset(){
            status = RenderBuildStageStatus::not_run;
            stages.clear();
            stages.reserve(stage_order.size());
            for(const auto id : stage_order){
                stages.emplace_back(id);
            }
        }

        [[nodiscard]] RenderBuildStage* find(RenderBuildStageId id) noexcept {
            const auto it = std::ranges::find(stages, id, &RenderBuildStage::id);
            return it == stages.end() ? nullptr : std::addressof(*it);
        }

        [[nodiscard]] const RenderBuildStage* find(RenderBuildStageId id) const noexcept {
            const auto it = std::ranges::find(stages, id, &RenderBuildStage::id);
            return it == stages.end() ? nullptr : std::addressof(*it);
        }

        RenderBuildStage& stage(RenderBuildStageId id){
            return *find(id);
        }

        const RenderBuildStage& stage(RenderBuildStageId id) const {
            return *find(id);
        }

        inline RenderBuildStage& operator[](RenderBuildStageId id) {
            return stage(id);
        }

        inline const RenderBuildStage& operator[](RenderBuildStageId id) const {
            return stage(id);
        }

        inline RenderBuildStage& succeed(RenderBuildStageId id) {
            auto& s = stage(id);
            s.succeed();
            return s;
        }

        inline RenderBuildStage& fail(RenderBuildStageId id, std::string_view reason = {}) {
            auto& s = stage(id);
            s.fail(reason);
            return s;
        }

        inline RenderBuildStage& skip(RenderBuildStageId id, std::string_view reason = {}) {
            auto& s = stage(id);
            s.skip(reason);
            return s;
        }

        inline RenderBuildStage& partial(RenderBuildStageId id, std::string_view reason = {}) {
            auto& s = stage(id);
            s.partial(reason);
            return s;
        }

        inline RenderBuildStage& set_status(RenderBuildStageId id, RenderBuildStageStatus s) {
            auto& st = stage(id);
            st.set_status(s);
            return st;
        }

        inline void skip(std::initializer_list<RenderBuildStageId> ids, std::string_view reason = {}) {
            for(const auto id : ids){
                skip(id, reason);
            }
        }

        inline RenderBuildStage* skip_if(bool condition, RenderBuildStageId id, std::string_view reason = {}) {
            if(condition){
                return &skip(id, reason);
            }
            return nullptr;
        }

        void finish(){
            const bool has_failure = std::ranges::any_of(
                stages,
                [](const RenderBuildStage& stage){
                    return stage.status == RenderBuildStageStatus::failed;
                }
            );
            status = has_failure
                ? RenderBuildStageStatus::failed
                : RenderBuildStageStatus::succeeded;
        }

        [[nodiscard]] bool succeeded() const noexcept {
            return status == RenderBuildStageStatus::succeeded;
        }

        [[nodiscard]] bool failed() const noexcept {
            return status == RenderBuildStageStatus::failed;
        }

        [[nodiscard]] RenderBuildStageStatus stage_status(
            RenderBuildStageId id
        ) const noexcept {
            const auto * found = find(id);
            return found
                ? found->status
                : RenderBuildStageStatus::not_run;
        }
    };

    using StageId = RenderBuildStageId;
    using StageStatus = RenderBuildStageStatus;
    
    struct AVE_API WithGlobalInput {
    private:
        friend class RenderProfile;
        Window * window;

        WithGlobalInput(Window * w):window(w){}
    public:
        inline std::vector<std::string> get_required_extensions(){
            if(window){
                return
                    GLFWManager::get_required_instance_extensions() |
                    std::views::transform([](const char * p){
                        return std::string(p);
                    }) |
                    std::ranges::to<std::vector>();
            }
            return {};
        }

        inline std::vector<ExtensionProperties> enumerate_extension_properties(){
            uint32_t extensionCount = 0;
            std::vector<VkExtensionProperties> extensions;
            VkResult result = VK_SUCCESS;

            do {
                result = vkEnumerateInstanceExtensionProperties(
                    nullptr,
                    &extensionCount,
                    nullptr
                );
                if (result != VK_SUCCESS) return {};

                extensions.resize(extensionCount);
                result = vkEnumerateInstanceExtensionProperties(
                    nullptr,
                    &extensionCount,
                    extensions.data()
                );
            } while (result == VK_INCOMPLETE);

            if (result != VK_SUCCESS) return {};
            extensions.resize(extensionCount);

            return extensions
                | std::views::transform([](const VkExtensionProperties & prop){
                    return ExtensionProperties(prop);
                })
                | std::ranges::to<std::vector>();
        }

        inline std::vector<LayerProperties> enumerate_layer_properties(){
            uint32_t layerCount = 0;
            std::vector<VkLayerProperties> availableLayers;
            VkResult result = VK_SUCCESS;

            do {
                result = vkEnumerateInstanceLayerProperties(
                    &layerCount,
                    nullptr
                );
                if (result != VK_SUCCESS) return {};

                availableLayers.resize(layerCount);
                result = vkEnumerateInstanceLayerProperties(
                    &layerCount,
                    availableLayers.data()
                );
            } while (result == VK_INCOMPLETE);

            if (result != VK_SUCCESS) return {};
            availableLayers.resize(layerCount);

            return availableLayers
                | std::views::transform([](const VkLayerProperties & prop){
                    return LayerProperties(prop);
                })
                | std::ranges::to<std::vector>();
        }
    };


    /// 按顺序会依次执行，配置实行覆盖，其中若制定了instance之类的一些配置会被忽略
    struct AVE_API ProfileWith {
        std::shared_ptr<Instance> instance { nullptr };
            // 指定instance后被忽略，用configure_instance设置名字等基础信息
            std::function<void(WithGlobalInput &, CreateInstanceInfo & ci)> configure_instance { nullptr };
            // 扩展
            std::vector<std::string> required_extensions;
            std::vector<std::string> optional_extensions;
            // 每个解析阶段执行一次
            std::function<void(
                bool is_required,
                std::vector<std::string> satisfied,
                std::vector<std::string> missing
            )> on_extensions_resolved { nullptr };

            // 中间层
            bool validation_layer { true };
            std::vector<std::string> required_layers;
            std::vector<std::string> optional_layers;
            // 每个解析阶段执行一次
            std::function<void(
                bool is_required,
                std::vector<std::string> satisfied,
                std::vector<std::string> missing
            )> on_layers_resolved { nullptr };

        // 指定现有对象时，configure_debug_messenger 会被忽略。
        std::shared_ptr<DebugMessenger> debug_messenger;
            std::optional<CreateDebugMessengerInfo> configure_debug_messenger { std::nullopt };
    };
};
