#include <AVE/config.h>
#include <vulkan/vulkan.h>
#include <AVE/alib.h>
#include <AVE/error_code.h>
#include <AVE/render/descriptor_config.h>
#include <alib5/autil.h>



namespace ave {

namespace {

std::string ascii_lower(std::string_view value) {
    std::string result(value);
    for(auto& ch : result) {
        if(ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return result;
}

std::optional<DescriptorKind> kind_from_name(std::string_view name) {
    const auto lower = ascii_lower(name);
    if(lower == "ubo") return DescriptorKind::ubo;
    if(lower == "dynamic_ubo") return DescriptorKind::dynamic_ubo;
    if(lower == "ssbo") return DescriptorKind::ssbo;
    if(lower == "dynamic_ssbo") return DescriptorKind::dynamic_ssbo;
    if(lower == "sampler" || lower == "combined_image_sampler") return DescriptorKind::sampler;
    if(lower == "image" || lower == "storage_image") return DescriptorKind::image;
    return std::nullopt;
}

std::string_view kind_name(DescriptorKind kind) {
    switch(kind) {
        case DescriptorKind::ubo: return "ubo";
        case DescriptorKind::dynamic_ubo: return "dynamic_ubo";
        case DescriptorKind::ssbo: return "ssbo";
        case DescriptorKind::dynamic_ssbo: return "dynamic_ssbo";
        case DescriptorKind::sampler: return "sampler";
        case DescriptorKind::image: return "image";
    }
    return "ubo";
}

std::string_view stage_name(ShaderStage stage) {
    switch(stage) {
        case ShaderStage::vertex: return "vertex";
        case ShaderStage::tessellation_control: return "tessellation_control";
        case ShaderStage::tessellation_evaluation: return "tessellation_evaluation";
        case ShaderStage::geometry: return "geometry";
        case ShaderStage::fragment: return "fragment";
        case ShaderStage::compute: return "compute";
        case ShaderStage::all_graphics: return "all_graphics";
        case ShaderStage::all: return "all";
    }
    return "all";
}

std::optional<ShaderStage> stage_from_name(std::string_view name) {
    const auto lower = ascii_lower(name);
    if(lower == "vertex") return ShaderStage::vertex;
    if(lower == "tessellation_control") return ShaderStage::tessellation_control;
    if(lower == "tessellation_evaluation") return ShaderStage::tessellation_evaluation;
    if(lower == "geometry") return ShaderStage::geometry;
    if(lower == "fragment") return ShaderStage::fragment;
    if(lower == "compute") return ShaderStage::compute;
    if(lower == "all_graphics") return ShaderStage::all_graphics;
    if(lower == "all") return ShaderStage::all;
    return std::nullopt;
}

std::optional<VkShaderStageFlags> stage_flags_from_name(std::string_view text) {
    VkShaderStageFlags flags = 0;
    while(!text.empty()) {
        const auto split = text.find_first_of("|+, \t");
        const auto token = text.substr(0, split);
        if(!token.empty()) {
            const auto parsed = stage_from_name(token);
            if(!parsed) return std::nullopt;
            flags |= to_vk_shader_stages(*parsed);
        }
        if(split == std::string_view::npos) break;
        text.remove_prefix(split + 1);
    }
    return flags == 0 ? std::nullopt : std::optional<VkShaderStageFlags>(flags);
}

bool is_string(const alib5::AData& node) {
    return node.is_value() && node.value().get_type() == alib5::CacheValue::STRING;
}

} // namespace

VkDescriptorType to_vk_descriptor_type(DescriptorKind kind) noexcept {
    switch(kind) {
        case DescriptorKind::ubo: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorKind::dynamic_ubo: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        case DescriptorKind::ssbo: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorKind::dynamic_ssbo: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        case DescriptorKind::sampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case DescriptorKind::image: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    }
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

VkShaderStageFlags to_vk_shader_stages(ShaderStage stage) noexcept {
    switch(stage) {
        case ShaderStage::vertex: return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::tessellation_control: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case ShaderStage::tessellation_evaluation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case ShaderStage::geometry: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::compute: return VK_SHADER_STAGE_COMPUTE_BIT;
        case ShaderStage::all_graphics: return VK_SHADER_STAGE_ALL_GRAPHICS;
        case ShaderStage::all: return VK_SHADER_STAGE_ALL;
    }
    return VK_SHADER_STAGE_ALL;
}

DescriptorBinding to_descriptor_binding(const DescriptorBindingSpec& spec, ShaderStage default_stage) {
    DescriptorBinding result;
    result.binding = spec.binding;
    result.descriptor_type = to_vk_descriptor_type(spec.type);
    result.descriptor_count = spec.count == 0 ? 1u : spec.count;
    result.stage_flags = spec.stage_flags
        ? *spec.stage_flags
        : to_vk_shader_stages(spec.stage.value_or(default_stage));
    return result;
}

std::vector<DescriptorBinding> parse_descriptor_bindings(
    std::string_view json,
    ShaderStage default_stage,
    ave::ErrorWrapper ew
) {
    alib5::AData document;
    if(!alib5::data::JSON{}.parse(json, document)) {
        ew.report(ave_vk_create_pipeline_layout,
            "Descriptor layout JSON could not be parsed.");
        return {};
    }

    ShaderStage layout_stage = default_stage;
    const alib5::AData* entries = &document;
    if(document.is_object()) {
        if(document.object().contains("stage")) {
            const auto& stage_node = document.object()["stage"];
            if(!is_string(stage_node)) {
                ew.report(ave_vk_create_pipeline_layout,
                    "Descriptor layout 'stage' must be a string.");
                return {};
            }
            const auto parsed = stage_from_name(stage_node.to<std::string_view>());
            if(!parsed) {
                ew.report(ave_vk_create_pipeline_layout,
                    "Unknown descriptor layout stage '{}'.",
                    stage_node.to<std::string_view>());
                return {};
            }
            layout_stage = *parsed;
        }
        if(!document.object().contains("bindings") || !document.object()["bindings"].is_array()) {
            ew.report(ave_vk_create_pipeline_layout,
                "Descriptor layout JSON requires a 'bindings' array.");
            return {};
        }
        entries = &document.object()["bindings"];
    }

    if(!entries->is_array()) {
        ew.report(ave_vk_create_pipeline_layout,
            "Descriptor layout JSON root must be an array or an object with 'bindings'.");
        return {};
    }

    std::vector<DescriptorBinding> result;
    result.reserve(entries->array().size());
    ave::u32 index = 0;
    for(const auto& entry : entries->array()) {
        DescriptorBindingSpec spec;
        if(entry.is_object()) {
            // 反射路径：{"type":"ssbo","binding":3,"stage":"compute","count":1}
            alib5::AData normalized(entry);
            if(!normalized.object().contains("type") || !normalized.object().contains("binding")) {
                ew.report(ave_vk_create_pipeline_layout,
                    "Descriptor binding #{} object requires 'type' and 'binding'.", index);
                return {};
            }
            // 先自己校验并规范成枚举标识符，别名/大小写错误都经 ew 上报，而不是反射层 panic。
            const auto& type_node = normalized.object()["type"];
            const auto kind = is_string(type_node)
                ? kind_from_name(type_node.to<std::string_view>())
                : std::nullopt;
            if(!kind) {
                ew.report(ave_vk_create_pipeline_layout,
                    "Descriptor binding #{}: unknown type.", index);
                return {};
            }
            normalized.object()["type"] = kind_name(*kind);
            if(normalized.object().contains("stage")) {
                const auto& stage_node = normalized.object()["stage"];
                const auto parsed = is_string(stage_node)
                    ? stage_from_name(stage_node.to<std::string_view>())
                    : std::nullopt;
                if(!parsed) {
                    ew.report(ave_vk_create_pipeline_layout,
                        "Descriptor binding #{}: unknown stage.", index);
                    return {};
                }
                normalized.object()["stage"] = stage_name(*parsed);
            }
            alib5::from_adata(spec, normalized);
            result.push_back(to_descriptor_binding(spec, layout_stage));
            ++index;
            continue;
        }
        if(!entry.is_array()) {
            ew.report(ave_vk_create_pipeline_layout,
                "Descriptor binding #{} must be an array or an object.", index);
            return {};
        }
        const auto& items = entry.array();
        if(items.size() < 2 || items.size() > 4) {
            ew.report(ave_vk_create_pipeline_layout,
                "Descriptor binding #{} expects [type, binding?, stage?, count?].", index);
            return {};
        }
        if(!is_string(items[0])) {
            ew.report(ave_vk_create_pipeline_layout,
                "Descriptor binding #{}: type must be a string.", index);
            return {};
        }
        const auto kind = kind_from_name(items[0].to<std::string_view>());
        if(!kind) {
            ew.report(ave_vk_create_pipeline_layout,
                "Descriptor binding #{}: unknown type '{}'.",
                index, items[0].to<std::string_view>());
            return {};
        }
        spec.type = *kind;
        auto apply_stage = [&](const alib5::AData& node) {
            if(node.is_array()) {
                VkShaderStageFlags flags = 0;
                for(const auto& stage_item : node.array()) {
                    if(!is_string(stage_item)) return false;
                    const auto parsed = stage_from_name(stage_item.to<std::string_view>());
                    if(!parsed) return false;
                    flags |= to_vk_shader_stages(*parsed);
                }
                if(flags == 0) return false;
                spec.stage_flags = flags;
                return true;
            }
            if(!is_string(node)) return false;
            const auto parsed = stage_flags_from_name(node.to<std::string_view>());
            if(!parsed) return false;
            spec.stage_flags = *parsed;
            return true;
        };
        if(is_string(items[1])) {
            // 第二项是 stage 时，binding 取数组下标：["ssbo","compute"]
            spec.binding = index;
            if(!apply_stage(items[1])) {
                ew.report(ave_vk_create_pipeline_layout,
                    "Descriptor binding #{}: unknown stage '{}'.",
                    index, items[1].to<std::string_view>());
                return {};
            }
            if(items.size() >= 3) spec.count = items[2].to<ave::u32>();
        } else {
            spec.binding = items[1].to<ave::u32>();
            if(items.size() >= 3 && !items[2].is_null()) {
                if(!apply_stage(items[2])) {
                    ew.report(ave_vk_create_pipeline_layout,
                        "Descriptor binding #{}: unknown stage.", index);
                    return {};
                }
            }
            if(items.size() >= 4) spec.count = items[3].to<ave::u32>();
        }
        result.push_back(to_descriptor_binding(spec, layout_stage));
        ++index;
    }
    return result;
}

std::vector<DescriptorBinding> load_descriptor_bindings(
    std::string_view path,
    ShaderStage default_stage,
    ave::ErrorWrapper ew
) {
    const std::pmr::string content = alib5::io::read_all(path);
    if(content.empty()) {
        ew.report(ave_vk_create_pipeline_layout,
            "Cannot read descriptor layout file '{}'.", path);
        return {};
    }
    return parse_descriptor_bindings(content, default_stage, ew);
}

}
