#include "app.h"

using enum alib6::Severity;

auto main() -> int {
    // 读取配置
    AppConfig config;
    {
        // 临时的控制台日志
        alib6::Logger logger;
        alib6::LogFactory lg(logger, "Fog Entry");
        logger.append_mod<alib6::lot::Console>("console");

        auto entry = alib6::io::load_entry(config_file, false);
        if(entry.invalid()){
            lg(Info) << "Failed to load " << quote(config_file) << ", using default config." << std::endl;
        } else {
            lg(Info) << "Overriding configuration with " << quote(config_file) << " ..." << std::endl;
           
            alib6::AData data;
            // 读取数据
            data.load_from_entry(entry);
            //  生成Schema
            alib6::AData schema = alib6::generate_schema<AppConfig>();
            // 校验schema
            alib6::Validator validator;
            if(auto error = validator.from_adata(schema); error != "") {
                lg(Error) << "Failed to compile schema, using default config. error=" << quote(error) << std::endl;
            } else if(auto result = validator.validate(data); !result.success ) {
                // 校验数据失败
                lg(Error) << "Bad config file, using default config. " << quote(result.recorded_errors) << std::endl;
            } else {
                alib6::from_adata(config, data);
            }
        }
    }
    App app(config);
    app.setup();
    return app.run();
}

App::App(const AppConfig & cfg)
:cfg(cfg) 
,lg(logger, "Fog") { }