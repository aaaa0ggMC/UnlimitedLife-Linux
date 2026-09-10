module ave.profile;

using namespace ave;

Renderer RenderProfile::build(alib6::ErrorWrapper ew){
    Renderer renderer;
    /// 创建好instance
    if(!with_data.instance){
        // 用户可以自己销毁这个 instance,如果不需要的话
        if(!__vk_instance(renderer, ew)) return renderer;
    }else{
        renderer.instance = with_data.instance;
    }

    return renderer;
}

bool RenderProfile::__vk_instance(Renderer & r, alib6::ErrorWrapper ew){
    CreateInstanceInfo ci{
        .ctx = ctx,
        .ew = ew
    };
    WithGlobalInput gi (window);
    // glfw需要一些必需扩展，因此这里加入，用户要修改可以自己删了
    ci.extensions = gi.get_required_extensions();

    if(with_data.configure_instance){
        with_data.configure_instance(gi, ci);
    }

    r.instance = std::make_shared<Instance>();

    return r.instance->create(ci);
}