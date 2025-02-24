#include <alib-g4/autil.h>
#include <alib-g4/ahandle.h>
#include <cstdio>
#include <iostream>

int main(void){
	using namespace std;
	AHandle str = astr_allocate("Hello World!");
	cout << astr_get(str) << " Size:" << astr_length(str) << endl; 
	AHandle bs = astr_allocate("World!!!");
	astr_add(str,bs);
	cout << astr_get(str) << " Size:" << astr_length(str) << endl;
	astr_add_ptr(str,"Hola!");
	cout << astr_get(str) << " Size:" << astr_length(str) << endl;
	return 0;
}
