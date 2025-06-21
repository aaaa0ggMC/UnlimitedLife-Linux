#include <iostream>
#include <AMC/AMC.h>
#include <AGE/Application.h>

using namespace std;

// 复用AGE代码
class AppManager: public amc::Manager {
    public:
    age::Application app;
    AppManager(){
        std::cout << 'h'<< std::endl;
    }
};

class UnlimitedLife: public amc::Applicatioin {
    
};

int main(){
    cout << "Hello world!" << endl;
    return 0;
}
