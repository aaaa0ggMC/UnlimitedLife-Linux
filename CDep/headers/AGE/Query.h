/**
 * @file Query.h
 * @author aaaa0ggmc
 * @brief 获取OpenGL各种信息，glew必须初始化完成才可以使用这个文件的东西
 * @version 0.1
 * @date 2025/07/18
 * @start-date 2025/07/18
 * @copyright Copyright (c) 2025
*/
#ifndef AGE_H_QUERY
#define AGE_H_QUERY
#include <GLFW/glfw3.h>
#include <GL/glew.h>
#include <stdexcept>

#include <AGE/Utils.h>

namespace age{
    /// @brief QueryOpenGL State @warning OpenGL Context Must Be Available,or the program will definitely crash!!
    struct AGE_API Queryer{
        inline Queryer(){
            if(glfwGetCurrentContext() == nullptr){
                throw std::runtime_error("GLFW Context is invalid!!");
            }
            glewInit(); //Make Sure GLEW is inited
        }

        /// @brief 查看 AF 支持情况
        /// @return <是否支持，AF最大值>
        inline std::pair<bool,float> anisotropicFiltering(){
            float result = 0;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&result);
            if(result == 0)return std::make_pair(false,0);
            return std::make_pair(true,result); 
        }
    };   
}

 #endif