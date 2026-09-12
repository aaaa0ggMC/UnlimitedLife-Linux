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

import :report;

export namespace ave{
    inline constexpr std::string_view vulkan_validation_layer_name = "VK_LAYER_KHRONOS_validation";
    
    struct AVE_API WithGlobalInput {
    private:
        friend class RenderProfile;
        // 可能存在
        Window * window;

        inline WithGlobalInput(Window * w):window(w){}
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

    struct AVE_API WithPhysicalDevices {
    private:
        friend class RenderProfile;
        // 一定存在
        std::shared_ptr<Instance> instance;
        // 可能存在
        std::shared_ptr<Surface> surface;
        // 缓存的物理设备
        bool physical_devices_enumerated { false };
        std::vector<PhysicalDeviceInfo> physical_devices {};
        std::vector<std::string> required_device_extensions {};

        inline WithPhysicalDevices(
            std::shared_ptr<Instance> i,
            std::shared_ptr<Surface> s,
            std::vector<std::string> required_extensions
        )
        :instance(std::move(i))
        ,surface(std::move(s))
        ,required_device_extensions(std::move(required_extensions)){}
    public:
        inline alib6::u32 get_physical_devices_count() noexcept {
            uint32_t physical_device_count = 0;
            vkEnumeratePhysicalDevices(
                instance->get_system_handle(),
                &physical_device_count,
                nullptr
            );
            return physical_device_count;
        }

        const std::vector<PhysicalDeviceInfo>& enumerate_physical_devices(){
            if(physical_devices_enumerated)return physical_devices;

            alib6::u32 physical_device_count = get_physical_devices_count();

            std::vector<VkPhysicalDevice> devices(physical_device_count);
            vkEnumeratePhysicalDevices(instance->get_system_handle(), &physical_device_count, devices.data());
            devices.resize(physical_device_count);

            const VkSurfaceKHR surface_handle = surface
                ? surface->get_system_handle()
                : VK_NULL_HANDLE;
            physical_devices.reserve(physical_device_count);
            for(const auto device : devices) {
                physical_devices.push_back(
                    PhysicalDeviceInfo::query(device, surface_handle)
                );
            }
            physical_devices_enumerated = true;
            return physical_devices;
        }

        [[nodiscard]] bool supports_required_extensions(
            const PhysicalDeviceInfo& info
        ) const noexcept {
            return std::ranges::all_of(
                required_device_extensions,
                [&info](const std::string& extension) {
                    return info.supports_extension(extension);
                }
            );
        }
    };

    using SelectPhysicalDevice = std::function<std::optional<std::size_t>(
        WithPhysicalDevices&
    )>;

    /// LearnVulkan2 风格的默认物理设备评分与选择。
    [[nodiscard]] inline std::optional<std::size_t>
    default_select_physical_device(WithPhysicalDevices& input) {
        constexpr double api_version_multiplier = 0.6;
        constexpr double image_dimension_2d_multiplier = 0.2;
        constexpr double discrete_gpu_multiplier = 2.0;

        const auto& devices = input.enumerate_physical_devices();
        std::optional<std::size_t> selected;
        double highest_score = 0.0;

        for(std::size_t i = 0; i < devices.size(); ++i){
            const auto gpu = devices[i].as_gpu();

            if(!gpu.geometry_shader || !gpu.support_graphics) continue;
            if(devices[i].has_surface() && !gpu.swapchain_adequate) continue;
            if(!input.supports_required_extensions(devices[i])) continue;

            const double api_score =
                (static_cast<double>(gpu.api_version.major) * 1024.0 +
                 static_cast<double>(gpu.api_version.minor)) *
                api_version_multiplier;
            const double image_score =
                static_cast<double>(gpu.max_image_dimension_2d) *
                image_dimension_2d_multiplier;

            double score = api_score + image_score;
            if(gpu.discrete) score *= discrete_gpu_multiplier;

            if(score > highest_score) {
                highest_score = score;
                selected = i;
            }
        }

        return selected;
    }

    struct AVE_API WithSelectedPhysicalDevice {
    private:
        friend class RenderProfile;
        const PhysicalDeviceInfo& info;

        explicit WithSelectedPhysicalDevice(const PhysicalDeviceInfo& value)
        :info(value){}
    public:
        [[nodiscard]] const PhysicalDeviceInfo& get_info() const noexcept {
            return info;
        }

