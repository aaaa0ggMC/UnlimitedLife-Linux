/**
 * @file Light.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 光照支持
 * @version 0.1
 * @date 2025/07/24
 *
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/07/24 
 */
#ifndef AGE_H_LIGHT
#define AGE_H_LIGHT
#include <AGE/Utils.h>
#include <AGE/Shader.h>
#include <functional>
#include <tuple>

namespace age{
    namespace light{

        template<class T> struct DataUploader{
            std::function<void(const T & value)> upload;
        };

        namespace uploaders{
            template<class T> struct UniformName{
                std::string name;
                ShaderUniform cached;

                inline UniformName(std::string_view name){
                    this->name = name;
                }

                inline UniformName(Shader & shader,std::string_view name){
                    cached = shader.getUniformByName(name);
                }

                inline void operator()(const T & val){
                    if(!cached.isInvalid()){
                        // 利用cache的重载，但是注意，重载类型错误了那么你就完蛋了
                        cached.upload(val);
                        return;
                    }
#ifdef AGE_LIGHT_BUZZ
                    Error::def.pushMessage({
                        AGEE_EMPTY_DATA,"The cached shader uniform is invalid in UniformName.",ErrorLevel::Error
                    });
#endif
                }

                inline void operator()(const T & val,Shader & shader){
                    // 由于构建DataUploader时已经选好了是那个functor的调用了，所以这里直接parallel call
                    cached = shader[name];
                    operator()(val);
                }

            };
        }

        // 卧槽，要被c++折磨死了，没想到c++还能玩得这么花，AI还是懂的太多了
        // 现在我的脑子正在被std::apply,std::move,std::forward,std::bind,Policy,Functor,std::tuple，std::forward_as_tuple强碱
        template<class T,class Fn,class... Ts> inline DataUploader<T> createDataUploader
            (Fn&& func,Ts&&... args){
            DataUploader<T> ret;
            //using namespace std::placeholders;
            //ret.upload = std::bind(std::forward<Fn>(func),_1,std::forward<Ts>(args)...);
            auto params = std::forward_as_tuple(std::forward<Ts>(args)...);
            ret.upload = [f = std::forward<Fn>(func), param = std::move(params)](const T & val) mutable ->void{
                std::apply(
                    [&](auto&... unpacked){
                        f(val,unpacked...);
                    },
                param);
            };
            return ret;
        }

        template<class T = Shader> struct LightBindings{
        private:
            T * data {nullptr};

        public:
            DataUploader<glm::vec3> position;

            inline void bind(T & target){
                data = &target;
            }

            inline LightBindings<T> copy(T & t){
                LightBindings<T> ret;
                ret.position = position;
            }

            template<class L> inline void upload(L & l){
                position.upload(&l.position);
            }
        };

        /// @brief Light,a bunch of cpu data
        struct AGE_API Light{
            glm::vec3 position;
            glm::vec3 color;

            template<class T> inline void upload(const LightBindings<T> & binding){
                binding.upload(*this);
            }
        };
    }
}


#endif