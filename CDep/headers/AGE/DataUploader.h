/**
 * @file DataUploader.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 支持上传数据到用户定义的任何地方，支持prebind(创建uploader对象) 和 createDataUploader的postbind
 * @version 0.1
 * @date 2025/12/01
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/07/25 
 */
#ifndef AGE_H_DU
#define AGE_H_DU
#include <AGE/Utils.h>
#include <functional>
#include <tuple>

namespace age{
    template<class Fn,class... Ts> concept Callable = requires(Fn f,Ts... args){
        {f(args...)};
    };
    
    template<class Fn,class T,class... Ts> concept CallableWith = requires(Fn f,const T& val,Ts... args){
        {f(val,args...)};
    };

    template<class T> struct AGE_API DataUploader{
        std::function<void(const T & value)> upload;

        inline void safe_upload(const T& val) const{
            if(upload)upload(val);
        }
    };

    // 卧槽，要被c++折磨死了，没想到c++还能玩得这么花，AI还是懂的太多了
    // 现在我的脑子正在被std::apply,std::move,std::forward,std::bind,Policy,Functor,std::tuple，std::forward_as_tuple强碱
    template<class T,class Fn,class... Ts> inline DataUploader<T> createDataUploader
        (Fn&& func,Ts&&... args){
        static_assert(
            Callable<Fn,T,Ts...>,
            "The object you passed doesnt have a functor."
        );
        static_assert(
            CallableWith<Fn,T,Ts...>,
            "The object you passed have a functor that doesnt match the args you passed."
        );
        DataUploader<T> ret;
        //using namespace std::placeholders;
        //ret.upload = std::bind(std::forward<Fn>(func),_1,std::forward<Ts>(args)...);
        // auto params = std::forward_as_tuple(std::forward<Ts>(args)...);
        ret.upload = [f = std::forward<Fn>(func), param = std::make_tuple(std::forward<Ts>(args)...)](const T & val) mutable ->void{
            std::apply(
                [&](auto&... unpacked){
                    f(val,unpacked...);
                },
            param);
        };
        return ret;
    };
}

#endif