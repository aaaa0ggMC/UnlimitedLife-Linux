#include <AGE/ModelLoader/Loader.h>
#include <sstream>
#include <vector>
#include <glm/glm.hpp>
#include <alib-g3/autil.h>
#include <unordered_set>
#include <bit>
#include <alib-g3/adata.h>

using namespace age::model::fmt;
using namespace age::model;
using namespace age;

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

/// @note in my computer #3, 23ms to load a 9MB obj file in release mode,78ms in debug mode
void Obj::parse(std::string_view data, ModelData& md,bool flipV,std::string_view fp){
    std::vector<glm::vec3> vertDatas;
    std::vector<glm::vec3> normalDatas;
    std::vector<glm::vec2> coordDatas;
    std::unordered_map<Verts, int> vertSet;
    int diffIndex = 0;

    // 负索引转换函数
    auto fixIndex = [](int idx, int size) -> int{
        if (idx > 0) return idx - 1;
        else if (idx < 0) return size + idx;
        return -1;
    };

    ///预留空间，反正后面会销毁 提升不大，反而占用内存空间
    /*vertDatas.reserve(data.size() / 10);
    normalDatas.reserve(data.size() / 10);
    coordDatas.reserve(data.size() / 10);
    md.vertices.reserve(data.size() / 10 * 3);
    md.normals.reserve(data.size() / 10 * 3);
    md.coords.reserve(data.size() / 10 * 2);*/

    // 构造唯一顶点索引的函数
    auto vert_auto = [
        &flipV,&md,&fixIndex,
        &vertDatas,&normalDatas,&coordDatas,
        &vertSet,&diffIndex
    ](int vIndex, int vtIndex, int vnIndex)->int{
        vIndex = fixIndex(vIndex, (int)vertDatas.size());
        vtIndex = fixIndex(vtIndex, (int)coordDatas.size());
        vnIndex = fixIndex(vnIndex, (int)normalDatas.size());

        Verts v{vIndex, vtIndex, vnIndex};
        auto it = vertSet.find(v);
        if(it != vertSet.end()){
            return it->second;
        }

        // 新顶点，加入
        int ret = diffIndex++;

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
            md.coords.push_back(flipV?(1.0f - coordDatas[vtIndex].y):coordDatas[vtIndex].y); // 翻转V分量
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

        vertSet[v] = ret;
        return ret;
    };

    auto valid = [](char* ch){
        if(ch && *ch && *ch != '\n')return true;
        return false;
    };

    size_t pos = 0, len = data.size();
    char * p,* endptr; // 缓存变量
    while(pos < len){
        size_t end = data.find('\n', pos);
        if(end == std::string_view::npos)end = len;
        std::string_view line = data.substr(pos, end - pos);
        pos = end + 1;

        if(line.empty() || line[0] == '#')continue;
        else if(line.starts_with("mtllib")){
            //待定
        }else if(line.starts_with("usemtl")){
            //待定
        }else if(line.starts_with("s")){
            // 平滑指数，忽略
        }else if(line.starts_with("v ")){
            glm::vec3& v = vertDatas.emplace_back();
            p = (char*)line.data() + 2;
            endptr = p;
            // 解析顶点坐标
            v.x = strtof(p,&endptr);
            p = (*endptr == '\0')? endptr : endptr + 1;
            v.y = strtof(p,&endptr);
            p = (*endptr == '\0')? endptr : endptr + 1;
            v.z = strtof(p,&endptr);
        }else if(line.starts_with("vn")){
            glm::vec3& n = normalDatas.emplace_back();
            p = (char*)line.data() + 2;
            endptr = p;
            // 解析顶点坐标
            n.x = strtof(p,&endptr);
            p = (*endptr == '\0')? endptr : endptr + 1;
            n.y = strtof(p,&endptr);
            p = (*endptr == '\0')? endptr : endptr + 1;
            n.z = strtof(p,&endptr);
        }else if(line.starts_with("vt")){
            glm::vec2& uv = coordDatas.emplace_back();
            p = (char*)line.data() + 2;
            endptr = p;
            // 解析顶点坐标
            uv.x = strtof(p,&endptr);
            p = (*endptr == '\0')? endptr : endptr + 1;
            uv.y = strtof(p,&endptr);
            // 注意有些obj只有u，没有v，默认0
            if(endptr == p) uv.y = 0.0f;
        }else if(line.starts_with("f ")){
            // 解析面
            struct FaceVertex {
                int v = -1, vt = -1, vn = -1;
            };
            FaceVertex faceVerts[4];
            int vertCount = 0;
            size_t eleCount = 0;
            if(line.find("//") != std::string::npos)eleCount = 5;
            else {
                auto ps = line.find('#'); // 防止误判注释，虽然我觉得也没有人会去写注释在f后面
                auto lend = line.begin();
                lend += (ps == std::string::npos)?line.size():ps;
                eleCount = std::count(line.begin(),lend,'/');
                float feleCount = eleCount / 3;
                if(feleCount - (int)feleCount > 0.1){ // 小数都是 0.33 0.25起步的，0.1判断绰绰有余了
                    eleCount = eleCount / 4;
                }else eleCount = eleCount / 3;
            }

            p = (char*)line.data() + 2;
            endptr = p;
            switch(eleCount){
            case 0: // passed lagrange_test_data/bunny
                while (valid(p) && vertCount < 4) {
                    FaceVertex fv = {};
                    int vIdx = 0;

                    vIdx = strtol(p, &endptr, 10); //strtol默认返回0
                    p = (!valid(endptr))? endptr : endptr + 1;
                    
                    fv.v = vIdx;
                    fv.vt = -1;
                    fv.vn = -1;

                    faceVerts[vertCount++] = fv;
                }
                break;
            case 1: // passed lagrange_test_data/avacado(processed bt blender to delete normal infos)
                while (valid(p) && vertCount < 4) {
                    FaceVertex fv = {};
                    int vIdx = 0, vtIdx = 0;

                    vIdx = strtol(p, &endptr, 10); //strtol默认返回0
                    p = !valid(endptr)? endptr : endptr + 1;
                    vtIdx = strtol(p, (&endptr), 10);
                    p = !valid(endptr)? endptr : endptr + 1;

                    fv.v = vIdx;
                    fv.vt = (vtIdx == 0) ? -1 : vtIdx;
                    fv.vn = -1;

                    faceVerts[vertCount++] = fv;
                }
                break;
            case 2:  // passed, lagrange_test_data/avacado
                while (valid(p) && vertCount < 4) {
                    FaceVertex fv = {};
                    int vIdx = 0, vtIdx = 0, vnIdx = 0;

                    vIdx = strtol(p, &endptr, 10); //strtol默认返回0
                    p = !valid(endptr)? endptr : endptr + 1;
                    vtIdx = strtol(p, (&endptr), 10);
                    p = !valid(endptr)? endptr : endptr + 1;
                    vnIdx = strtol(p, (&endptr), 10);
                    p = !valid(endptr)? endptr : endptr + 1;

                    fv.v = vIdx;
                    fv.vt = (vtIdx == 0) ? -1 : vtIdx;
                    fv.vn = (vnIdx == 0) ? -1 : vnIdx;

                    faceVerts[vertCount++] = fv;
                }
                break;
            case 5: // passed, lagrange_test_data/dragon
                while (valid(p) && vertCount < 4) {
                    FaceVertex fv = {};
                    int vIdx = 0, vnIdx = 0;

                    vIdx = strtol(p, &endptr, 10); //strtol默认返回0
                    p = !valid(endptr)? endptr : endptr + 1;
                    p++; //跳过一个 /
                    vnIdx = strtol(p, (&endptr), 10);
                    p = !valid(endptr)? endptr : endptr + 1;

                    fv.v = vIdx;
                    fv.vt = -1;
                    fv.vn = (vnIdx == 0) ? -1 : vnIdx;

                    faceVerts[vertCount++] = fv;
                }
                break;
            }

            if(vertCount == 3){
                for (int i = 0; i < 3; ++i) {
                    md.indices.push_back(vert_auto(faceVerts[i].v, faceVerts[i].vt, faceVerts[i].vn));
                }
            }else if (vertCount == 4){
                // 四边形拆三角形
                md.indices.push_back(vert_auto(faceVerts[0].v, faceVerts[0].vt, faceVerts[0].vn));
                md.indices.push_back(vert_auto(faceVerts[1].v, faceVerts[1].vt, faceVerts[1].vn));
                md.indices.push_back(vert_auto(faceVerts[2].v, faceVerts[2].vt, faceVerts[2].vn));

                md.indices.push_back(vert_auto(faceVerts[0].v, faceVerts[0].vt, faceVerts[0].vn));
                md.indices.push_back(vert_auto(faceVerts[2].v, faceVerts[2].vt, faceVerts[2].vn));
                md.indices.push_back(vert_auto(faceVerts[3].v, faceVerts[3].vt, faceVerts[3].vn));
            }
        }
    }

#ifdef AGE_ML_DEBUG
    {
        std::string out = "\nLoaded OBJ Model:\n";
        if(!fp.empty()){
            out += "File Path: " + std::string(fp) + "\n";
        }
        out += "Vertices: " + std::to_string(md.vertices.size() / 3) + "\n";
        out += "Normals: " + std::to_string(md.normals.size() / 3) + "\n";
        out += "Coords: " + std::to_string(md.coords.size() / 2) + "\n";
        out += "Indices: " + std::to_string(md.indices.size()) + "\n";
        out += "Flip V: " + std::string(flipV ? "true" : "false") + "\n";
        out += "Total Size: " + std::to_string(md.vertices.size() * sizeof(float) +
                                                md.normals.size() * sizeof(float) +
                                                md.coords.size() * sizeof(float) +
                                                md.indices.size() * sizeof(int)) + " bytes\n";  
        out += "Data Size: " + std::to_string(data.size()) + " bytes\n";
        out += "Data: \n" + std::string(data.substr(0, 100)) + "...\n";
        out += "End of OBJ Model";
        Error::def.pushMessage(
           {0, out.c_str(), alib::g3::LogLevel::Debug}
        );
    }
#endif
}

