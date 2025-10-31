#include <miniaudio.h>
#include <AGE/Audio.h>

#define val_eng ((ma_engine*)engine.get())
namespace age{
    namespace audio{
        struct SoundEngineWrapper{
            SoundEngineWrapper();
            ~SoundEngineWrapper();

            inline void * get(){
                return engine;
            }
        private:
            void * engine;
        };
    }
}

using namespace age::audio;
static SoundEngineWrapper engine;

SoundEngineWrapper::SoundEngineWrapper(){
    engine = new ma_engine;
    if(ma_engine_init(nullptr,(ma_engine*)engine) != MA_SUCCESS){
        Error::def.pushMessage({AGEE_FEATURE_NOT_SUPPORTED,"Cannot init sound engine!"});
        delete (ma_engine*)engine;
        engine = nullptr;
    }
}

SoundEngineWrapper::~SoundEngineWrapper(){
    if(engine){
        ma_engine_uninit((ma_engine*)engine);
        // std::print("Uninited engine {}\n",engine);    
        delete (ma_engine*)engine;
    }
}

Sound::Sound(){
    __trival__init();
    sound = new ma_sound;
    inited = false;
}

void Sound::__trival__init(){
    volume = 100;
    m_length = std::chrono::milliseconds(0);
}

Sound::Sound(const Sound& in){
    __trival__init();
    sound = new ma_sound;
    inited = false;
    if(!val_eng)return;
    if(in.inited && ma_sound_init_copy(val_eng,(ma_sound*)in.sound,0,NULL,(ma_sound*)sound) == MA_SUCCESS){
        inited = true;
    }
}

Sound& Sound::operator=(Sound&& snd){
    __try_reset();
    this->inited = snd.inited;
    this->volume = snd.volume;
    this->m_length = snd.m_length;
    delete (ma_sound*)this->sound;
    this->sound = snd.sound;

    snd.__trival__init();
    snd.inited = false;
    snd.sound = new ma_sound;
    return *this;
}

glm::vec3 Sound::setPosition(const glm::vec3 & pos){
    if(!val_eng || !inited)return glm::vec3(0,0,0);
    glm::vec3 old = getPosition();
    ma_sound_set_position((ma_sound*)sound,pos.x,pos.y,pos.z);
    return old;
}

glm::vec3 Sound::getPosition(){
    if(!val_eng || !inited)return glm::vec3(0,0,0);
    auto v = ma_sound_get_position((ma_sound*)sound);
    return glm::vec3(v.x,v.y,v.z);
}

Sound& Sound::operator=(const Sound& in){
    if(!val_eng)return *this;
    if(this == &in)return *this; // 谁会干出自赋值这种操作啊
    __try_reset();
    if(in.inited && ma_sound_init_copy(val_eng,(ma_sound*)in.sound,0,NULL,(ma_sound*)sound) == MA_SUCCESS){
        inited = true;
        m_length = lengthDynamicCalc();
    }   
    return *this;
}

Sound::~Sound(){
    __try_reset();
    delete (ma_sound*)sound;
}

bool Sound::loadFromFile(std::string_view fp){
    if(!val_eng)return false;
    __try_reset();
    if(ma_sound_init_from_file(val_eng,fp.data(),0,NULL,NULL,(ma_sound*)sound) == MA_SUCCESS){
        inited = true;
        ma_sound_set_volume((ma_sound*)sound,volume / 100.f);
        m_length = lengthDynamicCalc();
        return true;
    }
    return true;
}

void Sound::__try_reset(){
    if(inited){
        // std::print("Uninited sound {}\n",sound);
        ma_sound_uninit((ma_sound*)sound);
        m_length = std::chrono::milliseconds(0);
    }
}

unsigned int Sound::setVolume(unsigned int v){
    if(!val_eng || !inited)return volume;
    ma_sound_set_volume((ma_sound*)sound,v);
    unsigned int ret = volume;
    volume = v;
    return volume;
}

unsigned int Sound::getVolume(){
    return volume;
} 

void Sound::play(int volume){
    if(!val_eng || !inited)return;
    if(volume >= 0)setVolume(volume);
    ma_sound_start((ma_sound*)sound);
}

void Sound::stop(){
    if(!val_eng || !inited)return;
    ma_sound_stop((ma_sound*)sound);
    ma_sound_seek_to_second((ma_sound*)sound,0);
}

void Sound::pause(){
    if(!val_eng || !inited)return;
    ma_sound_stop((ma_sound*)sound);
}

std::chrono::milliseconds Sound::tell(){
    if(!val_eng || !inited)return std::chrono::milliseconds(0);
    return std::chrono::milliseconds(ma_sound_get_time_in_milliseconds((ma_sound*)sound));
}

std::chrono::milliseconds Sound::seek(std::chrono::milliseconds ms){
    if(!val_eng || !inited)return std::chrono::milliseconds(0);
    auto old = tell();
    ma_sound_seek_to_second((ma_sound*)sound,ms.count() / 1000.f);
    return old;   
}

std::chrono::milliseconds Sound::lengthDynamicCalc(){
    if(!val_eng || !inited)return std::chrono::milliseconds(0);
    float res = 0;
    if(ma_sound_get_length_in_seconds((ma_sound*)sound,&res) == MA_SUCCESS)return std::chrono::milliseconds((int)(res * 1000 / getPitch()));
    return std::chrono::milliseconds(0);
}

std::chrono::milliseconds Sound::length(){
    return std::chrono::milliseconds((int)(m_length.count() / getPitch()));
}

bool Sound::isLooping(){
    if(!val_eng || !inited)return false;
    return ma_sound_is_looping((ma_sound*)sound);
}

bool Sound::setLooping(bool loop){
    if(!val_eng || !inited)return false;
    bool old = isLooping();
    ma_sound_set_looping((ma_sound*)sound,(ma_bool32)loop);
    return old;
}

float Sound::setPitch(float val){
    if(!val_eng || !inited)return 0;
    float old_val = getPitch();
    ma_sound_set_pitch((ma_sound*)sound,val);
    return old_val;
}

float Sound::getPitch(){
    if(!val_eng || !inited)return 0;
    return ma_sound_get_pitch((ma_sound*)sound); 
}

age::audio::Status Sound::getStatus(){
    if(!val_eng || !inited)return Status::Invalid;
    if(ma_sound_is_playing((ma_sound*)sound)){
        return Status::Playing;
    }else if(tell().count() == 0 || tell() == length()){
        return Status::Stopped;
    }else{
        return Status::Paused;
    }
}