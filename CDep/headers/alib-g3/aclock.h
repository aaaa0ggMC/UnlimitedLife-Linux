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
<tr><td>2025-4-04 <td>3.1          <th>aaaa0ggmc    <td>完成doc
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
		 *    Clock clk;
		 *    ...//过了102ms
		 *    out(clk.getOffset());
		 *    ...//过了100ms
		 *    out(clk.getOffset());
		 *    clk.clearOffset();
		 *    ...//过了114ms
		 *    out(clk.getOffset());
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


    /** @struct Trigger
     * @brief 一个简单的触发器，使用test进行时间测试
     * @par 例子:
     * @code
     *  Clock clk;
     *  Trigger tri(clk,100);//100ms触发一次时钟
     *  while(true){
     *      if(tri.test()){//默认自动重置时间
     *          std::cout << "Hello World!" << std::endl;
     *      }
     *  }
     * @endcode
     * 效果是每间隔100ms打印一次 Hello World!
     */
    struct DLL_EXPORT Trigger{
    public:
        friend struct RateLimiter;///<与RateLimiter为友元类方便访问内部数据（其实是RateLimiter需要）
        /** @brief 构造函数
         * @param[in] clock 一个用于绑定的时钟对象
         * @param[in] duration 间隔时间，单位ms
         */
        Trigger(Clock& clock,double duration);
        /** @brief 测试时钟
         * @param[in] resetIfSucceeds 如果到时间了自动重置trigger,默认为true
         * @note m_clock为NULL时什么也不干（不过这个正常情况基本上不可能）
         * @return 是否到时间了
         */
        bool test(bool resetIfSucceeds = true);
        /** @brief 手动重置触发器*/
        void reset();
        /** @brief 设置出发时间
         * @param[in] duration 时长，单位ms
         */
        void setDuration(double duration);
        /** @brief 换一个计时器
         *  @param[in] clock 计时器，需要是引用
         */
        void setClock(Clock & clock);
        /**< you can also set the duration manually :) */
        double duration;///<时长，单位ms
        double recordedTime;///<上次记录下的时间，单位也是ms
    private:
        Clock * m_clock;///<这个不太方便所以不许你manually设置
    };


    /** @struct RateLimiter
     *  @brief 一个简单的速率限制器，可用于限制fps,拥有自己的内部的Clock，使用std::this_thread::sleep_for \n
     *      每次睡眠时间不是固定的，而是会根据到上次睡眠完后的时间进行调整，因此准确度相对来讲很高
     *  @note 说实话我不知道自己怎么设计的,Trigger需要clock但是RateLimiter不需要......
     *  @par 例子:
     * @code
     *  RateLimiter test(60);//我要60帧
     *  while(){
     *      Paint();//绘制什么的
     *      test.wait();//挺聪明的名字，wait,wait,wait...
     *  }
     * @endcode
     */
    struct DLL_EXPORT RateLimiter {
        //这里的命名比较垃圾了......
        Clock clk; ///<内部时钟
        float desire; ///<用户想要的fps,可以为小数（就看系统做不做得到了），这个为睡眠时间
        Trigger trig; ///<RateLimiter内部用的就是trigger,相当于是一个简单的封装

        /** @brief 构造函数
         * @param[in] wantFps 你想要的速率
         * @param[in] highResolution 高精度调整，动态调整
         */
        RateLimiter(float wantFps);

       /** @brief 等到预定时间
        *  @note 每次睡眠时间不是固定的，比如我要一秒钟10fps,一段代码运行了50ms然后wait,那么只会sleep 50ms, \n
        *   要是大于等于100ms直接不睡了return
        */
        void wait();

        /** @brief  重置
         *  @param[in] wantFps 比如你想换一个fps
         */
        void reset(float wantFps);
    };
}
}

#ifdef __cplusplus
}
#endif

#endif // ACCLOCK_H_INCLUDED
