#include "imgui.h"
#include "renderer.h"
#include "renderer.h"

void ImGUIInjector::info(){
    ImGui::Text("信息");
    ImGui::DragFloat("窗口不透明度",&s.im_winalpha,0.008F,0.0F,1.0F);
    ImGui::DragFloat("UI不透明度 ",&s.im_uialpha,0.008F,0.2F,1.0F);
    ImGui::DragFloat("鼠标灵敏度",&s.mouse_sensitivity,1);
    ImGui::Checkbox("使用光源相机", &s.use_light_cam);
}

// 程序使用imgui的缓存
struct ImGuiCache{
    int texture_select_index = 0;
    int model_select_index = 0;
};

void ImGUIInjector::inspector(){
    static std::vector<const char *> texture_names;
    static std::vector<const char *> model_names;

    if(app.textures.size() != texture_names.size()){
        texture_names.clear();

        for(auto & [k,_] : app.textures){
            texture_names.push_back(k.c_str());
        }
    }

    if(app.models.size() != model_names.size()){
        model_names.clear();

        for(auto & [k,_] : app.models){
            model_names.push_back(k.c_str());
        }
    }


    std::string a;
    ImGui::Text("检查器");
    bool unlocked = true;

    auto check_pool = [&]<class T,class Fn>(const Entity & entity,Fn && fn) 
        requires std::invocable<Fn, T&>
    {
        static ComponentPool<T>* pool = nullptr;

        if(pool){
            auto it = pool->mapper.find(entity.id);
            if(it != pool->mapper.end()) fn(pool->data[it->second]);
        }else if(unlocked) pool = app.em.get_component_pool<T>();
    };
     
    auto get_cache = [this](Entity entity) -> ImGuiCache&{
        return app.em.add_component<ImGuiCache>(entity).get();
    };

    app.em.get_entities_storage().for_each(
    [&](Entity & entity){
        a = "Entity";

        check_pool.template operator()<Tag>(entity,
            [&](Tag & tag){
                a += " @";
                a += tag.tag.c_str();
            }
        );

        a += " #" + std::to_string(entity.id);
        
        if(ImGui::CollapsingHeader(a.c_str())){
            ImGui::PushID(entity.id);
            ImGui::Text("ID: %lu",entity.id);
            ImGui::Text("版本号: %u",entity.version);
            ImGui::Indent();
            
            /// Transform
            check_pool.template operator()<Transform>(entity,
                [&](Transform & transform){
                    if(ImGui::CollapsingHeader("Transform",ImGuiTreeNodeFlags_DefaultOpen)){
                        ImGui::DragFloat3("位置",glm::value_ptr(transform.m_position),0.1);
                        ImGui::DragFloat4("旋转",glm::value_ptr(transform.m_rotation.get_mutable_unnorm()),0.01);
                        ImGui::DragFloat3("缩放",glm::value_ptr(transform.m_scale),0.01);
                        ImGui::DragFloat3("速度",glm::value_ptr(transform.velocity),0.1);
                    }
                }
            );
            
            /// Parents
            check_pool.template operator()<Parent>(entity,
                [&](Parent & parent){
                    if(ImGui::CollapsingHeader("Parent",ImGuiTreeNodeFlags_DefaultOpen)){
                        ImGui::Text("父类 %lu %u",parent.parent.id,parent.parent.version);
                    } 
                }
            );

            /// Projector
            check_pool.template operator()<Projector>(entity,
                [&](Projector & proj){
                     if(ImGui::CollapsingHeader("Projector",ImGuiTreeNodeFlags_DefaultOpen)){
                        bool changed = false;

                        changed |= ImGui::DragFloat("FOV",&proj.fovRad,0.01);
                        changed |= ImGui::DragFloat("近裁切面",&proj.zNear,0.1);
                        changed |= ImGui::DragFloat("远裁切面",&proj.zFar,0.1);

                        if(changed){
                            proj.dm_mark();
                        }
                    }
                }
            );

            /// Object Render
            check_pool.template operator()<my_comps::ObjectRender>(entity,
                [&](my_comps::ObjectRender & render){
                    if(ImGui::CollapsingHeader("Render",ImGuiTreeNodeFlags_DefaultOpen)){
                        ImGui::Checkbox("显示", &render.visible);

                        ImGuiCache & cache = get_cache(entity);
                        /// 贴图选择
                        if(texture_names.size() && ImGui::ListBox("贴图", &cache.texture_select_index, texture_names.data(),texture_names.size())){
                            auto it = app.textures.find(texture_names[cache.texture_select_index]);
                            if(it != app.textures.end()){
                                render.texture = it->second;
                            }
                        }

                        /// 渲染模式
                        bool is_draw_array = render.mode == render.DrawArray;
                        if(ImGui::Checkbox("Draw Array Mode", &is_draw_array)){
                            render.mode = is_draw_array ? render.DrawArray : render.DrawModel;
                        }

                        if(render.mode == render.DrawModel){
                            if(model_names.size() && ImGui::ListBox("模型",&cache.model_select_index,model_names.data(),model_names.size())){
                                auto it = app.models.find(model_names[cache.model_select_index]);
                                if(it != app.models.end()){
                                    render.model.model = &(it->second);
                                }
                            }
                        }else if(render.mode == render.DrawArray){
                            
                        }
                    }
                }
            );


            ImGui::Unindent();
            ImGui::PopID();

            /// 单次只查询一次池子，多查询没必要
            unlocked = false;
        }
    });
}

