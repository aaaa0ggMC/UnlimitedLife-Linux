#ifndef AGE_H_SOUNDENGINE
#define AGE_H_SOUNDENGINE
#include <AGE/Utils.h>

namespace age::audio{
    struct AGE_API SoundEngine{

        SoundEngine();
        ~SoundEngine();

        static SoundEngine& get_global();

    private:
    #ifdef AGE_IMPL_SOUNDENGINE
        ImplSoundEngine * m_impl;
    #else
        void * m_impl;
    #endif
    };
}

#endif