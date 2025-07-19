/**
 * @file im_config.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 对im得各种设置进行包装，我只做我需要的
 * @version 0.1
 * @date 2025/07/19
 * @copyright Copyright(c)2025 aaaa0ggmc
 * @start-date 2025/07/19 
 * @dependency aaaa0ggmcLib
 */
#ifndef IM_CONFIG_H_AA
#define IM_CONFIG_H_AA
#include <alib-g3/adata.h>
#include <GL/glew.h>

struct ImConfig{

    struct CheckBoxInfo{
        std::vector<const char *> str;
        int sindex;

        template<class T,class V> inline V& get(T & t){
            return t[sindex];
        }

        inline CheckBoxInfo(){
            str = {};
            sindex = 0;
        }

        inline const char** data(){
            return str.data();
        }

        inline size_t size(){
            return str.size();
        }

        inline const char * get(int outer = -1){
            int mindex = 0;
            if(outer < 0)mindex = sindex;
            if(mindex < 0 || mindex > size())return "";
            return str[mindex];
        }

        inline int* ref(){
            return &sindex;
        }

        inline int& index(){
            return sindex;
        }
    };

    std::unordered_map<std::string,int> ints;
    std::unordered_map<std::string,float> floats;
    std::unordered_map<std::string,bool> bools;
    std::unordered_map<std::string,CheckBoxInfo> checkboxes;
    
    template<class T> inline T& get(const std::string& s);

    inline CheckBoxInfo& getCheckBox(const std::string& s){
        return checkboxes[s]; 
    }

    /// @warning 要么给字面量，要么char*生命周期足够强
    inline void addCheckBox(const std::string& s,const std::vector<const char*> & buf){
        checkboxes[s].str = buf;
    }
};

template<> inline int& ImConfig::get<int>(const std::string& s){
    return ints[s];
}

template<> inline float& ImConfig::get<float>(const std::string& s){
    return floats[s];
}

template<> inline bool& ImConfig::get<bool>(const std::string& s){
    return bools[s];
}

#endif