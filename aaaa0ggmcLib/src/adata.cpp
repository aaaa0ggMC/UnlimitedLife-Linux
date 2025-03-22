#include <alib-g3/adata.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/allocators.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <toml.hpp>
#include <iostream>

using namespace alib::g3;
using namespace std;

const string Analyser::empty_ret = "";

class JSONAnalyser : public Analyser{
public:
    rapidjson::Document document;

    void m_mapDocument(const rapidjson::Value& value,dstring node = "") {
        if(value.IsObject()){
            for(auto it = value.MemberBegin(); it != value.MemberEnd(); ++it){
                string nodepp;
                if(node.compare(""))nodepp = node + "." + (it->name.GetString());
                else nodepp = (it->name.GetString());
                m_mapDocument(it->value,nodepp);
            }
        }else if (value.IsArray()) {
            for(rapidjson::SizeType i = 0; i < value.Size(); ++i){
                string nodepp;
                if(node.compare(""))nodepp = node + "." + to_string(i);
                else nodepp = to_string(i);
                m_mapDocument(value[i],nodepp);
            }
        }else if (value.IsString()){
            mapping.emplace(node,value.GetString());
        }else if (value.IsInt()){
            mapping.emplace(node,to_string(value.GetInt()));
        }else if (value.IsDouble()){
            mapping.emplace(node,to_string(value.GetDouble()));
        }else if (value.IsBool()){
            mapping.emplace(node,(value.GetBool() ? "1" : "0"));
        }//else if (value.IsNull()){}
    }

    void mapDocument() override{
        if(document.IsNull())return;
        m_mapDocument(document);
    }

    int parseString(dstring data) override{
        document.Parse(data.c_str());
        if(document.HasParseError())return AE_HAS_PARSE_ERROR;
        return AE_SUCCESS;
    }

    dstring getConst(dstring key) override{
        auto it = mapping.find(key);
        return it->second;
    }

    JSONAnalyser(mapping_tp &mtp) : Analyser(mtp){

    }
};

class TOMLAnalyser : public Analyser{
public:
    toml::table tml;

    void m_mapDocument(const toml::node & nd,dstring node = "") {
        if(nd.is_table()){
            for(auto it : *(nd.as_table())){
                string nodepp = node + (node.compare("") ? "." : "") + it.first.data();
                m_mapDocument(it.second,nodepp);
            }
        }else if(nd.is_array()){
            toml::array arr = *(nd.as_array());
            for(unsigned int i = 0;i < arr.size();++i){
                string nodepp = node + (node.compare("") ? "." : "") + to_string(i);
                m_mapDocument(arr[i],nodepp);
            }
        }else if(nd.is_boolean()){
            bool v = nd.as_boolean()->get();
            mapping.emplace(node,to_string(v));
        }else if(nd.is_integer()){
            int64_t v = nd.as_integer()->get();
            mapping.emplace(node,to_string(v));
        }else if(nd.is_string()){
            mapping.emplace(node,nd.as_string()->get());
        }else if(nd.is_floating_point()){
            double v = nd.as_floating_point()->get();
            mapping.emplace(node,to_string(v));
        }else{
            auto vv = nd.as_string();
            if(vv != nullptr){
                mapping.emplace(node,vv->get());
            }
        }
    }

    void mapDocument() override{
        if(tml.empty())return;
        m_mapDocument(tml);
    }

    int parseString(dstring data) override{
        try{
            tml = toml::parse(data);
        }catch(const toml::parse_error& err){
            cout << err << endl;
            return AE_HAS_PARSE_ERROR;
        }
        return AE_SUCCESS;
    }

    dstring getConst(dstring key) override{
        auto it = mapping.find(key);
        return it->second;
    }

    TOMLAnalyser(mapping_tp &mtp) : Analyser(mtp){}
};

std::optional<const char *> GDoc::get(dstring key){
    if(!analyser)return std::nullopt;
    auto it = analyser->mapping.find(key);
    if(it == analyser->mapping.end())return std::nullopt;
    return it->second.c_str();
}

std::string Analyser::getCopy(dstring key){
    return getConst(key);
}

Analyser::~Analyser(){

}

Analyser::Analyser(mapping_tp & mtp) : mapping(mtp){}

void Analyser::mapDocument(){}
dstring Analyser::getConst(dstring){return empty_ret;}
int Analyser::parseString(dstring){return AE_SUCCESS;}

void GDoc::clearMapping(){
    mapping.clear();
}

GDoc::GDoc(){
    analyser = nullptr;
}

GDoc::~GDoc(){
    if(analyser)delete analyser;
}

int GDoc::read_parseStringJSON(dstring data){
    if(analyser){
        delete analyser;
        analyser = nullptr;
    }
    analyser = new JSONAnalyser(mapping);

    int ret = analyser->parseString(data);
    if(ret){
        delete analyser;
        analyser = nullptr;
        return ret;
    }
    ///Mapping all the data
    analyser->mapDocument();

    return AE_SUCCESS;
}

int GDoc::read_parseFileJSON(dstring fp){
    string data = "";
    return Util::io_readAll(fp,data) | read_parseStringJSON(data);
}

int GDoc::read_parseFileTOML(dstring fp){
    string data = "";
    Util::io_readAll(fp,data);
    return read_parseStringTOML(Util::str_trim_rt(data));
}

int GDoc::read_parseStringTOML(dstring data){
    if(analyser){
        delete analyser;
        analyser = nullptr;
    }
    analyser = new TOMLAnalyser(mapping);

    int ret = analyser->parseString(data);
    if(ret){
        delete analyser;
        analyser = nullptr;
        return ret;
    }
    ///Mapping all the data
    analyser->mapDocument();

    return AE_SUCCESS;
}

std::optional<const char*> GDoc::operator [](dstring key){
    return this->get(key);
}

