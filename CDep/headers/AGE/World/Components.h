#ifndef AGE_COMP
#define AGE_COMP
#include <AGE/Base.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace age::world{
    namespace comps{

        struct Basic{

        };

        struct Transform : public DirtyMarker,public Basic{
            glm::vec3 m_position;
            glm::vec3 m_scale;
            glm::quat m_rotation;

            glm::mat4 model_matrix;

            inline static Transform null(){
                Transform ret;
                ret.m_position = glm::vec3(0,0,0);
                ret.m_scale = glm::vec3(1,1,1);
                ret.m_rotation = glm::quat(0,0,0,0);
                ret.model_matrix = glm::mat4(1.0f);
                return ret;
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
            inline Transform& rotateLocal(const glm::vec3& axis,float rad){
                dm_mark();
                m_rotation = glm::angleAxis(rad, glm::normalize(axis)) *m_rotation;
                return *this;
            }

            inline Transform& rotateLocal(const glm::quat & iquat){
                dm_mark();
                m_rotation = iquat *m_rotation;
                return *this;
            }

            inline Transform& rotateWorld(const glm::vec3& axis,float rad){
                dm_mark();
                m_rotation = m_rotation * glm::angleAxis(rad, glm::normalize(axis));
                return *this;
            }

            inline Transform& rotateWorld(const glm::quat & iquat){
                dm_mark();
                m_rotation = m_rotation * iquat;
                return *this;
            }

            inline Transform& setRotation(const glm::quat & rot){
                dm_mark();
                m_rotation = rot;
                return *this;
            }

            //mat
            inline glm::mat4& buildModelMatrix(){
                if(!dm_check())return model_matrix;
                model_matrix = glm::scale(glm::mat4(1.0f), m_scale);
                model_matrix = model_matrix * glm::mat4_cast(glm::normalize(m_rotation));
                model_matrix = glm::translate(model_matrix, m_position);
                return model_matrix;
            }
        };

        struct Viewer: public Basic{
            inline static Viewer null(){
                return Viewer();
            }

            inline glm::mat4& buildViewMatrix(Transform & trs){
                if(!trs.dm_check())return trs.model_matrix;
                glm::mat4 & model = trs.model_matrix;
                model = glm::scale(glm::mat4(1.0f), trs.m_scale);
                model = model * glm::transpose(glm::mat4_cast(glm::normalize(trs.m_rotation)));
                model = glm::translate(model,trs.m_position);
                return model;
            }
        };

        struct Projector: public Basic{
            glm::mat4 proj_matrix;

            inline static Projector null(){
                Projector ret;
                ret.proj_matrix = glm::mat4(1.0f);
                return ret;
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
    }
}

#endif