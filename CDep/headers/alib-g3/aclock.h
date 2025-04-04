/** @file aclock.h
* @brief 与计时有关的函数与类
* @author aaaa0ggmc
* @date 2025-3-31
* @version 3.1
* @copyright Copyright(C)2025
********************************
@par 修改日志:
<table>
<tr><th>时间       <th>版本         <th>作者          <th>介绍        
<tr><td>2025-3-31 <td>3.1          <th>aaaa0ggmc    <td>添加doc
</table>
********************************
*/
#ifndef ACCLOCK_H_INCLUDED
#define ACCLOCK_H_INCLUDED
#include <time.h>
#include <alib-g3/autil.h>

#ifdef __cplusplus
extern "C"
{
#endif
namespace alib{
namespace g3{
    /** @struct ClockTimeInfo
	 *  @brief Clock返回的时间信息
	 *  @see Clock::now() Clock::getAllTime() Clock::getOffset()
     */
    struct DLL_EXPORT ClockTimeInfo{
        double all;///<毫秒·，从clock启动到获取时的所有时间
        double offset;///<毫秒，到上次clearOffset的时间
    };

	/** @class Clock
	 *	@brief 核心计时类，使用clock_gettime高精度计时
	 *	@note  为了跨平台抛弃了windows的performancecounter
	 */
    class DLL_EXPORT Clock{
    public:
		/** @enum ClockState
		 *	@brief 时钟状态
		 */
        enum ClockState{
            Running,///<运行中
            Paused,///<已暂停
            Stopped///<已经停止
        };

        /** @brief 构造函数，默认启动时钟
        *   @param[in] start 是否立马启动时钟，默认为true
        */
        Clock(bool start = true);
        /** @brief 启动时钟
        *   @note 如果时钟已经启动了，那么就什么也不做
		*@par 例子:
		*@code
		* Clock clock(false);
		* ...
		* clock.start();
		*@endcode
        */
        void start();
        /** @brief 停止时钟（不是暂停），会清空内部的offset以及初始时间
        *   @note 如果时钟已经停止了，那么就什么也不做
        *   @return 目前的时间信息
        */
        ClockTimeInfo stop();
        /** @brief 暂停时钟
        *  	@return 当前时间信息
        */
        ClockTimeInfo pause();
        /** @brief 恢复时钟（与pause配对)
        */
        void resume();
        /** @brief 重置时钟
        *   @note reset() = stop() + start()
        */
        void reset();
        /** @brief 获取当前时钟状态
		 *  @return 状态，为 enum ClockState
		 */
        ClockState getState();
        /** @brief 获取从初始化到现在的所有时间
        *   @return 毫秒数
        */
        double getAllTime();
        /** @brief 获取时间的偏移量（自从上次clearOffset）
		 *  @return 毫秒数
		 */
        double getOffset();
        /** @brief 获取当前时间信息、
		 *  @return ClockTimeInfo结构体的时间信息
		 */
        ClockTimeInfo now();
        /** @brief 清除时间偏移量
		 *@par 例子:
		 *@code
		 *Clock clk;
		 *...//过了102ms
		 *out(clk.getOffset());
		 *...//过了100ms
		 *out(clk.getOffset());
		 *clk.clearOffset();
		 *...//过了114ms
		 *out(clk.getOffset());
		 *@endcode
		 *@par 输出:
		 *@code
		 *102 202 114
		 *@endcode
		 */
        void clearOffset();
    private:
        double m_pauseGained;///<暂停保存的时间
        double m_StartTime;///<开始时间
        double m_PreTime;///<offset之前的时间
        ClockState state;///<时钟状态
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
