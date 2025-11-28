#ifndef AGETEST_H_IMGUI
#define AGETEST_H_IMGUI
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include "app.h"

struct ImGUIInjector{
    MainApplication & app;
    States & s;
    MainApplicationConfig cfg;
    ImGuiIO* im_io;
    size_t im_menu { 7 };

    ImGUIInjector(MainApplication & a)
    :app(a)
    ,s(app.state)
    ,cfg(app.cfg){
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& im_io = ImGui::GetIO();
        this->im_io = &im_io;
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(a.m_window->getSystemHandle(),true);
        ImGui_ImplOpenGL3_Init("#version 150");
        im_io.FontGlobalScale = getMonitorScale() / 1.75;
        im_io.FontDefault = im_io.Fonts->AddFontFromFileTTF("./test_data/wqy-microhei.ttf");
    }

    void ui(MainApplication &);
    void draw(MainApplication &);
    
    static void camera(MainApplication &);

    /// Menu Items
    void info();
    void inspector();
    void sampler();
    void texture();
    void model();
    void gl();
    void render();
    void music();

};

#endif