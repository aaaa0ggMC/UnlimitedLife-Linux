#include <alib-g3/atranslator.h>
#include <alib-g3/autil.h>
#include <alib-g3/adata.h>
#include <unordered_map>
#include <algorithm>
#include <string.h>

#include <iostream>

using namespace alib::ng;

Translator* Translator::instance;


void Translator::setVerifyToken(dstring verifyUTF8){verifyToken = verifyUTF8;}
void Translator::setAccessToken(dstring accessUTF8){accessToken = accessUTF8;}

int Translator::readTranslationFiles(dstring path,int enc){
    if(!verifyToken){
        return ALIB_TRANSLATION_VERIFY_TOKEN_NOT_SET;
    }else if(!accessToken) return ALIB_TRANSLATION_ACCESS_TOKEN_NOT_SET;
    std::vector<std::string> fs;
    Util::io_traverseFiles(path,fs);
    GDoc doc;
    for(dstring ss : fs){
        std::string tail = ss.substr(ss.find_last_of('.')+1);
        if(tail.compare("json") && tail.compare("toml")){
            continue;
        }
        ///删除之前留下来的translation
        doc.clearMapping();
        int ret = 0;
        if(tail.compare("json")){
            ///TOML
            //std::cout << "我读了toml" << std::endl;
            ret = doc.read_parseFileTOML(ss);
        }else ret = doc.read_parseFileJSON(ss);
        if(!ret){
            if(doc.get(*verifyToken)){
                //cout << "I got verify " << *doc.get(*verifyToken) << endl;
                auto token = doc.get(*accessToken);
                if(token && strcmp(*token,"")){
                    ///build transmap
                    TransMap mapping_multi;
                    for(auto&[key,value] : doc.mapping){
                        mstring push_v;
                        string u8_key;
                        switch(enc){
                        case ALIB_ENC_ANSI:
                            push_v.ansi = value;
                            push_v.utf8 = converter::ansi_to_utf8(push_v.ansi);
                            push_v.utf16 = converter::utf8_to_utf16(push_v.utf8);
                            u8_key = converter::ansi_to_utf8(key);
                            break;
                        case ALIB_ENC_UTF16:
                            push_v.utf16 = std::wstring(value.begin(),value.end());
                            push_v.utf8 = converter::utf16_to_utf8(push_v.utf16);
                            push_v.ansi = converter::utf8_to_ansi(push_v.utf8);
                            u8_key = converter::utf16_to_utf8(std::wstring(key.begin(),key.end()));
                            break;
                        case ALIB_ENC_UTF8:
                        default:
                            push_v.utf8 = value;
                            push_v.utf16 = converter::utf8_to_utf16(push_v.utf8);
                            push_v.ansi = converter::utf8_to_ansi(push_v.utf8);
                            u8_key = key;
                        }
                        mapping_multi.emplace(u8_key,push_v);
                    }
                    translations.emplace(*token,mapping_multi);
                }
            }
        }
    }
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
    return ALIB_SUCCESS;
}

std::optional<mstring*> Translator::translate(dstring idx,int enc){
    string id = idx;
    if(enc != ALIB_ENC_UTF8){
        id = converter::ansi_to_utf8(id);
    }
    if(!currentTranslation)return nullopt;
    auto iter = currentTranslation->find(id);
    if(iter == currentTranslation->end())return nullopt;
    return &(iter->second);
}

std::optional<mstring*> Translator::translate_static(dstring id,int enc){
    if(!instance)return nullopt;
    return instance->translate(id,enc);
}

void Translator::setDefaultKey(dstring s){defaultKey = s;}

void Translator::set(Translator * t){
    instance = t;
}

Translator::Translator(bool v,dstring x){
    if(v && (instance == NULL)){
        instance = this;
    }
    defaultKey = x;
    currentTranslation = NULL;
    accessToken = ALIB_DEF_ACCESS;
    verifyToken = ALIB_DEF_VERIFY;
    strBuffer.resize(TEXT_MAX_SIZE);
}

Translator* Translator::get(){return instance;}

void Translator::translate_args_internal(dstring u8_str,string& u8s,va_list va){
    ///strBuffer看起来也不需要clear
    int sz = vsnprintf(strBuffer.data(),TEXT_MAX_SIZE,u8_str.c_str(),va);
    u8s = strBuffer.substr(0,sz);
}

int Translator::translate_args_vlist(dstring idx,int enc,std::string & ss,int encs,va_list ap){
    string id = idx;
    if(enc != ALIB_ENC_UTF8){
        id = converter::ansi_to_utf8(id);
    }
    if(!currentTranslation)return ALIB_TRANSLATION_MISSING_TRANSLATOR;
    auto iter = currentTranslation->find(id);
    if(iter == currentTranslation->end())return ALIB_TRANSLATION_MISSING_KEY;
    translate_args_internal((iter->second).utf8,ss,ap);
    if(enc != ALIB_ENC_UTF8){
        ss = converter::utf8_to_ansi(ss);
    }
    return ALIB_SUCCESS;
}

int Translator::translate_args(dstring id,int e,std::string & ss,int encS,...){
    va_list vl;
    va_start(vl,encS);
    int ret = translate_args_vlist(id,e,ss,encS,vl);
    va_end(vl);
    return ret;
}

int Translator::translate_args(dstring id,int enc,std::wstring * storer,...){
    string content;
    va_list ap;
    va_start(ap,storer);
    int ret = translate_args_vlist(id,enc,content,ALIB_ENC_UTF8,ap);
    if(ret == ALIB_SUCCESS){
        *storer = converter::utf8_to_utf16(content);
    }
    va_end(ap);
    return ret;
}

int Translator::translate_args_vlist(dstring id,int enc,std::wstring * storer,va_list ap){
    string content;
    int ret = translate_args_vlist(id,enc,content,ALIB_ENC_UTF8,ap);
    if(!ret){
        *storer = converter::utf8_to_utf16(content);
    }
    return ret;
}

int Translator::translate_args_static(dstring id,int e,std::string& ss,int encS,...){
    if(!instance)return ALIB_TRANSLATION_MISSING_INSTANCE;
    va_list vl;
    va_start(vl,encS);
    int ret = instance->translate_args_vlist(id,e,ss,encS,vl);
    va_end(vl);
    return ret;
}

int Translator::translate_args_static(dstring id,int enc,std::wstring* ss,...){
    if(!instance)return ALIB_TRANSLATION_MISSING_INSTANCE;
    va_list vl;
    va_start(vl,ss);
    string content;
    int ret = instance->translate_args_vlist(id,enc,content,ALIB_ENC_UTF8,vl);
    if(!ret){
        *ss = converter::utf8_to_utf16(content);
    }
    va_end(vl);
    return ret;
}


dstring Translator::translate_def(dstring id,dstring def,int enc){
    auto opt = translate(id,enc);
    if(opt){
        if(enc == ALIB_ENC_UTF8)return (*opt)->utf8;
        else return (*opt)->ansi;
    }
    return def;
}

std::string Translator::translate_args_def(dstring id,dstring def,int enc,...){
    string data = "";
    va_list ap;
    va_start(ap,enc);
    int ret = translate_args(id,enc,data,ALIB_ENC_UTF8,ap);
    if(ret){
        std::string u8s;
        if(enc == ALIB_ENC_UTF8){
            u8s = def;
        }else u8s = converter::ansi_to_utf8(def);
        translate_args_internal(u8s,data,ap);
    }
    va_end(ap);
    return data;
}