void ImGUIInjector::sampler(){
    static const char * im_wrapItems[] = {"Repeat","MirroredRepeat","ClampToEdge","ClampToBorder"};
    static auto wrapFn = [](int id)->SamplerInfo::WrapMethod {
        switch(id){
        case 1:
            return SamplerInfo::WrapMethod::MirroredRepeat;
        case 2:
            return SamplerInfo::WrapMethod::ClampToEdge;
        case 3:
            return SamplerInfo::WrapMethod::ClampToBorder;
        default:
            return SamplerInfo::WrapMethod::Repeat;
        }
    };
    static float max_anisotropy = Queryer().anisotropicFiltering().second;

    ImGui::Text("采样器设置");
    ImGui::DragFloat4("边框颜色",glm::value_ptr(s.sampler_border_color),0.01F,0.0F,1.0F);
    if(ImGui::CollapsingHeader("环绕设置",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::Indent();
        ImGui::ListBox("R(2D贴图没用)",&s.sampler_wrap_r,im_wrapItems,4);
        ImGui::ListBox("S",&s.sampler_wrap_s,im_wrapItems,4);
        ImGui::ListBox("T",&s.sampler_wrap_t,im_wrapItems,4);
        ImGui::Unindent();
    }
    if(max_anisotropy)ImGui::DragFloat("各向异性过滤",&s.sampler_aniso,0.01F,0.0F,max_anisotropy);
    app.m_sampler.wrapR(wrapFn(s.sampler_wrap_r)).wrapS(wrapFn(s.sampler_wrap_s)).wrapT(wrapFn(s.sampler_wrap_t));
    if(max_anisotropy)app.m_sampler.try_anisotropy(s.sampler_aniso);
    app.m_sampler.borderColor(s.sampler_border_color);
}

void ImGUIInjector::shadow(){
    ImGui::Text("帧缓冲区设置");
    ImGui::Image((ImTextureID)(app.shadowTexCallback->getId()),ImVec2(256,256));
}

void ImGUIInjector::gl(){
    ImGui::Text("GL设置");
    if(ImGui::CollapsingHeader("多边形模式",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::ListBox("面",&s.gl_polygon_face_index,gl_polygon_face_desc.data(),gl_polygon_face_desc.size());
        ImGui::ListBox("模式",&s.gl_polygon_mode_index,gl_polygon_mode_desc.data(),gl_polygon_mode_desc.size());
    }
    ImGui::Checkbox("面剔除",&s.gl_cull);
    ImGui::Checkbox("深度测试",&s.gl_depth);
    ImGui::ListBox("深度测试函数",&s.gl_depthfunc_index,gl_depthfunc_desc.data(),gl_depthfunc_desc.size(),4);
    ImGui::DragFloat("点大小",&s.point_size,0.1F,0.1F,64.0F);
}

void ImGUIInjector::music(){
    ImGui::Text("音乐:");
    if(app.snd1.getStatus() == age::audio::Status::Stopped){
        if(ImGui::Button("播放")){
            app.snd1.play();
        }
    }else if(app.snd1.getStatus() == age::audio::Status::Playing){
        if(ImGui::Button("暂停")){
            app.snd1.pause();
        }
    }else if(app.snd1.getStatus() == age::audio::Status::Paused){
        if(ImGui::Button("继续")){
            app.snd1.play();
        }
    }
    size_t len = app.snd1.length().count();
    size_t prog = app.snd1.tell().count();
    prog %= len;

    if(ImGui::SliderFloat("进度调整",&s.progress,0.0,1.0)){
        app.snd1.seek(std::chrono::milliseconds((int)(s.progress * len)));
    }else s.progress = (float)prog / len;
    ImGui::Text("进度: %.1f / %.1f",prog/1000.f, len/1000.f);
    ImGui::Text("状态: %s",audio::getStatusText(app.snd1.getStatus()));
}