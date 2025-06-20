#include <alib-g3/aclock.h>
#include <cstdlib>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <sys/unistd.h>
#include <sys/time.h>
#elif __linux__
#include <time.h>
#include <unistd.h>
#endif // __linux__

using namespace alib::g3;

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
    this->state = Stopped;
    if(start){
        this->start();
    }
}

Clock::ClockState Clock::getState(){
    return state;
}

void Clock::start(){
    if(state != Stopped)return;
    state = Running;
    this->m_StartTime = AssertSt;
    clearOffset();
}

ClockTimeInfo Clock::pause(){
    if(state == Paused)return {0,0};
    ClockTimeInfo ret = now();
    m_pauseGained = ret.all;
    state = Paused;//use this to change stop() 's behavior
    stop();
    state = Paused;
    return ret;
}

void Clock::resume(){
    if(state != Paused)return;
    start();
    state = Running;
}

void Clock::reset(){
    stop();
    start();
}

ClockTimeInfo Clock::now(){
    ClockTimeInfo t;
    t.all = AssertSt - this->m_StartTime + m_pauseGained;
    if(state == Running)t.offset = AssertSt - this->m_PreTime;
    else t.offset = 0;
    return t;
}

double Clock::getAllTime(){
    return now().all;
}

//现在getoffset不会清零！
double Clock::getOffset(){
    return now().offset;
}

void Clock::clearOffset(){
    this->m_PreTime = AssertSt;
}

ClockTimeInfo Clock::stop(){
    if(state == Stopped)return {0,0};
    ClockTimeInfo rt = now();
    this->m_StartTime = 0;
	this->m_PreTime = 0;// @change @critical 2025-3-31 changed(added) here
    if(state != Paused){
        m_pauseGained = 0;
    }
    state = Stopped;
    return rt;
}

Trigger::Trigger(Clock & clk,double d){
    m_clock = &clk;
    duration = d;
    recordedTime = clk.getAllTime();
}

bool Trigger::test(bool v){
    if(duration <= 0)return true;
    bool ret = (m_clock->getAllTime() - recordedTime) >= duration;
    if(v && ret){
        reset();
    }
    return ret;
}

void Trigger::setClock(Clock & c){m_clock = &c;}

void Trigger::reset(){
    recordedTime = m_clock->getAllTime();
}

void Trigger::setDuration(double duration){
    this->duration = duration;
}

RateLimiter::RateLimiter(float wantFps):trig(clk,0){
    desire = 1000 / wantFps;
    trig.setDuration(desire);
}

void RateLimiter::wait(){
    if(desire <= 0)return;
    if(trig.test())return;
    std::this_thread::sleep_for(std::chrono::milliseconds((int)((desire - (clk.getAllTime() - trig.recordedTime)))));
    trig.reset();
}

void RateLimiter::reset(float wantFps){
    desire = 1000 / wantFps;
    trig.reset();
    trig.setDuration(desire);
}
