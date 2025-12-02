#ifndef AGE_MSAMPLER_H_INCLUDED
#define AGE_MSAMPLER_H_INCLUDED
#include <AGE/Utils.h>
#include <AGE/Texture.h>
#include <AGE/Details/CSBuffer.h>

namespace age::manager{
    struct AGE_API SamplerManager{
    private:
        /// 采样器表
        std::unordered_map<std::string_view,Sampler> samplers;
        std::unordered_map<std::string_view,SamplerInfo> samplersInfo;
        detail::ConstantStringBuffer& csbuffer;
    public:
        SamplerManager(detail::ConstantStringBuffer& cb):csbuffer{cb}{}
        ~SamplerManager();
        /// 由于sampler属性可以动态调整，所以只需要sid
        std::optional<Sampler> create(std::string_view sid);
        bool destroy(std::string_view sid);
        std::optional<Sampler> get(std::string_view sid);

        inline bool has(std::string_view sid){
            return samplers.find(sid) != samplers.end();
        }

        inline bool destroy(Sampler & sampler){
            return destroy(sampler.sid);
        }
    };
}

#endif