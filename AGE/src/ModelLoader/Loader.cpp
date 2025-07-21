#include <AGE/ModelLoader/Loader.h>
#include <sstream>
#include <vector>
#include <glm/glm.hpp>
#include <alib-g3/autil.h>

using namespace age::model::fmt;
using namespace age::model;
using namespace age;

void Stl::parse(std::string_view data,ModelData & md){
    
}

void Obj::parse(std::string_view data,ModelData & md){
    std::string token;
    size_t vertex = 0,coord = 0,index = 0,normal = 0;
    std::vector<glm::vec3> vertDatas;
    std::vector<glm::vec3> normalDatas;
    std::vector<glm::vec2> coordDatas;
    
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
        }else if(line.starts_with("v")){
            glm::vec3 & data = vertDatas.emplace_back(); 
            sscanf(line.data(),"v %f %f %f",&data.x,&data.y,&data.z);
        }else if(line.starts_with("vn")){ //法向量
            glm::vec3 & data = normalDatas.emplace_back();
            sscanf(line.data(),"vn %f %f %f",&data.x,&data.y,&data.z);
        }else if(line.starts_with("vt")){ //纹理坐标
            glm::vec2 &data = coordDatas.emplace_back();
            sscanf(line.data(),"vt %f %f",&data.x,&data.y);
        }else if(line.starts_with("f")){ //索引信息
            index++;
            //目前暂时处理三角形，四边形后面讲
        }else if(line.starts_with("s")){ //平滑信息

        }
    }
}