#include <alib-g4/autil.hpp>
#include <alib-g4/ahandle.hpp>
#include <string>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <vector>
#include <iostream>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <psapi.h>
#else
#include <unistd.h>
#include <sys/dir.h>
#include <sys/io.h>
#endif // _WIN32
#include <sys/stat.h>
#include <stdint.h>
#include <dirent.h>

struct AutoFix{
	AutoFix(){
		aenableVirtualTerminal();
	}
	
	~AutoFix(){
		printf(ACP_RESET);
	}
};

static thread_local int ecode = AE_SUCCESS;
static thread_local std::string econtent = "";
static std::vector<aErrorCallbackFn> callbacks = {};
static AutoFix __autofix;

extern "C" {

    void asetLastError(int code,const char * content){
        ecode = code;
        if(content)econtent = content;
		if(ecode != AE_SUCCESS){
			for(auto & tg : callbacks){
				tg(ecode,econtent.c_str(),nullptr);
			}
		}
    }

    aError agetLastError(){
        return {ecode,econtent.c_str()};
    }
	
	void aclearLastError(){
		asetLastError(AE_SUCCESS,"");
    }
	
	
	void asetLastErrorf(int code,const char * fmt,...){
		ecode = code;
		if(fmt){
			va_list val;
			va_start(val,fmt);
			avformatStringf(econtent,fmt,val);
			va_end(val);
		}
		if(ecode != AE_SUCCESS){
			for(auto & tg : callbacks){
				tg(ecode,econtent.c_str(),nullptr);
			}
		}
	}
	
	
	void aaddOnErrorCallback(aErrorCallbackFn fn,void * reserved){
		if(fn)callbacks.push_back(fn);
	}
	
	
	int aformatStringf(std::string& tg,const char * fmt,...){
		if(!fmt){
			return AE_EMPTY_DATA;
		}
		va_list val;
		va_start(val,fmt);
		int ret = avformatStringf(tg,fmt,val);
		va_end(val);
		return ret;
	}

	int aformatStringf_pmr(std::pmr::string & tg,const char * fmt,...){
		if(!fmt){
			return AE_EMPTY_DATA;
		}
		va_list val;
		va_start(val,fmt);
		int ret = avformatStringf_pmr(tg,fmt,val);
		va_end(val);
		return ret;
	}


	int avformatStringf(std::string& tg,const char * fmt,va_list ap){
		va_list cpy;
		va_copy(cpy,ap);
		tg.clear();
		tg.resize(tg.capacity());
		int ret = vsnprintf(&tg[0],tg.size()-1,fmt,ap);
		if(abs(ret) >= tg.size()){
			tg.clear();
			tg.resize(abs(ret) + 1);
			ret = vsnprintf(&tg[0],tg.size()-1,fmt,cpy);
		}
		va_end(cpy);
		return ret;
	}

	int avformatStringf_pmr(std::pmr::string & tg,const char * fmt,va_list ap){
		va_list cpy;
		va_copy(cpy,ap);
		tg.clear();
		tg.resize(tg.capacity());
		int ret = vsnprintf(&tg[0],tg.size()-1,fmt,ap);
		if(abs(ret) >= tg.size()){
			tg.clear();
			tg.resize(abs(ret) + 1);
			ret = vsnprintf(&tg[0],tg.size()-1,fmt,cpy);
		}
		va_end(cpy);
		return ret;
	}
	
	void* adefaultErrorCallback(int code,const char * ct,void*){
		std::cerr << ACP_RED << "Alib4Error[CODE:" << code << "]:" << ((ct)?ct:"") << ACP_RESET << std::endl;
		return nullptr;
	}
	
	void aenableVirtualTerminal(){
		#ifdef _WIN32
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			DWORD consoleMode;
			GetConsoleMode(hConsole, &consoleMode);
			consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
			SetConsoleMode(hConsole, consoleMode);
		#endif
		///do nothing in linux/unix or others
	}
	
	long aio_fileSize(const char * file){
		if(!file){
			asetLastError(AE_EMPTY_DATA,"aio_fileSize->Null was given!");
			return AE_EMPTY_DATA;
		}
		struct stat statbuf;
		int ret;
		ret = stat(file,&statbuf);
		if(ret != 0){
			asetLastErrorf(AE_FAILED,"aio_fileSize Unable to check file %s",file);
			return AE_FAILED;
		};
		return statbuf.st_size;
	}
	
	long aio_readAll(const char * fpath,AStrHandle appender){
		auto str = astr_getstring(appender);
		if(str == nullptr)return agetLastError().code;
		auto & ss = (*str);
		if(!fpath){
			asetLastError(AE_EMPTY_DATA,"aio_readAll->Null was given!");
			return AE_EMPTY_DATA;
		}
		FILE * file = fopen(fpath,"r");
		if(!file){
			asetLastErrorf(AE_IO,"aio_readAll->Cannot open file %s!",fpath);
			return AE_IO;
		}
		long fsize = aio_fileSize(fpath);
		if(fsize > 0){
			std::vector<char> buf;
			buf.resize(fsize / sizeof(char) +  1);
			buf[fsize / sizeof(char)] = '\0';
			fread(buf.data(),sizeof(char),fsize / sizeof(char),file);
			ss.append(buf.begin(),buf.end());
		}
		fclose(file);
		return (int)fsize;
	}
	
	const char * asafe(const char * ss){
		if(ss)return ss;
		return "Invalid String";
	}
	
	long aio_writeAll(const char * fp,const char * data,long length){
		if(!fp){
			asetLastError(AE_EMPTY_DATA,"aio_readAll->Null was given!");
			return AE_EMPTY_DATA;
		}
		if(length < 0)length = strlen(data);
		std::ofstream of;
		of.open(fp);
		if(!of.is_open()){
			asetLastErrorf(AE_IO,"aio_writeAll->Cannot open file %s!",fp);
			return AE_IO;
		}
		of.write(data,length);
		long ret = (long)of.tellp();
		of.close();
		return ret;
	}
	
	
	_Bool aio_checkExistence(const char * fp){
		if(!fp){
			asetLastError(AE_EMPTY_DATA,"aio_readAll->Null was given!");
			return false;
		}
		struct stat buffer;
		return (stat(fp, &buffer) == 0);
	}
	
	int aio_traverseImpl(
		const fs::path& basePath,
		std::vector<std::string>& results,
		int remainingDepth,
		const fs::path& currentAppender,
		bool includeFiles,
		bool includeDirs
	){
		if (!fs::exists(basePath)) {
			asetLastErrorf(AE_IO,"Path not found:%s",basePath.c_str());
			return AE_IO;
		}

		try {
			for (const auto& entry : fs::directory_iterator(basePath)) {
				const auto& path = entry.path();
				const fs::path relativePath = currentAppender / path.filename();

				// 处理目录
				if (entry.is_directory()) {
					if (includeDirs) {
						results.push_back(relativePath.generic_string());
					}

					// 递归处理子目录
					if (remainingDepth != 0) {
						const int newDepth = (remainingDepth > 0) ? remainingDepth - 1 : -1;
						///No error dealing here
						aio_traverseImpl(
							path,
							results,
							newDepth,
							relativePath,
							includeFiles,
							includeDirs
						);
					}
				}
				// 处理文件
				else if (includeFiles && entry.is_regular_file()) {
					results.push_back(relativePath.generic_string());
				}
			}
		} catch (const fs::filesystem_error& e) {
			asetLastErrorf(AE_IO,"Filesystem error:%s",e.what().c_str());
			return AE_IO;
		}
		return AE_SUCCESS;
	}
	
	int aio_traverseFilesDirs(
		const std::string& path,
		std::vector<std::string>& files,
		int traverseDepth,
		const std::string& appender
	) {
		const fs::path basePath(path);
		const fs::path initialAppender(appender);
		return aio_traverseImpl(basePath, files, traverseDepth, initialAppender, true, true);
	}

	// 变体1：仅遍历文件
	int aio_traverseFilesOnly(
		const std::string& path,
		std::vector<std::string>& files,
		int traverseDepth,
		const std::string& appender
	) {
		const fs::path basePath(path);
		const fs::path initialAppender(appender);
		return aio_traverseImpl(basePath, files, traverseDepth, initialAppender, true, false);
	}

	// 变体2：仅遍历目录
	int aio_traverseFolders(
		const std::string& path,
		std::vector<std::string>& folders,
		int traverseDepth,
		const std::string& appender
	) {
		const fs::path basePath(path);
		const fs::path initialAppender(appender);
		return aio_traverseImpl(basePath, folders, traverseDepth, initialAppender, false, true);
	}

	// 变体3：快速递归文件遍历（深度无限）
	int aio_traverseFilesRecursive(
		const std::string& path,
		std::vector<std::string>& files,
		const std::string& appender
	) {
		const fs::path basePath(path);
		const fs::path initialAppender(appender);
		return aio_traverseImpl(basePath, files, -1, initialAppender, true, false);
	}

	// 变体4：快速递归目录遍历（深度无限）
	int aio_traverseFoldersRecursive(
		const std::string& path,
		std::vector<std::string>& folders,
		const std::string& appender
	) {
		const fs::path basePath(path);
		const fs::path initialAppender(appender);
		return aio_traverseImpl(basePath, folders, -1, initialAppender, false, true);
	}
	
	
    const char * agetTime(){
		static thread_local std::string data = "";
		data.clear();
		data.resize(512);
		time_t rawtime;
		struct tm *ptminfo;
		time(&rawtime);
		ptminfo = localtime(&rawtime);
		sprintf(&data[0],"%02d-%02d-%02d %02d:%02d:%02d",
				ptminfo->tm_year + 1900, ptminfo->tm_mon + 1, ptminfo->tm_mday,
				ptminfo->tm_hour, ptminfo->tm_min, ptminfo->tm_sec);
		return data.c_str();
	}
	
	const char * aformatDuration(int secs){
		static thread_local std::string data = "";
		data.clear();
		int sec = secs%60;
		secs /= 60;
		int min = secs % 60;
		secs /= 60;
		int hour = secs % 60;
		secs /= 60;
		int day = secs % 24;
		secs /= 24;
		int year = secs % 356;
		if(year != 0)data += std::to_string(year) + "y ";
		if(day != 0)data += std::to_string(day) + "d ";
		if(hour != 0)data += std::to_string(hour) + "h ";
		if(min != 0)data += std::to_string(min) + "m ";
		if(sec != 0)data += std::to_string(sec) + "s";
		return data.c_str();
	}
}