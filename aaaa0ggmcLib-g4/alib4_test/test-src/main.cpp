#include <alib-g4/autil.h>
#include <alib-g4/ahandle.hpp>
#include <cstdio>
#include <iostream>

int main(void){
	using namespace std;
	aaddOnErrorCallback(adefaultErrorCallback,NULL);
	alib4::StrHandle hd = 1;
	cout << **hd << endl;
	return 0;
}
