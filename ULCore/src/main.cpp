#include <iostream>
#include <ul/ULServer.h>
#include <ul/ULSimpleRenderer.h>
#include <ul/base/requests.h>

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
    auto r = (ul::Request<int>*)rl.pop();
    rl.releaseRequest(r);

    return 0;
}