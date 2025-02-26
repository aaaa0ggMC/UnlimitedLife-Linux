#include <alib-g4/autil.h>
#include <alib-g4/ahandle.hpp>
#include <cstdio>
#include <iostream>

int main(void){
	using namespace std;
	aaddOnErrorCallback(adefaultErrorCallback,NULL);
	AStrHandle data = astr_allocate("");
	cout << aio_writeAll("test.txt","Hello World!",-1) << endl;
	cout << aio_checkExistence("CMakeCache.txt") << endl;
	return 0;
}
