#include <alib-g3/aclock.h>
#include <thread>
#include <chrono>
#ifdef _WIN32
#include <sys/unistd.h>
#include <sys/time.h>
#elif __linux__
#include <time.h>
#include <unistd.h>
#endif // __linux__

using namespace alib::ng;

#define _d(x) ((double)(x))
#define AssertSt _d(timeGetTimeEx())

static inline double timeGetTimeEx(){
    static bool inited = false;
    static timespec very_start;
    timespec time;
    if(!inited){
        inited = true;
        clock_gettime(CLOCK_REALTIME,&very_start);
    }
    clock_gettime(CLOCK_REALTIME,&time);
    return  (time.tv_sec - very_start.tv_sec) * 1000 + _d(time.tv_nsec - very_start.tv_nsec) / 1000000;
}


Clock::Clock(bool start){
    this->m_StartTime = this->m_PreTime = 0;
    this->m_pauseGained = 0;
    this->m_start = false;
    this->m_paused = false;
    if(start){
        this->start();
    }
}

bool Clock::isStop(){return !m_start;}

void Clock::start(){
    if(m_start)return;
    this->m_start = true;
    this->m_StartTime = AssertSt;
    clearOffset();
}

TMST0 Clock::pause(){
    if(m_paused)return {0,0};
    m_paused = true;
    TMST0 ret = now();
    m_pauseGained = ret.all;
    stop();
    return ret;
}

void Clock::resume(){
    if(!m_paused)return;
    start();
    clearOffset();
    m_paused = false;
}

TMST0 Clock::now(){
    if(!m_start)return {0,0};
    TMST0 t;
    t.all = AssertSt - this->m_StartTime + m_pauseGained;
    t.offset = AssertSt - this->m_PreTime;
    return t;
}

double Clock::getAllTime(){
    if(!m_start)return 0;
    return now().all;
}

//现在getoffset不会清零！
double Clock::getOffset(){
    if(!m_start)return 0;
    double off = now().offset;
    //this->m_PreTime = AssertSt;
    return off;
}

void Clock::clearOffset(){
    this->m_PreTime = AssertSt;
}

TMST0 Clock::stop(){
    if(!m_start)return {0,0};
    TMST0 rt = now();
    this->m_StartTime = 0;
    this->m_start = false;
    if(!m_paused){
        m_pauseGained = 0;
    }
    return rt;
}

Trigger::Trigger(Clock & clk,double d){
    m_clock = &clk;
    duration = d;
    rec = clk.getAllTime();
}

bool Trigger::test(bool v){
    bool ret = (m_clock->getAllTime() - rec) >= duration;
    if(v && ret){
        reset();
    }
    return ret;
}

void Trigger::setClock(Clock & c){m_clock = &c;}

void Trigger::reset(){
    rec = m_clock->getAllTime();
}

void Trigger::setDuration(double duration){
    this->duration = duration;
}

RPSRestrict::RPSRestrict(float wantFps):trig(clk,0){
    desire = 1000 / wantFps;
    trig.setDuration(desire);
}

void RPSRestrict::wait(){
    if(trig.test())return;
    std::this_thread::sleep_for(std::chrono::milliseconds((int)(desire - (clk.getAllTime() - trig.rec))));
}

void RPSRestrict::reset(float wantFps){
    desire = 1000 / wantFps;
    trig.setDuration(desire);
}
