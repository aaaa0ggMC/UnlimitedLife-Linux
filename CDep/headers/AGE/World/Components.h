/** @file world/Components.h
 * @brief 提供一些预制的components
 * @author aaaa0ggmc,euuen
 * @date 2025/12/04
 * @start-date 2025/6/19
 * @copyright copyright(c)2025 aaaa0ggmc
 */
#ifndef AGE_H_COMP
#define AGE_H_COMP
#include <AGE/Utils.h>
#include <alib-g3/aecs.h>

#include <cstdio>
#include <cstdlib>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_geometric.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <numbers>

#include <string>
#include <iostream>

inline static void glm_e_add(glm::vec3 & outp,float x1,float x2,float x3,const glm::vec3& left,const glm::vec3& up,const glm::vec3& forward,float mul){
    outp += mul * (x1*left + x2*up + x3*forward);
}

namespace age::world{
    using namespace alib::g3::ecs;

    /** @brief 这里是默认提供的一些组件，实际上你也可以自己写
      * @par 组件类规则:
      * 1.需要包含 void reset();这个函数
      * 2.没了
      */
    
    template<size_t index,class... Ts> struct AGE_API TypeAt;
    
    template<class T,class... Ts> struct AGE_API TypeAt <0,T,Ts...>{
        using type = T;
    };

    template<size_t index,class T,class... Ts> struct TypeAt <index,T,Ts...>{
        static_assert(index < sizeof...(Ts) + 1,"Index out of range!");
        using type = TypeAt<index-1,Ts...>::type;
    };

    template<class... Ts> struct AGE_API ComponentRequisitions{
    public:

        inline size_t getContSize(){
            return sizeof...(Ts);
        }
    
        template<size_t Index>
        using getType = typename TypeAt<Index, Ts...>::type;

        template<class Function,class... Args> void action(Function && func,Args&&... args){
            (func.template operator()<Ts>(args...),...);
        }

    };

    namespace comps{
        /// 有关模型与世界
        struct AGE_API Transform : public DirtyMarker{
            glm::vec3 m_position;
            glm::vec3 m_scale;
            glm::mat4 model_matrix;
            glm::vec3 velocity;

            glm::vec3 m_up,m_left,m_forward;

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

            inline void system_dm_mark(){
                dm_mark();
                m_rotation.dm_mark();
            }

            Transform(){
                reset();
            }

            inline void lookAt(glm::vec3 center,glm::vec3 up = glm::vec3(0,1,0)){
                dm_mark();
                // 复用model_matrix，后面会自动懒惰求值覆盖
                model_matrix = glm::lookAt(m_position,center,up);
                m_rotation.get_mutable_unnorm() = glm::quat_cast(model_matrix);
            }

            inline void reset(){
                dm_clear();
                Transform &ret = *this;
                ret.m_position = glm::vec3(0,0,0);
                ret.m_scale = glm::vec3(1,1,1);
                ret.m_rotation.get_mutable_unnorm() = glm::quat(1,0,0,0);
                ret.m_rotation.dm_clear();
                ret.model_matrix = glm::mat4(1.0f);
                ret.velocity = glm::vec3(0,0,0);
                ret.m_up = glm::vec3(0,1,0);
                ret.m_left = glm::vec3(1,0,0);
                ret.m_forward = glm::vec3(0,0,1);
            }

            //velocity
            inline Transform& buildVelocity(const glm::vec3& direction,float length){
                if(direction.x == 0 && direction.y == 0 && direction.z == 0){
                    velocity = glm::vec3(0,0,0);
                    return *this;
                }
                velocity = length * glm::normalize(direction);
                return *this;
            }

            inline Transform& buildVelocity(const glm::vec3& velo){
                velocity = velo;
                return *this;
            }

            //pos
            inline Transform& move(const glm::vec3& v){
                if(v.x == 0 && v.y == 0 && v.z == 0)return *this;
                dm_mark();
                m_position += v;
                return *this;
            }

            inline Transform& move(float x,float y,float z){
                if(x == 0 && y == 0 && z == 0)return *this;
                dm_mark();
                m_position.x += x;
                m_position.y += y;
                m_position.z += z;
                return *this;
            }

