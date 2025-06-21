#include<vector>
#include<alib-g3/aclock.h>

#ifndef AMC_H
#define AMC_H

namespace amc
{
    class Manager{
        public:
        bool isDestroyed = false;
        virtual void initialize(){}
        virtual void update(){}
        virtual void cleanup(){}

        inline void create(){
            initialize();
            isDestroyed = false;
        }

        inline void destroy(){
            if (!isDestroyed){
                cleanup();
                isDestroyed = true;
            }
        }
        };

    class System{
        public:
        alib::g3::RateLimiter rateLimiter = alib::g3::RateLimiter(60.0f);
        bool isRunning = false;
        bool isDestroyed = false;
        std::vector<Manager> managers = std::vector<Manager>();
        
        virtual void initialize() {}
        virtual void update() {}
        virtual void cleanup() {}

        inline void create() {
            initialize();
            isDestroyed = false;
            loop();
        };

        inline void loop(){
            isRunning = true;
            while (isRunning)
            {
                update();
                for (auto& i : managers){
                    i.update();
                    if (!isRunning) break;
                }
                rateLimiter.wait();
            }
        }

        inline void destroy() {
            if (!isDestroyed){
                for (auto& i : managers){
                    i.cleanup();
                }
                cleanup();
                isDestroyed = true;
                isRunning = false;
            }
        };
    };

    class Applicatioin: public System{
        inline void start(){
            create();
        }

        inline void stop(){
            destroy();
        }
    };
} // namespace amc

#endif