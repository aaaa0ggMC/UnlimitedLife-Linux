/// AI GENERATED
#ifndef AGE_GLO_MAPPER_H_INCLUDED
#define AGE_GLO_MAPPER_H_INCLUDED
#include <GL/glew.h> // 或者您的 OpenGL 头文件 (glad, etc.)
#include <type_traits>

namespace age{
    // 辅助用于 static_assert (标准做法，防止非实例化分支报错)
    template <auto...> struct always_false : std::false_type {};
    // -----------------------------------------------------------
    // 1. 对象类型 -> 当前绑定状态查询枚举 (用于 glGetIntegerv)
    // -----------------------------------------------------------
    template<GLuint in> constexpr GLenum GLObjectToBindingMapper(){
        // --- Textures ---
        if constexpr(in == GL_TEXTURE_2D) return GL_TEXTURE_BINDING_2D;
        else if constexpr(in == GL_TEXTURE_3D) return GL_TEXTURE_BINDING_3D;
        else if constexpr(in == GL_TEXTURE_1D) return GL_TEXTURE_BINDING_1D;
        else if constexpr(in == GL_TEXTURE_2D_MULTISAMPLE) return GL_TEXTURE_BINDING_2D_MULTISAMPLE;
        else if constexpr(in == GL_TEXTURE_CUBE_MAP) return GL_TEXTURE_BINDING_CUBE_MAP;
        else if constexpr(in == GL_TEXTURE_RECTANGLE) return GL_TEXTURE_BINDING_RECTANGLE;
        
        // --- Framebuffers (FBO) ---
        // 注意：GL_FRAMEBUFFER 绑定会同时影响 READ 和 DRAW
        else if constexpr(in == GL_FRAMEBUFFER) return GL_FRAMEBUFFER_BINDING;
        else if constexpr(in == GL_READ_FRAMEBUFFER) return GL_READ_FRAMEBUFFER_BINDING;
        else if constexpr(in == GL_DRAW_FRAMEBUFFER) return GL_DRAW_FRAMEBUFFER_BINDING;

        // --- Renderbuffers (RBO) ---
        else if constexpr(in == GL_RENDERBUFFER) return GL_RENDERBUFFER_BINDING;

        // --- Vertex Arrays (VAO) ---
        // 注意：VAO 不是用 bindBuffer 而是 bindVertexArray，但查询逻辑一致
        else if constexpr(in == GL_VERTEX_ARRAY) return GL_VERTEX_ARRAY_BINDING;

        // --- Buffers (VBO, EBO, UBO, SSBO) ---
        else if constexpr(in == GL_ARRAY_BUFFER) return GL_ARRAY_BUFFER_BINDING;
        else if constexpr(in == GL_ELEMENT_ARRAY_BUFFER) return GL_ELEMENT_ARRAY_BUFFER_BINDING;
        else if constexpr(in == GL_UNIFORM_BUFFER) return GL_UNIFORM_BUFFER_BINDING;
        else if constexpr(in == GL_SHADER_STORAGE_BUFFER) return GL_SHADER_STORAGE_BUFFER_BINDING;
        else if constexpr(in == GL_PIXEL_PACK_BUFFER) return GL_PIXEL_PACK_BUFFER_BINDING;
        else if constexpr(in == GL_PIXEL_UNPACK_BUFFER) return GL_PIXEL_UNPACK_BUFFER_BINDING;

        // --- Shader Program ---
        else if constexpr(in == GL_PROGRAM) return GL_CURRENT_PROGRAM;

        else {
            static_assert(always_false<in>::value, "Feature Not Supported or Mapping Missing!");
            return 0;
        }
    }

