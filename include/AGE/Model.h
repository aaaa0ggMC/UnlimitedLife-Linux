/**
 * @file Model.h
 * @author aaaa0ggmc
 * @brief 提供模型相关的支持，目前不支持Mesh,Mesh要等我再学学
 * @version 0.1
 * @date 2025/07/25
 * @start-date 2025/07/18
 * @copyright Copyright (c) 2025
*/
#ifndef AGE_H_MODEL
#define AGE_H_MODEL
#include <AGE/Utils.h>
#include <GL/glew.h>
#include <AGE/Material.h>
#include <AGE/VAO.h>
#include <AGE/VBO.h>

namespace age{
    template<class T> concept CanDrawElements = requires(T t){
        {t.drawElements(std::declval<age::PrimitiveType>(),std::declval<size_t>(),std::declval<GLuint>(),std::declval<const std::vector<int>&>(),std::declval<age::VAO>())};
    };

    /// @todo Mesh & Material
    struct AGE_API Mesh{
        size_t vertex_offset;
        size_t normal_offset;
        size_t coord_offset;
        size_t index_offset;

        material::Material material;
        std::string meshName;
    };

    /**
     * @brief 存储模型数据
     * @note 作为过渡的小玩意儿
     */
    struct AGE_API ModelData{
    private:
        VAO bindedVAO;
    public:
        std::vector<float> vertices;
        std::vector<float> normals;
        std::vector<float> coords;
        std::vector<int> indices;
        std::vector<Mesh> meshes;
        /// @todo 保存binding point,因为gltf可能需要

        /// 属于扩展功能,一般不建议使用
        /// 需要一个vao
        void bind(
            VAO vao,
            VBO vertBuffer,
            VBO indiceBuffer,
            VBO coordBuffer,
            VBO normalBuffer = VBO::null(),
            GLuint vertLocation = 0,
            GLuint coordLocation = 1,
            GLuint normalLocation = 2
        );

        inline size_t getIndiceCount() const{
            return indices.size();
        }
        
        template<CanDrawElements T> inline void draw(T & window,GLuint instanceCount,PrimitiveType type) const{
            if(!instanceCount)return;
            window.drawElements(
                type,
                getIndiceCount(),
                instanceCount,
                {}, // use GL_ELEMENT_ARRAY_BUFFER
                bindedVAO
            );
        }
    };

    struct Model{
    public:
        struct GraphRes{
            VAO vao;
            VBO vbuffer;        
            VBO ibuffer;
            VBO vtbuffer;
            VBO vnbuffer;
        };
    private:
        std::vector<ModelData> scenes; // 这里是为了兼容gltf,如果只是有一个scene那么defaultscene够了
        std::vector<GraphRes> gdata;
        int currentModelIndex { -1 };
    public:

        inline size_t getSceneCount(){
            return scenes.size();
        }

        inline std::optional<GraphRes*> getCurrentGraphRes(){
            if(currentModelIndex < 0)return std::nullopt;
            return (&gdata[currentModelIndex]);
        }

        inline std::optional<ModelData*> getCurrentModelData(){
            if(currentModelIndex < 0)return std::nullopt;
            return (&scenes[currentModelIndex]);
        }

        /// call loadXXX functions to add a element into model will break current binding status!!
        inline void bind(){
            if(currentModelIndex < 0)return;
            gdata[currentModelIndex].vao.bind();
        }

        /// @warning use this after you have created a OpenGL-Context Based Window and have inited GLEW!
        inline std::pair<ModelData*,GraphRes*> add(){
            auto& g = gdata.emplace_back();
            GLuint value[4] = {0};
            glGenVertexArrays(1,value);
            g.vao = VAO(value[0],0);
            value[0] = 0;
            glGenBuffers(4,value);
            g.vbuffer = VBO(value[0],0);
            g.ibuffer = VBO(value[1],0);
            g.vtbuffer = VBO(value[2],0);
            g.vnbuffer = VBO(value[3],0);
            auto& v = scenes.emplace_back();
            if(scenes.size() == 1){
                // 第一次默认加载
                currentModelIndex = 0;
            }
            return std::make_pair(&v,&g);
        }

        inline std::optional<std::pair<ModelData*,GraphRes*>> get(int index){
            if(index < 0 || index >= scenes.size())return std::nullopt;
            return {std::make_pair(&scenes[index],&gdata[index])};
        }

        inline void loadScene(int sceneId){
            if(sceneId < 0 || sceneId >= scenes.size())return;
            currentModelIndex = sceneId; // 防止vector扩容访问非法内存，不提供删除scene
        }

        template<CanDrawElements T> inline void draw(T & window,GLuint instanceCount,PrimitiveType type) const{
            if(!instanceCount  || currentModelIndex < 0)return;
            window.drawElements(
                type,
                scenes[currentModelIndex].getIndiceCount(),
                instanceCount,
                {}, // use GL_ELEMENT_ARRAY_BUFFER
                gdata[currentModelIndex].vao
            );
        }
    };
}

#endif