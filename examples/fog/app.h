import ave;
import std;
import alib6;

using namespace alib6::attr;

constexpr std::string_view config_file = "config.fog.json";
/// 应用配置
struct AppConfig {  
    struct Window {
        alib6::u32 width = 800;
        alib6::u32 height = 600;
    };

    [[=schema::optional()]]
    Window window;
};

struct App {
    AppConfig cfg;

    //// 基础 Infra ////
    /// 日志
    alib6::Logger logger;
    alib6::LogFactory lg;

    //// Vulkan 部分 ////
    /// 一些共用的内存组件
    ave::Context context;
    /// 程序窗口
    std::optional<ave::Window> window;

    App(const AppConfig & cfg);
    void setup();
    int run();

private:
    void setup_create_window();
};


//// 提供日志便利 ////
constexpr auto lc_quote = alib6::lot::color(
    alib6::lot::Color::Cyan
); 
constexpr auto lc_reset = alib6::lot::color(
    alib6::lot::Color::None
);

/// 引用
template<class T>
struct quote {
    T t;

    template<class R>
    quote(R && s):t(std::forward<R>(s)){}

    template<class Stream>
    Stream&& self_forward(Stream && ctx){
        std::move(ctx) << lc_quote << t << lc_reset;
        return std::move(ctx);
    }
};
template<class T> quote(T&&) -> quote<T>;

/// 扁平化输出
template<class T>
auto flat(T && value) {
    return alib6::to_adata(std::forward<T>(value)).str(
        alib6::data::Flat()
    );
}