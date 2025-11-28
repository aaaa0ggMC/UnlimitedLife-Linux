#include "imgui.h"

void ImGUIInjector::ui(MainApplication &){
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowBgAlpha(s.im_winalpha);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,s.im_uialpha);
    ImGui::Begin("Controller", nullptr , ImGuiWindowFlags_MenuBar);
    if(ImGui::BeginMenuBar()){
        if(ImGui::MenuItem("信息")){
            im_menu = 7;
        }
        if(ImGui::MenuItem("检查器")){
            im_menu = 0;
        }
        if(ImGui::MenuItem("采样器")){
            im_menu = 1;
        }
        if(ImGui::MenuItem("纹理")){
            im_menu = 2;
        }
        if(ImGui::MenuItem("模型")){
            im_menu = 3;
        }
        if(ImGui::MenuItem("GL设置")){
            im_menu = 4;
        }
        if(ImGui::MenuItem("渲染设置")){
            im_menu = 5;
        }
        if(ImGui::MenuItem("音乐")){
            im_menu = 6;
        }
        ImGui::EndMenuBar();
    }
    ImGui::Text("ImGui帧率: %.2f ", im_io->Framerate);
    ImGui::Text(" 游戏帧率: %.2f" , s.fps);
    ImGui::Separator();

    switch(im_menu){
    case 0:
        inspector();
        break;
    case 1:
        sampler();
        break;
    case 2:
        texture();
        break;
    case 3:
        model();
        break;
    case 4:
        gl();
        break;
    case 5:
        render();
        break;
    case 6:
        music();
        break;
    case 7:
        info();
        break;
    }

    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::Render();
    s.im_cached = (void*) ImGui::GetDrawData();
}

void ImGUIInjector::draw(MainApplication &){
    if(s.im_cached)ImGui_ImplOpenGL3_RenderDrawData((ImDrawData*)s.im_cached);
}

void ImGUIInjector::camera(MainApplication & a){
    Window * win = a.m_window;
    static bool mouse = false;
    
    if(a.input.getKeyInfo(KeyCode::M).status == age::KeyState::ReleasedThisTick){
        static glm::vec2 lastPos = {-114514,-114514};
        mouse = !mouse;
        if(mouse){
            win->setCursorVisibility(false);
            win->setInputMode(GLFW_CURSOR,GLFW_CURSOR_DISABLED);
            win->setMouseMoveCallback([&a](Window& win,double x,double y){
                glm::vec2 curPos = {x,y};
                if(lastPos.x <= -1000){
                    // init
                    lastPos = curPos;
                }else{
                    glm::vec2 delta = curPos - lastPos;

                    if(delta.x != 0 || delta.y != 0){
                        // 孩子们，我要旋转了
                        a.camera.transform().rotate(
                            glm::angleAxis(delta.x * a.state.mouse_sensitivity / 1'000'000,glm::vec3(0,1,0)) *
                            glm::angleAxis(delta.y * a.state.mouse_sensitivity / 1'000'000,glm::vec3(1,0,0))
                        ); // 绕y轴旋转
                    }

                    lastPos = win.getCursorPos();
                }
            });
        }else{
            lastPos = {-114514,-114514};
            win->setCursorVisibility(true);
            win->setInputMode(GLFW_CURSOR,GLFW_CURSOR_NORMAL);
            if(a.imgui_ui_injector){
                win->setMouseMoveCallback([](Window&,double x,double y){
                    ImGui::GetIO().AddMousePosEvent(x,y);
                });
            }else win->setMouseMoveCallback(nullptr);
        }
    }
}