    // -----------------------------------------------------------
    // 2. 对象类型 -> 绑定函数 (返回函数指针)
    // -----------------------------------------------------------
    template<GLuint in> constexpr auto GLObjectToBindingFuncMapper(){
        // --- Textures (使用 glBindTexture) ---
        if constexpr(in == GL_TEXTURE_2D || in == GL_TEXTURE_BINDING_2D){
            return +[](GLuint val){ glBindTexture(GL_TEXTURE_2D, val); };
        }
        else if constexpr(in == GL_TEXTURE_3D || in == GL_TEXTURE_BINDING_3D){
            return +[](GLuint val){ glBindTexture(GL_TEXTURE_3D, val); };
        }
        else if constexpr(in == GL_TEXTURE_1D || in == GL_TEXTURE_BINDING_1D){
            return +[](GLuint val){ glBindTexture(GL_TEXTURE_1D, val); };
        }
        else if constexpr(in == GL_TEXTURE_CUBE_MAP || in == GL_TEXTURE_BINDING_CUBE_MAP){
            return +[](GLuint val){ glBindTexture(GL_TEXTURE_CUBE_MAP, val); };
        }
        else if constexpr(in == GL_TEXTURE_2D_MULTISAMPLE || in == GL_TEXTURE_BINDING_2D_MULTISAMPLE){
            return +[](GLuint val){ glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, val); };
        }

        // --- Framebuffers (使用 glBindFramebuffer) ---
        else if constexpr(in == GL_FRAMEBUFFER || in == GL_FRAMEBUFFER_BINDING){
            return +[](GLuint val){ glBindFramebuffer(GL_FRAMEBUFFER, val); };
        }
        else if constexpr(in == GL_READ_FRAMEBUFFER || in == GL_READ_FRAMEBUFFER_BINDING){
            return +[](GLuint val){ glBindFramebuffer(GL_READ_FRAMEBUFFER, val); };
        }
        else if constexpr(in == GL_DRAW_FRAMEBUFFER || in == GL_DRAW_FRAMEBUFFER_BINDING){
            return +[](GLuint val){ glBindFramebuffer(GL_DRAW_FRAMEBUFFER, val); };
        }

        // --- Renderbuffers (使用 glBindRenderbuffer) ---
        else if constexpr(in == GL_RENDERBUFFER || in == GL_RENDERBUFFER_BINDING){
            return +[](GLuint val){ glBindRenderbuffer(GL_RENDERBUFFER, val); };
        }

        // --- Vertex Arrays (使用 glBindVertexArray) ---
        // 注意：VAO 的函数名不同
        else if constexpr(in == GL_VERTEX_ARRAY || in == GL_VERTEX_ARRAY_BINDING){
            return +[](GLuint val){ glBindVertexArray(val); };
        }

        // --- Buffers (使用 glBindBuffer) ---
        else if constexpr(in == GL_ARRAY_BUFFER || in == GL_ARRAY_BUFFER_BINDING){
            return +[](GLuint val){ glBindBuffer(GL_ARRAY_BUFFER, val); };
        }
        else if constexpr(in == GL_ELEMENT_ARRAY_BUFFER || in == GL_ELEMENT_ARRAY_BUFFER_BINDING){
            return +[](GLuint val){ glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, val); };
        }
        else if constexpr(in == GL_UNIFORM_BUFFER || in == GL_UNIFORM_BUFFER_BINDING){
            return +[](GLuint val){ glBindBuffer(GL_UNIFORM_BUFFER, val); };
        }
        else if constexpr(in == GL_SHADER_STORAGE_BUFFER || in == GL_SHADER_STORAGE_BUFFER_BINDING){
            return +[](GLuint val){ glBindBuffer(GL_SHADER_STORAGE_BUFFER, val); };
        }

        // --- Shader Program (使用 glUseProgram) ---
        else if constexpr(in == GL_PROGRAM || in == GL_CURRENT_PROGRAM){
            return +[](GLuint val){ glUseProgram(val); };
        }

        else {
            static_assert(always_false<in>::value, "Feature Not Supported or Mapping Missing!");
            return +[](GLuint){}; // Dummy return to satisfy compiler if needed
        }
    }
}

#endif