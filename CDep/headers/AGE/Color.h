/**
 * @file Color.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 颜色处理
 * @version 0.1
 * @date 2025/07/25
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/07/25 
 */
#ifndef AGE_H_COLOR
#define AGE_H_COLOR
#include <AGE/Utils.h>
#include <glm/glm.hpp>

namespace age{

    struct AGE_API Color{
        // 内部运算全部使用hsva,最终输出时使用rbga
    private:
        glm::vec4 rgba;

        static glm::vec4 HSVAToRGBA(const glm::vec4& hsva);
        static glm::vec4 RGBAToHSVA(const glm::vec4& rgba);
    public:
        DirtyWrapper<glm::vec4> hsva;

        inline const glm::vec4& getRGBA(){
            if(hsva.isDirty()){
                rgba = HSVAToRGBA(hsva.read());
                hsva.clearFlag();
            }
            return rgba;
        }

        inline const glm::vec4& getHSVA(){
            return hsva.read();
        }

        inline void fromRGBA(const glm::vec4 & rgba){
            this->rgba = rgba;
            hsva.write(RGBAToHSVA(rgba));
            hsva.clearFlag(); // 因为已经更新了
        }

        inline void fromHSVA(const glm::vec4 & hsva){
            this->hsva.write(hsva);
        }

        inline void fromHSVA(float h,float s,float v,float a){
            this->hsva.write(glm::vec4(h,s,v,a));
        }

        inline void fromRGBA(float r,float g,float b,float a){
            fromRGBA(glm::vec4(r,g,b,a));
        }

        static inline Color RGBA(float r,float g,float b,float a){
            Color ret;
            ret.fromRGBA(glm::vec4(r,g,b,a));
            return ret;
        }

        static inline Color RGBA(const glm::vec4& c){
            Color ret;
            ret.fromRGBA(c);
            return ret;
        }

        static inline Color HSVA(const glm::vec4& c){
            Color ret;
            ret.fromHSVA(c);
            return ret;
        }

        static inline Color HSVA(float h,float s,float v,float a){
            Color ret;
            ret.fromHSVA(glm::vec4(h,s,v,a));
            return ret;
        }

        template<class Uploader> inline void uploadRGBA(Uploader & up){
            up.safe_upload(getRGBA());
        }

        template<class Uploader> inline void uploadHSVA(Uploader & up){
            up.safe_upload(getHSVA());
        }
    };
}

#endif