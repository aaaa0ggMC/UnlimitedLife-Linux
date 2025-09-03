/**
 * @file Audio.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 音频播放
 * @version 0.1
 * @date 2025/09/03
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/08/31 
 */
#ifndef AGE_H_AUDIO
#define AGE_H_AUDIO
#include <AGE/Utils.h>
#include <string_view>
#include <chrono>
#include <glm/glm.hpp>

/// @NOTE 由于需要sound engine的支持，这个文件内大部分函数不建议使用inline

namespace age::audio{
    enum class Status{
        Playing,
        Stopped,
        Paused,
        Invalid
    };

    inline const char * getStatusText(const audio::Status& sta){
        switch(sta){
        case Status::Playing:
            return "Playing";
        case Status::Stopped:
            return "Stopped";
        case Status::Paused:
            return "Paused";
        }
        return "Invalid";
    }

    struct AGE_API Sound{
        Sound();
        ~Sound();
        Sound(const Sound&);
        /// copy from existing sound object
        Sound& operator=(const Sound&);
        Sound& operator=(Sound&&); // move the sound
        //// Load ////
        bool loadFromFile(std::string_view file_path);
        //// Basic Control ////
        void play(int volume = -1); /// we will use the current volume value if volume < 0
        void stop();
        void pause();
        //// Looping ////
        bool isLooping();
        bool setLooping(bool loop = true);
        //// Speed but pitch went higher ////
        float setPitch(float val);
        float getPitch();
        //// Progress ////
        std::chrono::milliseconds tell();
        std::chrono::milliseconds length();
        std::chrono::milliseconds lengthDynamicCalc();
        std::chrono::milliseconds seek(std::chrono::milliseconds ms);
        audio::Status getStatus();
        //// Volume ////
        /// suggested range [0,100]
        /// @return old value
        unsigned int setVolume(unsigned int volume);
        unsigned int getVolume();
        /// 3D sounds ////
        glm::vec3 setPosition(const glm::vec3 & pos);
        glm::vec3 getPosition();
    private:
        void __trival__init();
        void __try_reset();

        unsigned int volume;
        void * sound;
        bool inited = false;
        std::chrono::milliseconds m_length;
    };
}

#endif