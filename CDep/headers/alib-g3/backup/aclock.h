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
namespace ng{
    /** \brief time
     *  all : millisec time since launched
     *  offset : millisec time since last recorded "bookmark"
     */
    struct DLL_EXPORT TMST0{
        double all;
        double offset;
    };

    class DLL_EXPORT Clock{
    public:

        /** \brief constructor
        * \param start the clock right now?
        * \param use nano second if support
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
        TMST0 stop();
        /** \brief pause the clock
        * ...
        * \return recorded time
        */
        TMST0 pause();
        /** \brief resume the clock
        */
        void resume();
        /** \brief reset the clock
        *
        * = stop() + start()
        */

        void reset();

        /** \brief get current stats
        *
        * \return is stopped
        */

        bool isStop();

        /** \brief get milliseconds since started
        *
        * \return milliseconds
        */
        double getAllTime();
        /** \brief get offset*/
        double getOffset();
        /** \brief composed info:alltime + offset*/
        TMST0 now();

        /** \brief clear offset */
        void clearOffset();
    private:
        double m_pauseGained;
        double m_StartTime;
        double m_PreTime;
        bool m_start;
        bool m_paused;
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
        double rec;
    private:
        Clock * m_clock;
    };


    ///Run persecond
    struct DLL_EXPORT RPSRestrict{
        Clock clk;
        float desire;
        Trigger trig;

        RPSRestrict(float wantRps);

        void wait();

        void reset(float wantRps);

    };
}
}

#ifdef __cplusplus
}
#endif

#endif // ACCLOCK_H_INCLUDED
