/** @file world/Components.h
 * @brief 提供一些预制的components
 * @author aaaa0ggmc,euuen
 * @date 2025/6/19
 * @copyright copyright(c)2025 aaaa0ggmc
 */
#ifndef AGE_H_COMP
#define AGE_H_COMP
#include <AGE/Base.h>

#include <cstdio>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <string>
#include <iostream>

namespace age::world{
    /** @brief 这里是默认提供的一些组件，实际上你也可以自己写
      * @par 组件类规则:
      * 1.需要包含 void reset();这个函数
      * 2.没了
      */
    namespace comps{
        /// 有关模型与世界
        struct AGE_API Transform : public DirtyMarker{
            glm::vec3 m_position;
            glm::vec3 m_scale;
            glm::mat4 model_matrix;

            struct AGE_API RotationProxy : public DirtyMarker{
            private:
                glm::quat m_rotation;
            public:
                inline const glm::quat& get(){
                    if(dm_check()){
                        m_rotation = glm::normalize(m_rotation);
                        dm_clear();
                    }
                    return m_rotation;
                }

                //assume that you've changed the data
                inline glm::quat& get_mutable_unnorm(){
                    dm_mark();
                    return m_rotation;
                }

            };
            RotationProxy m_rotation;

            inline void reset(){
                Transform &ret = *this;
                ret.m_position = glm::vec3(0,0,0);
                ret.m_scale = glm::vec3(1,1,1);
                ret.m_rotation.get_mutable_unnorm() = glm::quat(1,0,0,0);
                ret.m_rotation.dm_clear();
                ret.model_matrix = glm::mat4(1.0f);
            }

            //pos
            inline Transform& move(const glm::vec3& v){
                dm_mark();
                m_position += v;
                return *this;
            }

            inline Transform& move(float x,float y,float z){
                dm_mark();
                m_position.x += x;
                m_position.y += y;
                m_position.z += z;
                return *this;
            }

            inline Transform& setPosition(const glm::vec3& v){
                dm_mark();
                m_position = v;
                return *this;
            }

            inline Transform& setPosition(float x,float y,float z){
                dm_mark();
                m_position = glm::vec3(x,y,z);
                return *this;
            }

            //scale
            inline Transform& scale(const glm::vec3 & s){
                dm_mark();
                m_scale.x *= s.x;
                m_scale.y *= s.y;
                m_scale.z *= s.z;
                return *this;
            }

            inline Transform& scale(float x,float y,float z){
                dm_mark();
                m_scale.x *= x;
                m_scale.y *= y;
                m_scale.z *= z;
                return *this;
            }

            inline Transform& setScale(float x,float y,float z){
                dm_mark();
                m_scale.x = x;
                m_scale.y = y;
                m_scale.z = z;
                return *this;
            }

            inline Transform& setScale(const glm::vec3& s){
                dm_mark();
                m_scale = s;
                return *this;
            }

            //m_rotation
            inline Transform& rotate(const glm::vec3& axis,float rad){
                dm_mark();
                m_rotation.get_mutable_unnorm() = glm::angleAxis(rad,glm::normalize(axis)) * m_rotation.get_mutable_unnorm();
                return *this;
            }

            inline Transform& rotate(const glm::quat & iquat){
                dm_mark();
                m_rotation.get_mutable_unnorm() = iquat * m_rotation.get_mutable_unnorm();
                return *this;
            }

            inline Transform& setRotation(const glm::quat & rot){
                dm_mark();
                m_rotation.get_mutable_unnorm() = rot;
                return *this;
            }

            //mat
            inline glm::mat4& buildModelMatrix(){
                if(!dm_check())return model_matrix;
                dm_clear();
                model_matrix = glm::translate(glm::mat4(1.0), m_position);
                model_matrix *= glm::mat4_cast(m_rotation.get());
                model_matrix *= glm::scale(glm::mat4(1.0f), m_scale);
                return model_matrix;
            }
        };

        /// 仅用于构建视图矩阵，而且还依赖Transform这个module @todo maybe可以使用c++模板元编程把组件之间的依赖关系也写出来
        struct AGE_API Viewer{
            inline static void reset(){}

            inline glm::mat4& buildViewMatrix(Transform & trs){
                if(!trs.dm_check())return trs.model_matrix;
                trs.dm_clear();
                glm::mat4 & model = trs.model_matrix;
                model = glm::transpose(glm::mat4_cast(trs.m_rotation.get()));
                model = glm::translate(model,trs.m_position);
                return model;
            }
        };

        /// reserved used for test
        struct AGE_API TestOutput{
            inline void reset(){}

            inline void out(const std::string& data){
                std::cout << data << std::endl;
            }
        };

        /// 投影矩阵，目前的版本由懒惰的 raleeuuen 写的
        struct AGE_API Projector : public DirtyMarker{
            glm::mat4 proj_matrix;

            inline void reset(){
                Projector ret = *this;
                ret.proj_matrix = glm::mat4(1.0f);
            }

            inline glm::mat4& buildProjectionMatrix(){
                proj_matrix = glm::perspective(1.0472f,4.0f / 3.0f,0.1f,1000.0f);
                return proj_matrix;
            }

            inline glm::mat4& buildProjectionMatrix(float fovInRadius, float aspectRatio, float zNear, float zFar){
                proj_matrix = glm::perspective(fovInRadius, aspectRatio, zNear, zFar);
                return proj_matrix;
            }
        };

        struct AGE_API Runner{
        public:
            void (*fn)(void*,void*);
            void * arg1;
            void * arg2;

            inline Runner(void (*ifn)(void*,void*),void * iarg1,void * iarg2):fn{ifn},arg1{iarg1},arg2{iarg2}{}

            inline void reset(){
                fn = NULL;
                arg1 = arg2 = NULL;
            }

            inline void run(){
                if(fn)fn(arg1,arg2);
            }
        };
    }
}

#endif
