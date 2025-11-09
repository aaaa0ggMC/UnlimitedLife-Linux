#include <miniaudio.h>
struct ImplSoundEngine{
    ma_engine engine;
    bool inited { false };

    bool setup(){
        if(inited)return true;
        inited = true;
        ma_engine_config cfg;
        /// some settings

        return ma_engine_init(&cfg,&engine) == MA_SUCCESS;
    }

    bool cleanup(){
        if(inited){
            ma_engine_uninit(&engine);
        }
        return true;
    }

    ~ImplSoundEngine(){cleanup();}
};

#define AGE_IMPL_SOUNDENGINE
#include <AGE/Audio/SoundEngine.h>

using namespace age::audio;

SoundEngine global_engine;

SoundEngine& SoundEngine::get_global(){
    return global_engine;
}

SoundEngine::SoundEngine(){
    m_impl = new ImplSoundEngine();
}

SoundEngine::~SoundEngine(){
    delete m_impl;
}