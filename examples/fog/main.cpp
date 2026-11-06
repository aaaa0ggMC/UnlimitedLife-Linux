/**
 * @file main.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 主程序允许
 * @version 5.0
 * @date 2026-10-14
 * 
 * @copyright Copyright (c) 2026
 * @parblock audit history (AI 别写这里)
 * 1. OK : 主要是这里也没啥好更改的，因此本身也是十分简洁，以至于 (1) 我想把 L21-24 通过 alib6.compact 
 *  给优化掉，基本上每次都要写，我就临时搞一下累不累啊，不过其实有 aout 来着🤪，咨询一下 AI (2) 数据加载那里
 *  也是老生常谈的 workflow ，对于我这个懒人十分不友好，也是严肃写一个 alib6.compact 处理
 * 2. OK 10-14 没变过
 * 3. OK 10-27 使用 acommand 而不是自己搞
 * @endparblock
 */
#include "app.h"
#include <alib5/acommand.h>
#include <algorithm>
#include <cstdlib>

using enum alib5::Severity;

namespace {
    using Command = alib5::Command;
    using CommandInput = Command::CommandInput;
    using CommandOutput = Command::CommandOutput;

    struct log_plain {
        void manipulate(alib5::LogMsgConfig & cfg) const {
            cfg.disable_extra_information = true;
        }
    };

    auto print_help(Command & cmd, alib5::LogFactory & lg) -> CommandOutput {
        lg(Info) << log_plain{} << cmd.help().color() << std::endl;
        return CommandOutput::with_code(0);
    }

    /// 路由之后还有剩余 token：多半是子命令拼错，打印帮助并返回失败
    auto usage_error(const CommandInput & in, Command & cmd, alib5::LogFactory & lg) -> CommandOutput {
        lg(Error) << "Unknown argument: " << quote(in.remains.front()) << std::endl;
        auto help = print_help(cmd, lg);
        help.code = 2;
        return help;
    }
}

auto main(int argc, const char* argv[]) -> int {
    alib5::LogFactory lg(alib5::alogger, "Fog Entry");

    Command cmd;
    cmd.add_route("radiance-check", [&](const CommandInput & in) -> CommandOutput {
        if(!in.remains.empty()) return usage_error(in, cmd, lg);
        return CommandOutput::with_code(check_radiance_gpu());
    });
    cmd.add_route("editor-check", [&](const CommandInput & in) -> CommandOutput {
        if(!in.remains.empty()) return usage_error(in, cmd, lg);
        return CommandOutput::with_code(check_editor());
    });
    cmd.add_route("help", [&](const CommandInput & in) -> CommandOutput {
        if(!in.remains.empty()) return usage_error(in, cmd, lg);
        return print_help(cmd, lg);
    });

    cmd.register_default_handler([&](const CommandInput & in) -> CommandOutput {
        if(!in.remains.empty()) return usage_error(in, cmd, lg);

        // 读取配置：生成 Schema、编译 Schema、校验、反序列化一条龙。
        // FOG_CONFIG / FOG_MASCOT_SIZE 用于一次性截帧，不改变默认配置。
        const char* configured = std::getenv("FOG_CONFIG");
        const std::string_view config_path = configured && *configured ? std::string_view(configured) : config_file;
        AppConfig config;
        if(auto result = alib5::compact::load_config_from_file(config_path, config); !result.success) {
            lg(Info) << "Failed to load " << quote(config_path) << ", using default config: " << result << std::endl;
        } else {
            lg(Info) << "Overriding configuration with " << quote(config_path) << " ..." << std::endl;
        }
        if(const char* size = std::getenv("FOG_MASCOT_SIZE"); size && *size) {
            const auto pixels = static_cast<ave::u32>(std::max(128L, std::strtol(size, nullptr, 10)));
            config.window.width = pixels;
            config.window.height = pixels;
        }

        App app(config);
        app.setup();
        return CommandOutput::with_code(app.run());
    });

    return cmd.from_args(argc, argv)[0].code;
}
