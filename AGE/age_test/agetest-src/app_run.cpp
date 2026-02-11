#include "app.h"

void MainApplication::run(){
    Window & win = *m_window;
    Sampler & sampler = m_sampler;
    //// Clocks ////
    Clock fps_counter (false);
    Clock im_clock (false);
    Clock elapse (false);
    Clock world_updater (false);

    Trigger im_trigger (im_clock,1000 / cfg.imgui_refresh_rate);
    Trigger world_trigger (world_updater,1000 / cfg.world_update_frame_rate);

    uint64_t frame_count {0};
    // play sounds
    snd1.play();
    lg(Info) << "Entering main loop..." << endlog;

    /// 预先更新，使得物体到正确的位置
    parent_system.update();

    // launch clocks
    fps_counter.start();
    im_clock.start();
    elapse.start();
    world_updater.start();
    while(!win.shouldClose()){
        //// Counting Framerate ////
        ++frame_count;
        if(fps_counter.get_offset() >= cfg.fpsCountTimeMs){
            fps_counter.clear_offset();
            state.fps = (int)(frame_count / cfg.fpsCountTimeMs * 1000);
            frame_count = 0;
        }
        //// Poll Events ////
        win.pollEvents();

        //// ImGUI UI event ////
        if(imgui_ui_injector && (im_trigger.test(true) || !state.im_cached)){
            imgui_ui_injector(*this);
        }

        //// Input System && Update Worlds ////
        input.update();
        if(input.checkTick()){
            float elapse_seconds = elapse.get_offset() / 1000.f;
            handle_input(elapse_seconds);
            elapse.clear_offset();
        }
        if(world_trigger.test()){
            world_update(world_trigger.duration);
        }

        draw();
        if(imgui_draw_injector)imgui_draw_injector(*this);
        win.display();
    }
}

void MainApplication::world_update(float ep){
    if(state.playing){
        marker.update();
        //post-input-update
        cube.transform().rotate(glm::vec3(1.0f,0.0f,0.0f),0.001 * ep);
        invPar.transform().rotate(glm::vec3(0.0f,1.0f,1.0f),0.002 * ep);
        pyramid.transform().rotate(glm::vec3(0.0f,1.0f,0.0f),0.0015 * ep); 
    }

    //// Update Systems ////
    /// 更新也是有顺序的
    em.update<comps::Transform>(ep,true);
    parent_system.update();
}

void MainApplication::handle_input(float p){
    glm::vec3 veloDir = glm::vec3(0,0,0);
    if(input.getKeyInfo(KeyCode::W).isPressing()){
        veloDir.z -= 1;
    }
    if(input.getKeyInfo(KeyCode::S).isPressing()){
        veloDir.z += 1;
    }

    if(input.getKeyInfo(KeyCode::A).isPressing()){
        veloDir.x -= 1;
    }
    if(input.getKeyInfo(KeyCode::D).isPressing()){
        veloDir.x += 1;
    }

    if(input.getKeyInfo(KeyCode::Space).isPressing()){
        veloDir.y += 1;
    }
    if(input.getKeyInfo(KeyCode::LeftShift).isPressing()){
        veloDir.y -= 1;
    }


    Camera & cam = state.use_light_cam?e_light:camera;
    if(input.getKeyInfo(KeyCode::Left).isPressing()){
        cam.transform().rotate(glm::vec3(0,1,0),-cfg.cam_rot.x * p);
    }else if(input.getKeyInfo(KeyCode::Right).isPressing()){
        cam.transform().rotate(glm::vec3(0,1,0),cfg.cam_rot.x * p);
    }

    if(input.getKeyInfo(KeyCode::Up).isPressing()){
        cam.transform().rotate(glm::vec3(1,0,0),-cfg.cam_rot.y * p);
    }else if(input.getKeyInfo(KeyCode::Down).isPressing()){
        cam.transform().rotate(glm::vec3(1,0,0),cfg.cam_rot.y * p);
    }

    cam.transform().buildVelocity(veloDir,cfg.cam_speed);

    if(imgui_camera_rot_injector)imgui_camera_rot_injector(*this);
}