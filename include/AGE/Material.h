/**
 * @file Material.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 材质
 * @version 0.1
 * @date 2025/11/17
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/07/20 
*/
#ifndef AGE_H_MATERIAL
#define AGE_H_MATERIAL
#include <AGE/Utils.h>
#include <AGE/DataUploader.h>
#include <AGE/Color.h>

namespace age{
    /// 材质配置
    namespace material{
        struct AGE_API MaterialBindings{
            DataUploader<float> shininess;
            DataUploader<glm::vec4> ambient;
            DataUploader<glm::vec4> diffuse;
            DataUploader<glm::vec4> specular;
        };

        struct AGE_API Material{
            float shininess;
            Color ambient;
            Color diffuse;
            Color specular;

            inline void upload(MaterialBindings & binding){
                binding.shininess.safe_upload(shininess);
                ambient.uploadRGBA(binding.ambient);
                diffuse.uploadRGBA(binding.diffuse);
                specular.uploadRGBA(binding.specular);
            }
        };
    }
}

#endif