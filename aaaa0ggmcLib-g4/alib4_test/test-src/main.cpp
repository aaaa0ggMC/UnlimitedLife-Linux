#include <alib-g4/autil.h>
#include <alib-g4/ahandle.hpp>
#include <cstdio>
#include <iostream>

int main(void){
    AHandle ah = alib4::ResourceManager::resManager.allocateString("Hello World!++");
    std::cout << alib4::ResourceManager::resManager.getString(ah) << std::endl;
}
