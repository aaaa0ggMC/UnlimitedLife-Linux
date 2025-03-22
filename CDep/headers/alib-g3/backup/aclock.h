#ifndef ACCLOCK_H_INCLUDED
#define ACCLOCK_H_INCLUDED
#include <time.h>
#include <alib-g3/autil.h>

#ifdef __cplusplus
extern "C"
{
#endif
///这么写是因为Codeblocks识别不了alib::ng
namespace alib{
namespace g3{
    /** \brief time
     *  all : millisec time since launched
     *  offset : millisec time since last recorded "bookmark"
     */
    struct DLL_EXPORT ClockTimeInfo{
        double all;
        double offset;
    };

    class DLL_EXPORT Clock{
    public:
        enum ClockState{
            Running,
            Paused,
            Stopped
        };

        /** \brief constructor
        * \param start the clock right now?
        */
        Clock(bool start = true);
        /** \brief start the clock
        * does nothing if already started
        */
        void start();
        /** \brief stop the clock
        * does nothing if already stopped
        * \return recorded time
        */
        ClockTimeInfo stop();
        /** \brief pause the clock
        * ...
        * \return recorded time
        */
        ClockTimeInfo pause();
        /** \brief resume the clock
        */
        void resume();
        /** \brief reset the clock
        *
        * = stop() + start()
        */
        void reset();

        /** \brief get current state*/
        ClockState getState();

        /** \brief get milliseconds since started
        *
        * \return milliseconds
        */
        double getAllTime();
        /** \brief get offset*/
        double getOffset();
        /** \brief composed info:alltime + offset*/
        ClockTimeInfo now();

        /** \brief clear offset */
        void clearOffset();
    private:
        double m_pauseGained;
        double m_StartTime;
        double m_PreTime;
        ClockState state;
    };

    struct DLL_EXPORT Trigger{
    public:
        friend struct RPSRestrict;
        Trigger(Clock& clock,double duration);
        /** \brief test the trigger,does nothing if no clock*/
        bool test(bool resetIfSucceeds = true);
        /** \brief reset the trigger*/
        void reset();
        /** \brief set duration */
        void setDuration(double duration);
        /** \brief set clock */
        void setClock(Clock & clock);
        /**< you can also set the duration manually :) */
        double duration;
        double recordedTime;
    private:
        Clock * m_clock;
    };


    ///RateLimiter Not just for limiting fps
    struct DLL_EXPORT RateLimiter {
        Clock clk;
        float desire;
        Trigger trig;

        RateLimiter(float wantFps);

       /** \brief Wait until enough time has passed to achieve the desired rate
         * Sleeps or blocks execution to maintain a steady rate (e.g., frames per second).
         */
        void wait();

        void reset(float wantFps);
    };
}
}

#ifdef __cplusplus
}
#endif

#endif // ACCLOCK_H_INCLUDED
