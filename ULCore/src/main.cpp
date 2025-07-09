import ULServer;
import ULSimpleRenderer;

#include <iostream>

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

    return 0;
}