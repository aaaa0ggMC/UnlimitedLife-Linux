#include <alib-g4/autil.hpp>
#include <iostream>

extern "C" {
	void hello(){
		std::cout << ALIB4_COLOR(red,) << "Hello World!" << std::endl;
	}
}