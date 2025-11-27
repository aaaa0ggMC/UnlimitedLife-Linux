// @author:里挥发
// @brief:游戏main入口
#include <iostream>
#include <AMC/AMC.h>
#include <AGE/Application.h>
#include <AGE/Window.h>

using namespace std;

// @brief:复用age::Application
class AppManager: public amc::Manager {
    public:
    age::Window* window;
    alib::g3::ecs::EntityManager em;
    age::Application app;
    AppManager(): app(em){
       
    }
};

class UnlimitedLife: public amc::Applicatioin {
    
};

int main(){
    cout << "Hello world!" << endl;
    return 0;
}
