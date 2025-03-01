#ifndef ALIB4_AUTIL_HPP_INCLUDED
#define ALIB4_AUTIL_HPP_INCLUDED
#include <alib-g4/autil.h>
#include <memory_resource>
#include <string>
#include <stdarg.h>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

extern "C"{
	ALIB4_API int aformatStringf(std::string& tg,const char * fmt,...);
	ALIB4_API int aformatStringf_pmr(std::pmr::string & pmrtg,const char * fmt,...);
	ALIB4_API int avformatStringf(std::string& tg,const char * fmt,va_list ap);
	ALIB4_API int avformatStringf_pmr(std::pmr::string & pmrtg,const char * fmt,va_list ap);

	ALIB4_API int aio_traverseImpl(const fs::path& basePath,std::vector<std::string>& results,int remainingDepth,const fs::path& currentAppender,bool includeFiles,bool includeDirs);

    ALIB4_API int aio_traverseFilesDirs(const std::string& path,std::vector<std::string>& files,int traverseDepth = -1,const std::string& appender = "");

    ///content in files automatically contacted with path,usually absoulute
    ALIB4_API int aio_traverseFilesOnly(const std::string& path,std::vector<std::string>& files,int traverseDepth = -1,const std::string& appender = "");

    ALIB4_API int aio_traverseFolders(const std::string& path,std::vector<std::string>& folders,int traverseDepth = -1,const std::string& appender = "");

    ALIB4_API int aio_traverseFilesRecursive(const std::string& path,std::vector<std::string>& files,const std::string& appender = "");

    ALIB4_API int aio_traverseFoldersRecursive(const std::string& path,std::vector<std::string>& folders,const std::string& appender = "");

}

#endif