void Stl::parse(std::string_view data,ModelData & md,bool flipV,std::string_view fp){
    char * p = (char*)data.data();
    // 跳过可能的空格，虽然没这个可能
    while(*p != '\0'){
        if(!isspace(*p)){
            break;
        }
        ++p;
    }
    if(std::strncmp(p,"solid",5) == 0){
        // treat it as ascii STL
#ifdef AGE_ML_DEBUG
        Error::def.pushMessage(
            {0, "Detected STL ASCII format", alib::g3::LogLevel::Debug}
        );
#endif
        StlAscii::parse(std::string_view(p),md,flipV,fp);
    }else {
        // stl-bin
#ifdef AGE_ML_DEBUG
        Error::def.pushMessage(
            {0, "Detected STL Binary format", alib::g3::LogLevel::Debug}
        );
#endif
        StlBinary::parse(data,md,flipV,fp);
    }
}

template<class T> static T inline endian_cast(const T & f){
    if constexpr (std::endian::native == std::endian::little){
        return f;
    }else{
        return std::bit_cast<T>(std::byteswap(std::bit_cast<uint32_t>(f)));
    }
} 

void StlAscii::parse(std::string_view data,ModelData & md,bool flipV,std::string_view fp){
    char * p = (char*)data.data();
    char * endptr = p;
    while(*p != '\0'){
        float x = 0, y = 0, z = 0;
        // 跳过空格
        while(*p && isspace(*p)) ++p;
        if(*p == '\0') break; // 到达末尾
        if(!strncmp(p,"solid",5)){
            //New Mesh
            //skip to a new line
            while(*p && *p != '\n') ++p;
            if(*p == '\0') break; // 到达末尾
            ++p; // 跳过换行符
        }else if(!strncmp(p,"endsolid",6)){
            //End Mesh
            //skip to a new line
            while(*p && *p != '\n') ++p;
            if(*p == '\0') break; // 到达末尾
            ++p; // 跳过换行符
        }else if(!strncmp(p,"facet normal",12)){
            // 解析法线
            p += 12; // 跳过"facet normal"
            x = strtof(p, &endptr); // 跳过空格
            p = endptr;
            y = strtof(p, &endptr);
            p = endptr;
            z = strtof(p, &endptr);
            p = endptr;
            md.normals.push_back(x);
            md.normals.push_back(y);
            md.normals.push_back(z);
        }else if(!strncmp(p,"vertex",6)){
            // 解析顶点
            p += 6; // 跳过"vertex"
            x = strtof(p, &endptr); // 跳过空格
            p = endptr;
            y = strtof(p, &endptr);
            p = endptr;
            z = strtof(p, &endptr);
            p = endptr;
            md.vertices.push_back(x);
            md.vertices.push_back(y);
            md.vertices.push_back(z);
            md.indices.push_back(md.vertices.size() / 3 - 1); // 索引
        }// 其余的符号忽略
        //跳转到下一行
        while(*p && *p != '\n') ++p;
        if(*p == '\0') break; // 到达末尾
        ++p; // 跳过换行符
    }
#ifdef AGE_ML_DEBUG
    {
        std::string out = "\nLoaded STL ASCII Model:\n";
        if(!fp.empty()){
            out += "File Path: " + std::string(fp) + "\n";
        }
        out += "Vertices: " + std::to_string(md.vertices.size() / 3) + "\n";
        out += "Normals: " + std::to_string(md.normals.size() / 3) + "\n";
        out += "Coords: " + std::to_string(md.coords.size() / 2) + "\n";
        out += "Indices: " + std::to_string(md.indices.size()) + "\n";
        out += "Flip V: " + std::string(flipV ? "true" : "false") + "\n";
        out += "Total Size: " + std::to_string(md.vertices.size() * sizeof(float) +
                                                md.normals.size() * sizeof(float) +
                                                md.coords.size() * sizeof(float) +
                                                md.indices.size() * sizeof(int)) + " bytes\n";  
        out += "Data Size: " + std::to_string(data.size()) + " bytes\n";
        out += "Data: \n" + std::string(data.substr(0, 100)) + "...\n";
        out += "End of STL ASCII Model";
        Error::def.pushMessage(
           {0, out.c_str(), alib::g3::LogLevel::Debug}
        );
    }
#endif
}