            inline Transform& move(const glm::vec3& velo,float mul){
                if(velo.x == 0 && velo.y == 0 && velo.z == 0)return *this;
                dm_mark();
                m_position.x += velo.x * mul;
                m_position.y += velo.y * mul;
                m_position.z += velo.z * mul;
                return *this;
            }

            inline Transform& move(float x,float y,float z,float mul){
                if(x == 0 && y == 0 && z == 0)return *this;
                dm_mark();
                m_position.x += x * mul;
                m_position.y += y * mul;
                m_position.z += z * mul;
                return *this;
            }

            //move directional
            inline Transform& moveDirectional(const glm::vec3& v){
                if(v.x == 0 && v.y == 0 && v.z == 0)return *this;
                dm_mark();
                glm_e_add(m_position,v.x,v.y,v.z,m_left,m_up,m_forward,1);
                return *this;
            }

            inline Transform& moveDirectional(float x,float y,float z){
                if(x == 0 && y == 0 && z == 0)return *this;
                dm_mark();
                glm_e_add(m_position,x,y,z,m_left,m_up,m_forward,1);
                return *this;
            }

            inline Transform& moveDirectional(const glm::vec3& v,float mul){
                if(v.x == 0 && v.y == 0 && v.z == 0)return *this;
                dm_mark();
                glm_e_add(m_position,v.x,v.y,v.z,m_left,m_up,m_forward,mul);
                return *this;
            }

            inline Transform& moveDirectional(float x,float y,float z,float mul){
                if(x == 0 && y == 0 && z == 0)return *this;
                dm_mark();
                glm_e_add(m_position,x,y,z,m_left,m_up,m_forward,mul);
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
                if(s.x == 1 && s.y == 1 && s.z == 1)return *this;
                dm_mark();
                m_scale.x *= s.x;
                m_scale.y *= s.y;
                m_scale.z *= s.z;
                return *this;
            }

            inline Transform& scale(float x,float y,float z){
                if(x == 1 && y == 1 && z == 1)return *this;
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
                if(axis.x == 0 && axis.y == 0 && axis.z == 0)return *this;
                dm_mark();
                m_rotation.get_mutable_unnorm() = glm::angleAxis(rad,glm::normalize(axis)) * m_rotation.get_mutable_unnorm();
                return *this;
            }

            inline Transform& rotate(const glm::quat & iquat){
                if(iquat.x == 0 && iquat.y == 0 && iquat.z == 0)return *this;
                dm_mark();
                m_rotation.get_mutable_unnorm() = iquat * m_rotation.get_mutable_unnorm();
                return *this;
            }

            inline Transform& setRotation(const glm::quat & rot){
                dm_mark();
                m_rotation.get_mutable_unnorm() = rot;
                return *this;
            }

            inline const glm::vec3& left(){
                return m_left;
            }

            inline const glm::vec3& up(){
                return m_up;
            }

            inline const glm::vec3& forward(){
                return m_forward;
            }

            //mat
            inline glm::mat4& buildModelMatrix(bool keepModelBuild = true){
                if(!dm_check())return model_matrix;
                dm_clear();
                if(keepModelBuild){
                    model_matrix = glm::translate(glm::mat4(1.0), m_position);
                    model_matrix *= glm::mat4_cast(m_rotation.get());
                    model_matrix *= glm::scale(glm::mat4(1.0f), m_scale);
                }

                /// build drs
                m_up  = glm::vec3(0,1,0) * m_rotation.get();
                m_left =  glm::vec3(1,0,0) * m_rotation.get();
                m_forward = glm::vec3(0,0,1) * m_rotation.get();

                return model_matrix;
            }

