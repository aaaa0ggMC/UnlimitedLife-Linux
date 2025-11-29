#include "imgui.h"

void ImGUIInjector::info(){
    ImGui::Text("信息");
    ImGui::DragFloat("窗口不透明度",&s.im_winalpha,0.008F,0.0F,1.0F);
    ImGui::DragFloat("UI不透明度 ",&s.im_uialpha,0.008F,0.2F,1.0F);
    ImGui::DragFloat("鼠标灵敏度",&s.mouse_sensitivity,1);
}

void ImGUIInjector::inspector(){
    static std::vector<Entity> valid_entities;
    static ComponentPool<Transform>* pool_transform = nullptr;
    static ComponentPool<Tag>* pool_tag = nullptr;
    static ComponentPool<Parent>* pool_parents = nullptr;
    static ComponentPool<Projector>* pool_projs = nullptr;
    
    valid_entities.clear(); 
    app.em.get_entities_storage().available_bits.for_each_skip_1_bits(
        [this](size_t pos){
            valid_entities.push_back(app.em.get_entities_storage()[pos]);
        },app.em.get_entities_storage().size()
    );

    ImGui::Text("检查器");
    
    std::string a;
    for(size_t i = 0;i < valid_entities.size();++i){
        a = "Entity";
        const char* entity_tag = nullptr;
        if(pool_tag){
            auto it = pool_tag->mapper.find(valid_entities[i].id);
            if(it != pool_tag->mapper.end()){
                entity_tag = pool_tag->data[it->second].tag.c_str();
            }
        }else pool_tag = app.em.get_component_pool<Tag>();
        if(entity_tag){
            a += " @" + std::string(entity_tag);
        }
        a += " #" + std::to_string(valid_entities[i].id);
        
        if(ImGui::CollapsingHeader(a.c_str())){
            ImGui::PushID(valid_entities[i].id);
            ImGui::Text("ID: %lu",valid_entities[i].id);
            ImGui::Text("版本号: %lu",valid_entities[i].version);
            ImGui::Indent();
            /// Transform
            if(pool_transform){
                auto it = pool_transform->mapper.find(valid_entities[i].id);
                if(it != pool_transform->mapper.end()){
                    comps::Transform & trans = pool_transform->data[it->second];
                    if(ImGui::CollapsingHeader("Transform",ImGuiTreeNodeFlags_DefaultOpen)){
                        ImGui::DragFloat3("位置",glm::value_ptr(trans.m_position),0.1);
                        ImGui::DragFloat4("旋转",glm::value_ptr(trans.m_rotation.get_mutable_unnorm()),0.01);
                        ImGui::DragFloat3("缩放",glm::value_ptr(trans.m_scale),0.01);
                        ImGui::DragFloat3("速度",glm::value_ptr(trans.velocity),0.1);
                    } 
                }
            }else pool_transform = app.em.get_component_pool<Transform>();
            
            /// Parents
            if(pool_parents){
                auto it = pool_parents->mapper.find(valid_entities[i].id);
                if(it != pool_parents->mapper.end()){
                    comps::Parent & parent = pool_parents->data[it->second];
                    if(ImGui::CollapsingHeader("Parent",ImGuiTreeNodeFlags_DefaultOpen)){
                        ImGui::Text("父类 %lu %lu",parent.parent.id,parent.parent.version);
                    } 
                }
            }else pool_parents = app.em.get_component_pool<Parent>();
            
            /// Projector
            if(pool_projs){
                auto it = pool_projs->mapper.find(valid_entities[i].id);
                if(it != pool_projs->mapper.end()){
                    comps::Projector & proj = pool_projs->data[it->second];
                    if(ImGui::CollapsingHeader("Projector",ImGuiTreeNodeFlags_DefaultOpen)){
                        ImGui::DragFloat("FOV",&proj.fovRad,0.01);
                        ImGui::DragFloat("近裁切面",&proj.zNear,0.1);
                        ImGui::DragFloat("远裁切面",&proj.zFar,0.1);

                        proj.dm_mark();
                        app.projectionMatrix.uploadmat4(proj.buildProjectionMatrix());
                    } 
                }
            }else pool_projs = app.em.get_component_pool<Projector>();

            ImGui::Unindent();
            ImGui::PopID();
        }
    }
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
    app.m_sampler->wrapR(wrapFn(s.sampler_wrap_r)).wrapS(wrapFn(s.sampler_wrap_s)).wrapT(wrapFn(s.sampler_wrap_t));
    if(max_anisotropy)app.m_sampler->try_anisotropy(s.sampler_aniso);
    app.m_sampler->borderColor(s.sampler_border_color);
}

void ImGUIInjector::texture(){
    ImGui::Text("纹理");
    ImGui::ListBox("默认绑定的",&s.current_texture_id,app.cfg.texture_sids.data(),app.cfg.texture_sids.size());
    ImGui::DragInt("预览",&s.texture_preview_size,0.5F,0,1024);
    auto tex = *app.app.getTexture(app.cfg.texture_sids[s.current_texture_id]);
    ImGui::Image((ImTextureID)(intptr_t)tex->getId(), ImVec2(s.texture_preview_size,s.texture_preview_size * tex->getTextureInfo().height / (float)tex->getTextureInfo().width));
}

void ImGUIInjector::model(){
    static std::vector<const char *> buf;
    ImGui::Text("模型");

    buf.clear();
    for(auto&[k,v] : app.models){
        buf.push_back(k.data());
    }
    ImGui::ListBox("默认加载的模型",&s.current_model_index,buf.data(),buf.size());
    if(ImGui::DragInt("生成精度",&s.precision,0.1,1,64)){
        app.load_dynamic_models();
    }

    if(buf.size())app.current_model = &app.models[buf[s.current_model_index]];
}

void ImGUIInjector::gl(){
    ImGui::Text("GL设置");
}

void ImGUIInjector::render(){
    ImGui::Text("渲染设置");
    ImGui::Checkbox("立方体",&s.show_cube);
    ImGui::Checkbox("金字塔",&s.show_pyramid);
    ImGui::Checkbox("模型",&s.show_model);
    ImGui::Checkbox("地板",&s.show_platform);
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