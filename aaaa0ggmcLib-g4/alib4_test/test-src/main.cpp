#include <alib-g4/autil.hpp>
#include <alib-g4/ahandle.hpp>
#include <cstdio>
#include <iostream>

int main(void){
	using namespace std;
	aaddOnErrorCallback(adefaultErrorCallback,NULL);
	cout << agetTime() << endl;
	return 0;
}