// 对stlbin 使用vert_auto进行优化纯属负优化
void StlBinary::parse(std::string_view data,ModelData & md,bool flipV,std::string_view fp){
    if(data.size() < 84){
        return;
    }
    const std::byte * p  = reinterpret_cast<const std::byte*>(data.data());
    p += 80; // 跳过头部80字节
    uint32_t triangleCount = endian_cast(*reinterpret_cast<const uint32_t*>(p));
    p += 4; // 跳过三角形计数

    md.vertices.reserve(triangleCount * 3 * 3);
    md.normals.reserve(triangleCount * 3 * 3);
    md.coords.reserve(triangleCount * 3 * 2);

    for(size_t i = 0; i < triangleCount; ++i){
        float x = 0, y = 0,z =0;
        //normal
        x = endian_cast(*reinterpret_cast<const float*>(p));
        p += sizeof(float);
        y = endian_cast(*reinterpret_cast<const float*>(p));
        p += sizeof(float);
        z = endian_cast(*reinterpret_cast<const float*>(p));
        p += sizeof(float);
        md.normals.push_back(x);
        md.normals.push_back(y);
        md.normals.push_back(z);

        for(int j = 0; j < 3; ++j){
            x = endian_cast(*reinterpret_cast<const float*>(p));
            p += sizeof(float);
            y = endian_cast(*reinterpret_cast<const float*>(p));
            p += sizeof(float);
            z = endian_cast(*reinterpret_cast<const float*>(p));
            p += sizeof(float);
            // 顶点
            md.vertices.push_back(x);
            md.vertices.push_back(y);
            md.vertices.push_back(z);
        }

        md.coords.push_back(0);
        md.coords.push_back(flipV ? 1.0f : 0.0f); // 默认填充为(0,1)或(0,0)
        // 索引
        size_t baseIndex = md.vertices.size() / 3 - 3;
        md.indices.push_back(baseIndex);
        md.indices.push_back(baseIndex + 1);
        md.indices.push_back(baseIndex + 2);

        p += 2; // 跳过属性字节计数（2字节）
    }
#ifdef AGE_ML_DEBUG
    {
        std::string out = "\nLoaded STL Binary Model:\n";
        if(!fp.empty()){
            out += "File Path: " + std::string(fp) + "\n";
        }
        out += "Vertices: " + std::to_string(md.vertices.size() / 3) + "\n";
        out += "Normals: " + std::to_string(md.normals.size() / 3) + "\n";
        out += "Coords: " + std::to_string(md.coords.size() / 2) + "\n";
        out += "Indices: " + std::to_string(md.indices.size()) + "\n";
        out += "Flip V: " + std::string(flipV ? "true" : "false") + "\n";
        out += "Total Size: " + std::to_string(md.vertices.size() * sizeof(float) +
                                                md.normals.size() * sizeof(float) +
                                                md.coords.size() * sizeof(float) +
                                                md.indices.size() * sizeof(int)) + " bytes\n";  
        out += "Data Size: " + std::to_string(data.size()) + " bytes\n";
        out += "End of STL Binary Model";
        Error::def.pushMessage(
           {0, out.c_str(), alib::g3::LogLevel::Debug}
        );
    }
#endif
}

