#include "app.h"
#include "imgui.h"
#include <alib5/adata.h>
#include <alib5/adebug.h>

int main(){
    MainApplicationConfig cfg;
    /// 初始化配置
    {
        // 读取配置文件
        AData data;
        // 读取配置文件
        data.load_from_entry(io::load_entry("agetest.config.json"));

        // 生成schema
        auto schema = alib5::generate_schema<MainApplicationConfig>();
        // schema编译
        Validator vl;
        auto schema_error = vl.from_adata(schema);
        panicf_if(!schema_error.empty(),"[Schema Error]{}",schema_error);

        // 进行校验，忽略所有未指定字段，因为本来就有默认值
        // from_adata就是基于默认值修改出来的
        auto vl_error = vl.validate(data,true);
        panicf_if(!vl_error.success,"[Validate Error]{}",vl_error.recorded_errors);

        alib5::from_adata(cfg,data);
    }

    cfg.build_internal();
    MainApplication app (cfg);
    app.setup();
    // 需要用到window，必须在setup后面
    ImGUIInjector gui(app);
    app.imgui_draw_injector = [&gui](MainApplication & a){
        gui.draw(a);
    };
    app.imgui_ui_injector = [&gui](MainApplication & a){
        gui.ui(a);
    };
    app.imgui_camera_rot_injector = ImGUIInjector::camera;

    app.run();
    return 0;
}