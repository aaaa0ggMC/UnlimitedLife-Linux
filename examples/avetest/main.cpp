import alib6;
import std;
import ave;

auto main() -> int {
    try{
        alib6::log::Logger logger;
        alib6::log::LogFactory lg(logger,"avetest");
        logger.append_mod<alib6::log::Console>("console");

        ave::Context context;
        ave::Window window({
            .ctx = context,
            .title = "Hello from AVE!",
            .width = 1920,
            .height = 1080,
        });

        while(!window.should_close()){
            window.poll_events();

            alib6::Timer(1).wait();
        }
        
    }catch(...){
        // 已经有panic了，也是直接忽略
        return 1;
    }
    return 0;
}