void AutoDetect::parse(std::string_view data, ModelData & md, bool flipV,std::string_view filePath){
    if(data.size() < 6) return; // 数据太小，无法判断
    if(!filePath.empty() && filePath.find(".") != std::string::npos){ //at least have a signal of file extension
        //judging suffix
        if(filePath.ends_with(".obj")){
#ifdef AGE_ML_DEBUG
            Error::def.pushMessage(
                {0, "Detected OBJ format by suffix.", alib::g3::LogLevel::Debug}
            );
#endif
            Obj::parse(data, md, flipV, filePath);
            return;
        }else if(filePath.ends_with(".stl")){
#ifdef AGE_ML_DEBUG
            Error::def.pushMessage(
                {0, "Detected STL format by suffix.", alib::g3::LogLevel::Debug}
            );
#endif
            Stl::parse(data, md, flipV, filePath);
            return;
        }else if(filePath.ends_with(".stla")){
#ifdef AGE_ML_DEBUG
            Error::def.pushMessage(
                {0, "Detected STL ASCII format by suffix.", alib::g3::LogLevel::Debug}
            );
#endif
            StlAscii::parse(data, md, flipV, filePath);
            return;
        }else if(filePath.ends_with(".stlb")){
#ifdef AGE_ML_DEBUG
            Error::def.pushMessage(
                {0, "Detected STL Binary format by suffix.", alib::g3::LogLevel::Debug}
            );
#endif
            StlBinary::parse(data, md, flipV, filePath);
            return;
        }
        //try other methods
    }
    // sample in a small region
    std::string_view sample = data.substr(0,data.size() > 1024 ? 1024 : data.size());
    if(sample.find("solid") != std::string_view::npos || 
       sample.find("facet normal") != std::string_view::npos){
#ifdef AGE_ML_DEBUG
        Error::def.pushMessage(
            {0, "Detected STL ASCII format by the first 1024 bytes.", alib::g3::LogLevel::Debug}
        );
#endif
        StlAscii::parse(data, md, flipV, filePath);
            return;
    }else if(sample.find("usemtl") != std::string_view::npos || 
             sample.find("v ") != std::string_view::npos || 
             sample.find("vn ") != std::string_view::npos || 
             sample.find("vt ") != std::string_view::npos){
#ifdef AGE_ML_DEBUG
        Error::def.pushMessage(
            {0, "Detected OBJ format by the first 1024 bytes.", alib::g3::LogLevel::Debug}
        );
#endif
        Obj::parse(data, md, flipV, filePath);
            return;
    }
    //try file chunk pattern
    uint32_t sus_triangleCount = data.data() + 80 < data.data() + data.size() ? 
        endian_cast(*reinterpret_cast<const uint32_t*>(data.data() + 80)) : 0;
    constexpr size_t chunk_size = 3 * 4 + 3 * 3 * 4 + 2; // 3 vertices, 1 normal, 2 bytes for attributes
    if(sus_triangleCount > 0 && data.size() == 84 + sus_triangleCount * chunk_size + 1){ // 1 seemed for '\0'
        //maybe stl-bin
#ifdef AGE_ML_DEBUG
        Error::def.pushMessage(
            {0, "Detected STL Binary format by file's chunk patterns.", alib::g3::LogLevel::Debug}
        );
#endif
        StlBinary::parse(data, md, flipV, filePath);
        return;
    }

    std::string s = "Unknown format for model data, cannot parse. Data size: " + std::to_string(data.size());
    if(!filePath.empty()){
        s += ", File Path: " + std::string(filePath);
    }
    Error::def.pushMessage(
        {AGEE_FEATURE_NOT_SUPPORTED, s.c_str() , alib::g3::LogLevel::Error}
    );
}