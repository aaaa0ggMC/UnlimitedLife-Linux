#include <AGE/Utils.h>
#include <iostream>

using namespace age;
using namespace std;

int main(){
	EventLoop loop;
	loop.setInterval([](){
		cout << "hello world" << endl;
	}, 1000);
	loop.start();
}
