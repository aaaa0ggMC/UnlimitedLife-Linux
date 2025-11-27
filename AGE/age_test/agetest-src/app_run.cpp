#include "app.h"

void MainApplication::run(){
    Window & win = *m_window;
    Sampler & sampler = *m_sampler;
    Clock fpsCounter (false);
    uint64_t frame_count {0};
    // play sounds
    snd1.play();
    lg(Info) << "Entering main loop..." << endlog;

    // launch clocks
    fpsCounter.start();
    while(!win.shouldClose()){
        ++frame_count;
        if(fpsCounter.getOffset() >= cfg.fpsCountTimeMs){
            fpsCounter.clearOffset();
            state.fps = (int)(frame_count / cfg.fpsCountTimeMs * 1000);
            frame_count = 0;
        }
        win.pollEvents();
    }
}