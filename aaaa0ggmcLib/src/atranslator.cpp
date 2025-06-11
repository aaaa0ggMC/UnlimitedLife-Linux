#include <alib-g3/atranslator.h>
#include <alib-g3/autil.h>
#include <alib-g3/adata.h>
#include <unordered_map>
#include <algorithm>
#include <string.h>
#include <iostream>

using namespace alib::g3;

Translator* Translator::instance;

Translator& Translator::setVerifyToken(dstring verifyUTF8){verifyToken = verifyUTF8;return *this;}
Translator& Translator::setAccessToken(dstring accessUTF8){accessToken = accessUTF8;return *this;}
Translator& Translator::setDefaultKey(dstring s){defaultKey = s;return *this;}
const std::string& Translator::getVerifyToken(){return verifyToken;}
const std::string& Translator::getAccessToken(){return accessToken;}
const std::string& Translator::getDefaultKey(){return defaultKey;}
std::optional<Translator> Translator::get(){
    //三元表达式有问题
    if(instance)return *instance;
    return std::nullopt;
}

void Translator::set(Translator * t){
    instance = t;
}

int Translator::readTranslationFiles(dstring path){
    if(!verifyToken.compare("")){
        return ALIB_TRANSLATION_VERIFY_TOKEN_NOT_SET;
    }else if(!accessToken.compare("")) return ALIB_TRANSLATION_ACCESS_TOKEN_NOT_SET;
    std::vector<std::string> fs;

    Util::io_traverseFiles(path,fs,0);
    GDoc doc;
    for(dstring ss : fs){
        std::string tail = ss.substr(ss.find_last_of('.')+1);
        if(tail.compare("json") && tail.compare("toml")){
            continue;
        }
        ///删除之前留下来的translation
        doc.clearMapping();
        if(tail.compare("json"))doc.read_parseFileTOML(ss);
        else doc.read_parseFileJSON(ss);
        if(doc.get(verifyToken)){
            //cout << "I got verify " << *doc.get(*verifyToken) << endl;
            auto token = doc.get(accessToken);
            if(token && strcmp(*token,"")){
                translations.try_emplace(*token,doc.mapping);
            }
        }
    }
    loadTranslation(defaultKey);
    return 0;
}

int Translator::loadTranslation(dstring id){
    currentTranslation = NULL;
    ///使用系统翻译
    if(!id.compare(""))return 0;
    auto iter = translations.find(id);
    if(iter == translations.end()){
        iter = translations.find(defaultKey);
        if(iter == translations.end())
            return ALIB_TRANSLATION_MISSING_DEFAULT;
        else{
            currentTranslation = &(iter->second);
            return ALIB_TRANSLATION_MISSING_EXPECTED;
        }
    }
    currentTranslation = &(iter->second);
    return AE_SUCCESS;
}

const std::string& Translator::translate(dstring idx){
    if(!currentTranslation)return idx;
    auto iter = currentTranslation->find(idx);
    if(iter == currentTranslation->end())return idx;
    return (iter->second);
}

Translator::Translator(dstring defKey,dstring verify,dstring access,bool v){
    if(v && (instance == NULL)){
        instance = this;
    }
    defaultKey = defKey;
    currentTranslation = NULL;
    setVerifyToken(verify);
    setAccessToken(access);
    strBuffer.reserve(ALIB_TRANSLATE_BUFFER_INITIAL_SIZE);
    strBuffer.clear();
}

void Translator::translate_args_internal(dstring u8_str,std::string& u8s,va_list va){
    ///strBuffer看起来也不需要clear
    va_list va3,va4;
    va_copy(va3,va);
    va_copy(va4,va);
    strBuffer.clear();
    strBuffer.resize(strBuffer.capacity());
    int sz = vsnprintf(&(strBuffer[0]),strBuffer.capacity(),u8_str.c_str(),va4);
    va_end(va4);
    if(std::abs(sz) >= strBuffer.capacity()){
        strBuffer.reserve(std::abs(sz) + 1);
        strBuffer.clear();
        sz = vsnprintf(&(strBuffer[0]),strBuffer.capacity(),u8_str.c_str(),va3);
        strBuffer[sz] = '\0';
    }
    va_end(va3);
    //u8s += strBuffer;
    u8s.append(strBuffer,0,sz);
}

int Translator::translate_args_vlist(dstring idx,std::string & ss,va_list ap){
    if(!currentTranslation)return ALIB_TRANSLATION_MISSING_TRANSLATOR;
    auto iter = currentTranslation->find(idx);
    if(iter == currentTranslation->end())return ALIB_TRANSLATION_MISSING_KEY;
    va_list va2;
    va_copy(va2,ap);
    translate_args_internal((iter->second),ss,va2);
    va_end(va2);
    return AE_SUCCESS;
}

std::string& Translator::translate_args(dstring id,std::string & ss,int encS,...){
    va_list vl;
    va_start(vl,encS);
    int ret = translate_args_vlist(id,ss,vl);
    va_end(vl);
    if(ret)ss.append(id);
    return ss;
}

dstring Translator::translate_def(dstring id,dstring def){
    const std::string& opt = translate(id);
    if(!opt.compare(id))return def;
    return opt;
}

std::string& Translator::translate_args_def(dstring id,dstring def,std::string & appender,int enc,...){
    va_list ap;
    va_start(ap,enc);
    int ret = translate_args_vlist(id,appender,ap);
    if(ret){
        translate_args_internal(def,appender,ap);
    }
    va_end(ap);
    return appender;
}
