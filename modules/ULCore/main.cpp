#include <iostream>
#include <ul/ULServer.h>
#include <ul/ULSimpleRenderer.h>
#include <ul/base/requests.h>
#include <ul/base/RequestManager.h>

struct Task : public ul::IRequest{
    
    int idx;

    Task(int i){
        idx = i;
    }

    void work(uint64_t tid) noexcept override {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        //std::cout << "There is something inside you. * " << idx << " with tid:" << tid << std::endl;
    }

    ul::IRequest* copy() const override{
        Task *tsk = new Task(idx);
        return (ul::IRequest*)tsk;
    }
};

int main(void){
    //// Initializing Server & Simple Renderer(for test...) ////
    ul::ULServer server;
    ul::ULSimpleRenderer renderer;

    //// TODO: Prepare Data ////

    //// Launching Server & Renderer ////
    server.launch();

    //// Directly Use Server's Data ////
    renderer.bindServer(&server);
    renderer.launch();

    ul::RequestList rl;

    rl.push(ul::Request<int>{1,{2}});
    auto r = rl.pop<int>();
    rl.releaseRequest(r);

    ul::RequestManager mgr(5);

    for(int i = 0;i < 1000;++i){
        mgr.push(Task(i));
    }

    std::string a;
    std::cin >> a;

    for(int i = 0;i < 1000;++i){
        mgr.push(Task(i));
    }

    std::cin >> a;

    return 0;
}