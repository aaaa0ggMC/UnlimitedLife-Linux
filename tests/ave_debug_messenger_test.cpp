#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <AVE/AVE.h>
#include <AVE/alib.h>


static void check(bool value, const char* message) {
    if(!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

///// 内核：on_message_advanced 直连 /////
static void test_kernel() {
    ave::CreateDebugMessengerInfo info;
    check(!info.on_message_advanced, "kernel callback starts empty");

    int calls = 0;
    VkDebugUtilsMessageTypeFlagsEXT last_type = 0;
    info.on_message_advanced = [&](auto severity, auto type, const auto& data) {
        ++calls;
        last_type = type;
        return (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0;
    };

    VkDebugUtilsMessengerCallbackDataEXT data {};
    data.pMessage = "raw message";
    check(info.on_message_advanced(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                                   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, data) == false,
          "kernel must return the callback's bool (warning -> no abort)");
    check(info.on_message_advanced(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                   VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, data) == true,
          "kernel must return the callback's bool (error -> abort)");
    check(calls == 2 && last_type == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
          "kernel receives every message with the raw type");
}

///// 包装：on_message 把简易回调翻译成内核回调 /////
static void test_simple_wrapper() {
    const struct {
        VkDebugUtilsMessageSeverityFlagBitsEXT severity;
        VkDebugUtilsMessageTypeFlagsEXT type;
        alib5::LogLevel level;
        std::string_view type_name;
    } cases[] = {
        { VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
          alib5::LogLevel::Error, "Validation" },
        { VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
          alib5::LogLevel::Warn, "Performance" },
        { VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
          alib5::LogLevel::Info, "General" },
        { VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
          alib5::LogLevel::Debug, "General" },
    };

    for(const auto& c : cases) {
        ave::CreateDebugMessengerInfo info;
        alib5::LogLevel seen_level = alib5::LogLevel::Debug;
        std::string seen_type = "<none>";
        std::string seen_message = "<none>";
        info.on_message([&](alib5::LogLevel level, std::string_view type, std::string_view message) {
            seen_level = level;
            seen_type = type;
            seen_message = message;
        });

        check(bool(info.on_message_advanced), "on_message must fill the kernel callback");
        VkDebugUtilsMessengerCallbackDataEXT data {};
        data.pMessage = "VUID-test placeholder";
        const bool abort = info.on_message_advanced(c.severity, c.type, data);
        check(!abort, "void-returning simple callback must not abort the Vulkan call");
        check(seen_level == c.level, "severity must map onto the expected log level");
        check(seen_type == c.type_name, "message type must map onto the expected name");
        check(seen_message == "VUID-test placeholder", "message text must pass through unchanged");
    }

    // 返回 bool 的简易回调按返回值交给 Vulkan
    {
        ave::CreateDebugMessengerInfo info;
        info.on_message([](alib5::LogLevel level, std::string_view, std::string_view) {
            return level == alib5::LogLevel::Error;
        });
        VkDebugUtilsMessengerCallbackDataEXT data {};
        data.pMessage = "boom";
        check(info.on_message_advanced(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, data) == true,
              "bool-returning simple callback must reach Vulkan (error -> abort)");
        check(info.on_message_advanced(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, data) == false,
              "bool-returning simple callback must reach Vulkan (warning -> no abort)");
    }

    // pMessage 为空时给空串，而不是空指针
    {
        ave::CreateDebugMessengerInfo info;
        std::string seen_message = "<none>";
        info.on_message([&](alib5::LogLevel, std::string_view, std::string_view message) {
            seen_message = message;
        });
        VkDebugUtilsMessengerCallbackDataEXT data {};
        data.pMessage = nullptr;
        info.on_message_advanced(VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, data);
        check(seen_message.empty(), "null pMessage must become an empty string");
    }

    // 后设置的简易回调覆盖之前的写法，返回引用可以继续链式配置
    {
        ave::CreateDebugMessengerInfo info;
        int calls = 0;
        info.on_message_advanced = [&](auto, auto, const auto&) { ++calls; return false; };
        info.on_message([&](alib5::LogLevel, std::string_view, std::string_view) { ++calls; });
        VkDebugUtilsMessengerCallbackDataEXT data {};
        data.pMessage = "x";
        info.on_message_advanced(VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, data);
        check(calls == 1, "on_message must replace the previous kernel callback");

        auto& same = info.on_message([](alib5::LogLevel, std::string_view, std::string_view) {});
        check(&same == &info, "on_message must return the config for chaining");
    }
}

int main() {
    test_kernel();
    test_simple_wrapper();

    std::printf("ALL AVE DEBUG MESSENGER TESTS PASSED\n");
    return 0;
}