            /// @brief It's a simple function,supports only 1 layer of parent-child relationship
            /// @note use ParentSystem for more reliable operations
            inline glm::mat4& buildModelMatrixWithParent(Transform & parent){
                if(!dm_check() && !parent.dm_check())return model_matrix;
                dm_clear();
                
                model_matrix = parent.buildModelMatrix();
                model_matrix *= glm::translate(glm::mat4(1.0), m_position);
                model_matrix *= glm::mat4_cast(m_rotation.get());
                model_matrix *= glm::scale(glm::mat4(1.0f), m_scale);
                /// build drs
                m_up  = glm::vec3(0,1,0) * m_rotation.get() * parent.m_rotation.get();
                m_left =  glm::vec3(1,0,0) * m_rotation.get() * parent.m_rotation.get();
                m_forward = glm::vec3(0,0,1) * m_rotation.get() * parent.m_rotation.get();

                return model_matrix;
            }

            inline void update(float elapseTime_ms,bool ignore_y_directional){
                float p = (elapseTime_ms / 1000.0);
                if(ignore_y_directional){
                    moveDirectional(velocity.x,0,velocity.z,p);
                    move(0,velocity.y,0,p);
                }
                else moveDirectional(velocity,p);
            }
        };

        /// 仅用于构建视图矩阵，而且还依赖Transform这个module @todo maybe可以使用c++模板元编程把组件之间的依赖关系也写出来
        struct AGE_API Viewer : public DirtyMarker{
            using Dependency = alib::g3::ecs::ComponentStack<Transform>;
            glm::mat4 view_matrix;

            inline glm::mat4& buildViewMatrix(Transform & trs){
                if(!trs.dm_check() && !dm_check())return view_matrix;
                trs.buildModelMatrix();
                dm_clear();
                glm::mat4 & model = view_matrix;
                model = glm::mat4_cast((trs.m_rotation.get()));
                model = glm::translate(model,-trs.m_position);
                return model;
            }
        };

        struct AGE_API Parent : IBindEntity{
            using Dependency = ComponentStack<Transform>;
            Entity parent;

            Parent(const Entity & p){
                parent = p;
            }
            //Cache Component Index is not a good option,because the index may vary when the user deleted the component and then added it
        };

        /// reserved used for test
        struct AGE_API TestOutput{
            inline void out(std::string_view  data){
                std::cout << data << std::endl;
            }
        };

        /// 投影矩阵，目前的版本由懒惰的 raleeuuen 写的。补充：居然给我改了？？
        struct AGE_API Projector : public DirtyMarker{
            glm::mat4 proj_matrix;

            float fovRad;
            float aspectRatio;
            float zNear;
            float zFar;

            Projector(){
                reset();
            }

            inline void reset(){
                dm_clear();
                proj_matrix = glm::mat4(1.0f);
                fovRad = std::numbers::pi / 3.0f;
                aspectRatio = 4.0 / 3.0;
                zNear = 0.1;
                zFar = 1000;
            }

            inline Projector& set(float angleRad,float w,float h,float lnear = 0.1f,float lfar = 1000.0f){
		auto & p = *this;
		p.setFOV(angleRad).setAspectRatio(w,h).setClipPlane(lnear,lfar);
                return p;
            }

            inline Projector& setFOV(float angleRad){
                fovRad = angleRad;
                return *this;
            }

            inline Projector& setAspectRatio(float w,float h){
                dm_mark();
                aspectRatio = w/h;
                return *this;
            }

            inline Projector& setClipPlane(float lnear,float lfar){
                dm_mark();
                zNear = lnear;
                zFar = lfar;
                return *this;
            }

            inline glm::mat4& buildProjectionMatrix(){
                if(!dm_check())return proj_matrix;
                dm_clear();
                proj_matrix = glm::perspective(fovRad,aspectRatio,zNear,zFar);
                return proj_matrix;
            }
        };

        struct AGE_API Tag{
        public:
            std::string tag;

            Tag(std::string_view set_tag){
                reset(set_tag);
            }

            inline void reset(std::string_view set_tag){
                tag = set_tag;
            }
        };

        struct AGE_API Runner{
        public:
            void (*fn)(void*,void*);
            void * arg1;
            void * arg2;

            inline Runner(void (*ifn)(void*,void*),void * iarg1,void * iarg2):fn{ifn},arg1{iarg1},arg2{iarg2}{}

            Runner(){
                reset();
            }

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