        [[nodiscard]] GPUInfo as_gpu() const {
            return info.as_gpu();
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

        // 指定现有对象时，Surface 创建会被忽略。
        std::shared_ptr<Surface> surface { nullptr };

        // 指定现有对象时，configure_debug_messenger 会被忽略。
        std::shared_ptr<DebugMessenger> debug_messenger { nullptr };
            std::optional<CreateDebugMessengerInfo> configure_debug_messenger { std::nullopt };

        // 指定现有 Device 后，下面的物理设备选择与 Device 创建配置会被忽略。
        std::shared_ptr<Device> device { nullptr };
            // 返回 enumerate_physical_devices() 中的下标；置空时直接选择第一个设备。
            SelectPhysicalDevice select_physical_device {
                default_select_physical_device
            };
            // 仅在当前 Profile 拥有 Surface 时自动将 VK_KHR_swapchain 作为 required。
            bool add_khr_swapchain { true };
            // 是否优先尝试启用 dynamic rendering（优先 Vulkan 1.3 核心，其次 KHR 扩展，不支持则优雅降级为 LegacyRender）。
            bool try_dynamic_rendering { true };
            std::vector<std::string> required_device_extensions;
            std::vector<std::string> optional_device_extensions;
            std::function<void(
                bool is_required,
                std::vector<std::string> satisfied,
                std::vector<std::string> missing
            )> on_device_extensions_resolved { nullptr };

            // 在默认队列与扩展填充完成后、创建逻辑设备前调用。
            std::function<void(
                WithSelectedPhysicalDevice&,
                CreateDeviceInfo&
            )> configure_device { nullptr };

        // 指定现有 Swapchain 后，下面的 Swapchain 创建配置会被忽略。
        std::shared_ptr<Swapchain> swapchain { nullptr };
            // 默认函数负责从 format 到 extent 以及其余创建参数的完整配置。
            std::function<void(
                WithSwapchain&,
                CreateSwapchainInfo&
            )> configure_swapchain {
                default_configure_swapchain
            };

        // 指定现有 Images（包括显式空集合）后，configure_images 会被忽略。
        // 这些是 swapchain 之外由应用拥有的 attachment/texture images。
        std::optional<std::vector<std::shared_ptr<Image>>> images { std::nullopt };
            std::function<void(
                WithImagesInput&,
                CreateImagesInfo&
            )> configure_images {
                default_configure_images
            };

        // 指定现有 SyncObjects 后，下面的同步对象选择与创建配置会被忽略。
        std::shared_ptr<SyncObjects> sync_objects { nullptr };
            // 默认与 Swapchain image 数量一致。
            SelectSyncObjectsCount select_sync_objects_count {
                default_select_sync_objects_count
            };
            // Device 与 count 填充完成后、创建同步对象前调用。
            std::function<void(
                WithSyncObjectsInput&,
                CreateSyncObjectsInfo&
            )> configure_sync_objects { nullptr };

        // 指定现有 LegacyRender 后，下面的 RenderPass/Framebuffer 配置会被忽略。
        std::shared_ptr<LegacyRender> legacy_render { nullptr };
            std::function<void(
                WithLegacyRenderInput&,
                CreateLegacyRenderInfo&
            )> configure_legacy_render {
                default_configure_legacy_render
            };

        // 指定现有 DynamicRender 后，下面的 DynamicRender 配置会被忽略。
        std::shared_ptr<DynamicRender> dynamic_render { nullptr };
            ConfigureDynamicRender configure_dynamic_render {
                default_configure_dynamic_render
            };

        // 指定现有 CommandPool 后，下面的命令池配置会被忽略。
        std::shared_ptr<CommandPool> command_pool { nullptr };
            std::function<void(
                WithCommandPoolInput&,
                CreateCommandPoolInfo&
            )> configure_command_pool {
                default_configure_command_pool
            };

        // 指定现有 CommandBuffers 后，下面的分配配置会被忽略。
        std::shared_ptr<CommandBuffers> command_buffers { nullptr };
            // 默认数量与 SyncObjects 的 frame 数量一致。
            std::function<void(
                WithCommandBuffersInput&,
                CreateCommandBuffersInfo&
            )> configure_command_buffers {
                default_configure_command_buffers
            };

    };
};
