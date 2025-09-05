#ifndef UL_BASE_RM_H
#define UL_BASE_RM_H
#include <queue>
#include <thread>
#include <vector>
#include <unordered_map> 
#include <atomic>

#ifndef UL_USE_NO_LOCK /// Note that this will silently break ABI!!!
#include <mutex>
#include <condition_variable>
#endif

constexpr int pause_interval = 20;
constexpr int wait_interval = 5;
constexpr int termin_wait_sec = 5;

namespace ul{

    struct IRequest{
        virtual void work(uint64_t tid) noexcept = 0;
        virtual IRequest* copy() const = 0;
        // virtual IRequest* free() = 0; 这个结合进析构函数
        inline virtual ~IRequest(){}
    };

    template<class T> concept IsARequest = std::derived_from<T,IRequest>;

    // 后面会加入锁，现在先不管
    struct RequestManager{
        enum RunningState{
            Run,
            Pause,
            Stop
        };
    private:
        uint64_t id_max;
        std::queue<IRequest*> tasks;
        std::unordered_map<uint64_t,std::thread> threads;
        std::atomic<RunningState> running { Run };
        #ifndef UL_USE_NO_LOCK
        std::mutex mutex;
        std::condition_variable cv;
        std::mutex cv_mutex;
        #endif

    public:

        void appendThreads(unsigned int numOfThreads){
            for(int i = 0;i < numOfThreads;++i){
                auto newid = ++id_max;
                threads.emplace(newid,std::thread(worker,id_max,this,&running));
            }// 缩小数量算法比较复杂同时实际用处不大，所以这里不处理
        }

        inline static void worker(uint64_t id,RequestManager * rm,std::atomic<RunningState> * runningState){
            auto next = [&rm]{
                return rm->pop();
            };
            RunningState state;
            while((state = runningState->load()) != Stop){
                switch(state){
                case Pause:
                    std::this_thread::sleep_for(std::chrono::milliseconds(pause_interval));
                    continue;
                // WIP More States
                }
                IRequest * req = next();
                if(req){
                    req->work(id);
                    delete req;
                }else{
                    #ifndef UL_USE_NO_LOCK
                    //std::cout << "time to sleep." << std::endl;
                    bool empty = true;
                    std::unique_lock<std::mutex> lock(rm->mutex);
                    while(true){
                        if(!rm->tasks.empty() || rm->running.load() == Stop)break;
                        rm->cv.wait(lock);
                    }
                    #else 
                    std::this_thread::sleep_for(std::chrono::milliseconds(pause_interval));
                    #endif
                    //std::cout << "i wake up." << std::endl;
                }
            }
        }

        inline RequestManager(unsigned int numOfThreads = 2){
            id_max = 0;
            appendThreads(numOfThreads);
        }

        inline ~RequestManager(){
            int waited = 0;
            while(!tasks.empty()){
                waited += pause_interval;
                std::this_thread::sleep_for(std::chrono::milliseconds(pause_interval));
                if(waited >= termin_wait_sec * 1000){
                    break;
                }
            }
            running.store(RunningState::Stop);
            cv.notify_all();
            for(auto & t : threads){
                if((t.second).joinable())(t.second).join();
            }
            {
                #ifndef UL_USE_NO_LOCK
                std::lock_guard<std::mutex> lock(mutex);
                #endif
                while(!tasks.empty()){
                    IRequest * req = tasks.front();
                    delete req;
                    tasks.pop();
                }
            }
        }

        template<IsARequest T> inline void push(const T& req){
            #ifndef UL_USE_NO_LOCK
            std::lock_guard<std::mutex> lock(mutex);
            #endif
            if(tasks.empty()){
                cv.notify_all();
            }
            tasks.push(req.copy());
        }

        IRequest* pop(){
            #ifndef UL_USE_NO_LOCK
            std::lock_guard<std::mutex> lock(mutex);
            #endif
            if(tasks.empty())return nullptr;
            auto t = tasks.front();
            tasks.pop();
            return t;
        }
    };
}

#endif