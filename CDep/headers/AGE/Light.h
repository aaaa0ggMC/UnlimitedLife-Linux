/**
 * @file Light.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 光照支持
 * @version 0.1
 * @date 2026/02/11
 *
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/07/24 
 */
#ifndef AGE_H_LIGHT
#define AGE_H_LIGHT
#include <AGE/Utils.h>
#include <AGE/Shader.h>
#include <AGE/DataUploader.h>
#include <AGE/Color.h>

namespace age{
    namespace light{

        struct AGE_API LightBindings{
            //// Position
            DataUploader<glm::vec3> position;

            //// DirectionalLights
            DataUploader<glm::vec3> direction;

            //// ADS
            DataUploader<glm::vec4> ambient;
            DataUploader<glm::vec4> diffuse;
            DataUploader<glm::vec4> specular;
        };

        /// @brief 方向光
        struct AGE_API DirectionalLight{
            glm::vec3 direction;
            // color has been wrapped with dirtywrapper already
            Color ambient;
            Color diffuse;
            Color specular;

            /// 见下面
            inline void upload(const LightBindings & binding){
                binding.direction.safe_upload(direction);
                ambient.uploadRGBA(binding.ambient);
                diffuse.uploadRGBA(binding.diffuse);
                specular.uploadRGBA(binding.specular);
            }
        };


        struct AGE_API PositionalLight{
            glm::vec3 position;
            // color has been wrapped with dirtywrapper already
            Color ambient;
            Color diffuse;
            Color specular;

            /// 由于binding是可变的，为了灵活性，这里不应该检测
            inline void upload(const LightBindings & binding){
                binding.position.safe_upload(position);
                ambient.uploadRGBA(binding.ambient);
                diffuse.uploadRGBA(binding.diffuse);
                specular.uploadRGBA(binding.specular);
            }
        };

        struct AGE_API SpotLight{

        };

        /// @brief default uploaders for binding,you can also define you own
        namespace uploaders{
            template<class T> struct AGE_API UniformName{
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
                        AGEE_EMPTY_DATA,"The cached shader uniform is invalid in UniformName.",alib5::LogLevel::Error
                    });
#endif
                }

                inline void operator()(const T & val,Shader & shader){
                    // 由于构建DataUploader时已经选好了是那个functor的调用了，所以这里直接parallel call
                    cached = shader[name];
                    operator()(val);
                }

            };

            template<class K,class T>  struct _UploaderBuilder{
                K target;

                inline _UploaderBuilder(K && t):target(std::move(t)){}

                template<class... Args> DataUploader<T> operator()(Args&&... args){
                    return createDataUploader<T>(target,std::forward<Args>(args)...);
                }
            };

            template<class K,class T,class... Args> inline _UploaderBuilder<K,T> createType(Args&&... args){
                //static_assert(std::is_constructible_v<K,Args...>,"UploaderType must be constructible with the provided arguments");
                return _UploaderBuilder<K,T>(K(std::forward<Args>(args)...));
            }

            template<class T,class... Args> inline _UploaderBuilder<UniformName<T>,T> createUniformName(Args&&... args){
                //static_assert(std::is_constructible_v<UniformName<T>,Args...>,"UniformName<T> must be constructible with the provided arguments");
                return createType<UniformName<T>,T>(std::forward<Args>(args)...);
            }
        }
    }
}


#endif