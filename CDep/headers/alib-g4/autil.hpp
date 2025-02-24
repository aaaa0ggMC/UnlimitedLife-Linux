#ifndef ALIB4_AUTIL_HPP_INCLUDED
#define ALIB4_AUTIL_HPP_INCLUDED
#include <alib-g4/autil.h>
#include <memory_resource>
#include <string>
#include <stdarg.h>

namespace alib4{
	int aformatStringf(std::string& tg,const char * fmt,...);
	int aformatStringf(std::pmr::string & pmrtg,const char * fmt,...);
	
	
	int avformatStringf(std::string& tg,const char * fmt,va_list ap);
	int avformatStringf(std::pmr::string & pmrtg,const char * fmt,va_list ap);
}

#endif
