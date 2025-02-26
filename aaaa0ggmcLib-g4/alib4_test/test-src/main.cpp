#include <alib-g4/autil.h>
#include <alib-g4/ahandle.hpp>
#include <cstdio>
#include <iostream>

int main(void){
	using namespace std;
	aaddOnErrorCallback(adefaultErrorCallback,NULL);
	cout << aio_fileSize("CMakeCache.txt") << " " << aio_fileSize("fdssfds.txt") << endl;
	cout << ACP_RED << "Hello World Reddish!" << endl;
	return 0;
}
