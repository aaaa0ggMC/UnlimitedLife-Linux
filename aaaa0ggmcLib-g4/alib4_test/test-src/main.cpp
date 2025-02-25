#include <alib-g4/autil.h>
#include <alib-g4/ahandle.h>
#include <cstdio>
#include <iostream>

int main(void){
	using namespace std;
	aaddOnErrorCallback(adefaultErrorCallback,NULL);
	astr_allocate("1");
	astr_get(22);
	return 0;
}
