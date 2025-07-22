#include <AGE/ModelLoader/Loader.h>
#include <sstream>
#include <vector>
#include <glm/glm.hpp>
#include <alib-g3/autil.h>
#include <unordered_set>

using namespace age::model::fmt;
using namespace age::model;
using namespace age;

void Stl::parse(std::string_view data,ModelData & md){
    
}


struct Verts{
    int vert;
    int coord;
    int normal;
    
    bool operator ==(const Verts & other) const {
        return vert == other.vert && coord == other.coord && normal == other.normal;
    }
};

namespace std{
    template <>
    struct hash<Verts> {
        size_t operator()(const Verts & v) const {
            size_t h1 = std::hash<int>()(v.vert);
            size_t h2 = std::hash<int>()(v.coord);
            size_t h3 = std::hash<int>()(v.normal);
            return ((h1 ^ (h2 << 1)) >> 1) ^ (h3 << 1);
        }
    };
}

void Obj::parse(std::string_view data,ModelData & md){
    std::string token;
    size_t vertex = 0,coord = 0,index = 0,normal = 0;
    std::vector<glm::vec3> vertDatas;
    std::vector<glm::vec3> normalDatas;
    std::vector<glm::vec2> coordDatas;
    std::unordered_map<Verts,int> vertSet;
    int diffIndex = 0;

    static auto vert_auto = [&](int vIndex, int vtIndex, int vnIndex) -> int {
        int ret = diffIndex;
        auto tg = vertSet.try_emplace(
            Verts(vIndex, vtIndex, vnIndex),
            diffIndex
        );
        if(tg.first->second == diffIndex){
            if(vIndex >= 0){  
                md.vertices.push_back(vertDatas[vIndex].x);
                md.vertices.push_back(vertDatas[vIndex].y);
                md.vertices.push_back(vertDatas[vIndex].z);
            }else{
                md.vertices.push_back(0.0f);
                md.vertices.push_back(0.0f);
                md.vertices.push_back(0.0f);
            }
            if(vtIndex >= 0){
                md.coords.push_back(coordDatas[vtIndex].x);
                md.coords.push_back(coordDatas[vtIndex].y);
            }else{
                md.coords.push_back(0.0f);
                md.coords.push_back(0.0f);
            }
            if(vnIndex >= 0){
                md.normals.push_back(normalDatas[vnIndex].x);
                md.normals.push_back(normalDatas[vnIndex].y);
                md.normals.push_back(normalDatas[vnIndex].z);
            }else{
                md.normals.push_back(0.0f);
                md.normals.push_back(0.0f);
                md.normals.push_back(0.0f);
            }
            diffIndex++;
        }else {
            ret = tg.first->second;
        }
        return ret;
    };

    size_t pos = 0,len = data.size();
    while(pos < len){
        size_t end = data.find('\n',pos);
        if(end == std::string_view::npos)end = len;
        std::string_view line = data.substr(pos,end - pos);
        pos = end + 1;

        //比较起始token 
        if(line.starts_with("#")){
            //忽略注释
        }else if(line.starts_with("mtllib")){
            //暂时忽略mesh信息
        }else if(line.starts_with("usemtl")){
            if(line.size() > 7){
                std::string str = "";
                str += line.substr(7);
                alib::g3::Util::str_trim_nrt(str);
                md.meshes.push_back({vertex,coord,index,normal,Material(),str});
            }
        }else if(line.starts_with("o")){
            //分部名字
        }else if(line.starts_with("v ")){ //顶点
            glm::vec3 & data = vertDatas.emplace_back(); 
            sscanf(line.data(),"v %f %f %f",&data.x,&data.y,&data.z);
        }else if(line.starts_with("vn")){ //法向量
            glm::vec3 & data = normalDatas.emplace_back();
            sscanf(line.data(),"vn %f %f %f",&data.x,&data.y,&data.z);
        }else if(line.starts_with("vt")){ //纹理坐标
            glm::vec2 &data = coordDatas.emplace_back();
            sscanf(line.data(),"vt %f %f",&data.x,&data.y);
        }else if(line.starts_with("f ")){ //索引信息
            index++;
            //目前暂时处理三角形，四边形后面讲
            int v = 0, vt = 0, vn = 0;
            int vertCount = 0;
            //手动解析
            const char * endptr = line.data() + 2; //跳过f和空格
            for(int i = 0; i < 3 && endptr-line.data() < line.size(); i++){
                static auto readNew = [&]()->int{
                    const char * oldEnd = endptr;
                    int ret = strtol(endptr, (char**)(&endptr), 10);
                    while(*endptr == ' ' || *endptr == '/' || *endptr == '\0') endptr++;
                    return (endptr - oldEnd > 0) ? ret : 0; // obj起始为1，所以如果endptr没有移动就返回0
                };
                v = readNew();
                if(*endptr == '\0')goto Summary; //如果到末尾了就跳
                vt = readNew();
                if(*endptr == '\0')goto Summary; //如果到末尾了就跳
                vn = readNew();
                if(*endptr == '\0')goto Summary; //如果到末尾了就跳
            Summary:
                vertCount++;
                md.indices.push_back(vert_auto(v - 1, vt - 1, vn - 1));
            }
            //暂时只处理三角形
            if(vertCount == 4){

            }
        }else if(line.starts_with("s")){ //平滑信息
            //暂时忽略
        }
    }
    std::cout << md.vertices.size() / 3 << " vertices, "
              << md.normals.size() / 3 << " normals, "
              << md.coords.size() / 2 << " coords, "
              << md.indices.size() << " indices, "
              << md.meshes.size() << " meshes." 
              << vertDatas.size() << " unique vertices, "
              << normalDatas.size() << " unique normals, "
              << coordDatas.size() << " unique coords."
              << data.size() << " bytes of data."
              << std::endl;
    std::cout << "Total unique vertices: " << diffIndex << std::endl;
}