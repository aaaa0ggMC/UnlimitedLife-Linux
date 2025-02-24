#include <alib-g4/autil.h>
#include <alib-g4/ahandle.h>
#include <cstdio>
#include <iostream>

int main(void){
	using namespace std;
	astr_get(11000);
	cout << agetLastError().code << " " << agetLastError().content << endl;
	return 0;
}
