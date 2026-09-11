/**
 * @file report.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 报告输出
 * @version 5.0
 * @date 2026-09-11
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <AVE/config.h>
#include <vulkan/vulkan.h>

export module ave.profile:report;

import std;
import alib6;

export namespace ave{
    enum class RenderBuildStageId {
        instance_extensions,
        instance_layers,
        create_instance,
        debug_messenger,
        create_surface,
        select_physical_device,
        create_device,
        create_swapchain,
        create_sync_objects,
        create_render_pass,
        create_framebuffers,
        create_command_pool,
        allocate_command_buffers
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
            RenderBuildStageId::debug_messenger,
            RenderBuildStageId::create_surface,
            RenderBuildStageId::select_physical_device,
            RenderBuildStageId::create_device,
            RenderBuildStageId::create_swapchain,
            RenderBuildStageId::create_sync_objects,
            RenderBuildStageId::create_render_pass,
            RenderBuildStageId::create_framebuffers,
            RenderBuildStageId::create_command_pool,
            RenderBuildStageId::allocate_command_buffers
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
}
