#include <alib-g3/autil.h>
#include <functional>
#include <time.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#ifdef _WIN32
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

using namespace alib::g3;
namespace fs = std::filesystem;
using namespace std;

struct AutoFix{
    AutoFix(){
        Util::sys_enableVirtualTerminal();
    }

    ~AutoFix(){
        printf(ACP_RESET);
    }
};

static AutoFix __autofix;

void Util::sys_enableVirtualTerminal(){
    #ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD consoleMode;
    GetConsoleMode(hConsole, &consoleMode);
    consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hConsole, consoleMode);
    #endif
    ///do nothing in linux/unix or others
}

int Util::io_printColor(dstring message,const char * color){
    printf(color);
    int ret = printf(message.c_str());
    printf(ACP_RESET);
    return ret;
}

std::string Util::ot_getTime() {
    time_t rawtime;
    struct tm *ptminfo;
    std::string rt = "";
    time(&rawtime);
    ptminfo = localtime(&rawtime);
    char * mdate = (char *)malloc(sizeof(char) * (512));
    memset(mdate,0,sizeof(char) * (512));
    sprintf(mdate,"%02d-%02d-%02d %02d:%02d:%02d",
            ptminfo->tm_year + 1900, ptminfo->tm_mon + 1, ptminfo->tm_mday,
            ptminfo->tm_hour, ptminfo->tm_min, ptminfo->tm_sec);
    rt = mdate;
    free(mdate);
    return rt;
}

std::string Util::sys_getCPUId(){
    #ifdef _WIN32
    long lRet;
    HKEY hKey;
    CHAR tchData[1024];
    DWORD dwSize;

    lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE,TEXT("Hardware\\Description\\System\\CentralProcessor\\0"),0,KEY_QUERY_VALUE,&hKey);

    if(lRet == ERROR_SUCCESS) {
        dwSize = 1024;
        lRet = RegQueryValueEx(hKey,TEXT("ProcessorNameString"),0,0,(LPBYTE)tchData,&dwSize);

        tchData[dwSize] = '\0';
        RegCloseKey(hKey);
        if(lRet == ERROR_SUCCESS) {
            return std::string(tchData);
        } else {
            return "Unknown";
        }
    }
    return "Unknown";
    #else
    std::ifstream proc("/proc/cpuinfo");
    if(proc.good()){
        std::string line;
        std::string ret = "";
        while(std::getline(proc,line)){
            if(line.find("model name") != std::string::npos){
                ret = line.substr(line.find(":") + 2);
            }
        }
        return ret;
    }else return "Unknown";
    #endif // _WIN32
}

// 实现部分
void Util::io_traverseImpl(
    const fs::path& physicalPath,
    std::vector<std::string>& results,
    const fs::path fixedRoot,
    int remainingDepth,
    const std::string& appender,
    bool includeFiles,
    bool includeDirs,
    bool useAbsolutePath
) {
    if (!fs::exists(physicalPath)) {
        std::cerr << "Path not found: " << physicalPath << std::endl;
        return;
    }

    try {
        // 预处理基础路径
        const fs::path processedBase = useAbsolutePath ?
        physicalPath.lexically_normal().make_preferred() :
        physicalPath.lexically_relative(fs::current_path()).make_preferred();

        for (const auto& entry : fs::directory_iterator(physicalPath)) {
            // 构造逻辑路径
            const std::string logicalPath = (processedBase / entry.path().filename()).generic_string();

            const fs::path newVer = (fixedRoot / processedBase / entry.path().filename());

            // 构建最终结果路径
            const std::string finalPath = appender +
            (useAbsolutePath ?
            fs::absolute(logicalPath).generic_string() :
            newVer.generic_string());

            if (entry.is_directory()) {
                if (includeDirs) {
                    results.push_back(finalPath);
                }
                if (remainingDepth != 0) {
                    const int newDepth = (remainingDepth > 0) ? remainingDepth - 1 : -1;
                    io_traverseImpl(
                        entry.path(),
                                    results,
                                    newVer,
                                    newDepth,
                                    appender,
                                    includeFiles,
                                    includeDirs,
                                    useAbsolutePath
                    );
                }
            } else if (includeFiles && entry.is_regular_file()) {
                results.push_back(finalPath);
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }
}

void Util::io_traverseFiles(
    const std::string& path,
    std::vector<std::string>& files,
    int traverseDepth,
    const std::string& appender,
    bool absolute
) {
    const fs::path basePath(path);
    io_traverseImpl(basePath, files, basePath , traverseDepth, appender, true, true, absolute);
}

void Util::io_traverseFilesOnly(
    const std::string& path,
    std::vector<std::string>& files,
    int traverseDepth,
    const std::string& appender,
    bool absolute
) {
    const fs::path basePath(path);
    io_traverseImpl(basePath, files, basePath , traverseDepth, appender, true, false, absolute);
}

void Util::io_traverseFolders(
    const std::string& path,
    std::vector<std::string>& folders,
    int traverseDepth,
    const std::string& appender,
    bool absolute
) {
    const fs::path basePath(path);
    io_traverseImpl(basePath, folders, basePath , traverseDepth, appender, false, true, absolute);
}

std::string Util::str_unescape(dstring in) {
    std::string out = "";
    for(size_t i = 0; i < in.length(); i++) {
        if(in[i] == '\\') {
            if(++i == in.length()) return out; // 边界检查
            switch(in[i]) {
            case 'n'://New Line
                out += '\n';
                break;
            case '\\'://Backslash
                out += '\\';
                break;
            case 'v'://vertical
                out += '\v';
                break;
            case 't'://tab
                out += '\t';
                break;
            case 'r'://return
                out += '\r';
                break;
            case 'a'://alll
                out += '\007';
                break;
            case 'f'://换页符
                out += '\f';
                break;
            case 'b'://退格符
                out += '\b';
                break;
            case '0'://空字符
                out += '\0';
                break;
            case '?'://问号
                out += '\?';
                break;
            case '\"'://双引号
                out += '\"';
                break;
            default:
                i--;
                out += in[i];
                break;
            }
        } else {
            out += in[i];
        }
    }
    return out;
}


long Util::io_fileSize(dstring filePath) {
    struct stat statbuf;
    int ret;
    ret = stat(filePath.c_str(),&statbuf);//调用stat函数
    if(ret != 0) return AE_FAILED;//获取失败。
    return statbuf.st_size;//返回文件大小。
}

int Util::io_writeAll(dstring fth,dstring s) {
    std::ofstream of;
    of.open(fth);
    if(!of.is_open())return AE_FAILED;
    of.write(s.c_str(),s.length());
    of.close();
    return s.length();
}

int Util::io_readAll(std::ifstream & reader,string & ss) {
    unsigned long pos0 = reader.tellg();
    reader.seekg(0,std::ios::end);
    unsigned long pos1 = reader.tellg();
    reader.seekg(pos0,std::ios::beg);
    unsigned long size = pos1 - pos0+1;

    char * buf = new char[size+1];
    memset(buf,0,sizeof(char) * (size+1));

    reader.read(buf,size);
    buf[size] = 0;
    ss.append(buf);

    delete [] buf;

    return size-1;
}

int Util::io_readAll(dstring fpath,std::string & ss) {
    FILE * file = fopen(fpath.c_str(),"r");
    if(!file)return -1;
    long fsize = io_fileSize(fpath);
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

std::string Util::str_trim_rt(std::string & str) {
    std::string blanks("\f\v\r\t\n ");
    std::string temp;
    for(int i = 0; i < (int)str.length(); i++) {
        if(str[i] == '\0')
            str[i] = '\t';
    }
    str.erase(0,str.find_first_not_of(blanks));
    str.erase(str.find_last_not_of(blanks) + 1);
    temp = str;
    return temp;
}

void Util::str_trim_nrt(std::string & str) {
    std::string blanks("\f\v\r\t\n ");
    std::string temp;
    for(int i = 0; i < (int)str.length(); i++) {
        if(str[i] == '\0')
            str[i] = '\t';
    }
    str.erase(0,str.find_first_not_of(blanks));
    str.erase(str.find_last_not_of(blanks) + 1);
    temp = str;
}

void Util::str_split(dstring line,const char sep,std::vector<std::string> & vct) {
    const size_t size = line.size();
    const char* str = line.c_str();
    int start = 0,end = 0;
    for(int i = 0; i < (int)size; i++) {
        if(str[i] == sep) {
            string st = line.substr(start,end);
            str_trim_nrt(st);
            vct.push_back(st);
            start = i + 1;
            end = 0;
        } else
            end++;
    }
    if(end > 0) {
        string st = line.substr(start,end);
        str_trim_nrt(st);
        vct.push_back(st);
    }
}

void Util::str_split(dstring str,dstring splits, std::vector<std::string>& res) {
    if(!str.compare(""))return;
    //在字符串末尾也加入分隔符，方便截取最后一段
    std::string strs = str + splits;
    size_t pos = strs.find(splits);
    int step = splits.size();

    // 若找不到内容则字符串搜索函数返回 npos
    while (pos != strs.npos) {
        std::string temp = strs.substr(0, pos);
        res.push_back(temp);
        //去掉已分割的字符串,在剩下的字符串中进行分割
        strs = strs.substr(pos + step, strs.size());
        pos = strs.find(splits);
    }
}

bool Util::io_checkExistence(dstring name) {
    struct stat buffer;
    return(stat(name.c_str(), &buffer) == 0);
}

std::string Util::str_encAnsiToUTF8(dstring strAnsi) {
    #ifdef _WIN32
    std::string strOutUTF8 = "";
    WCHAR *str1;
    int n = MultiByteToWideChar(CP_ACP, 0, strAnsi.c_str(), -1, NULL, 0);
    str1 = new WCHAR[n];
    MultiByteToWideChar(CP_ACP, 0, strAnsi.c_str(), -1, str1, n);
    n = WideCharToMultiByte(CP_UTF8, 0, str1, -1, NULL, 0, NULL, NULL);
    char *str2 = new char[n];
    WideCharToMultiByte(CP_UTF8, 0, str1, -1, str2, n, NULL, NULL);
    strOutUTF8 = str2;
    delete [] str1;
    str1 = NULL;
    delete [] str2;
    str2 = NULL;
    return strOutUTF8;
    #else
    ///WARNING!!!!!!unsupported!PleaseGuaranteeUTF-8
    return strAnsi;
    #endif
}


std::string Util::str_encUTF8ToAnsi(dstring strUTF8) {
    #ifdef _WIN32
    int len = MultiByteToWideChar(CP_UTF8, 0, strUTF8.c_str(),  - 1, NULL, 0);
    WCHAR * wsz = new WCHAR[len + 1];
    memset(wsz, 0, (len+1)*sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)strUTF8.c_str(),  - 1, wsz, len);
    len = WideCharToMultiByte(CP_ACP, 0, wsz,  - 1, NULL, 0, NULL, NULL);
    char *sz = new char[len + 1];
    memset(sz, 0, len + 1);
    WideCharToMultiByte(CP_ACP, 0, wsz,  - 1, sz, len, NULL, NULL);
    std::string strTemp(sz);
    delete [] sz;
    sz = NULL;
    delete [] wsz;
    wsz = NULL;
    return strTemp;
    #else
    ///ANSI UnSupportted!!!
    return strUTF8;
    #endif
}

std::string Util::ot_formatDuration(int secs) {
    int sec = secs%60;
    secs /= 60;
    int min = secs % 60;
    secs /= 60;
    int hour = secs % 60;
    secs /= 60;
    int day = secs % 24;
    secs /= 24;
    int year = secs % 356;
    std::string ret = "";
    if(year != 0)ret += to_string(year) + "y ";
    if(day != 0)ret += to_string(day) + "d ";
    if(hour != 0)ret += to_string(hour) + "h ";
    if(min != 0)ret += to_string(min) + "m ";
    if(sec != 0)ret += to_string(sec) + "s";
    return ret;
}

ProgramMemUsage Util::sys_getProgramMemoryUsage() {
    #ifdef _WIN32
    mem_bytes mem = 0, vmem = 0;
    PROCESS_MEMORY_COUNTERS pmc;

    // 获取当前进程句柄
    HANDLE process = GetCurrentProcess();
    if (GetProcessMemoryInfo(process, &pmc, sizeof(pmc))) {
        mem = pmc.WorkingSetSize;
        vmem = pmc.PagefileUsage;
    }
    CloseHandle(process);

    // 使用 GetCurrentProcess() 可以获取当前进程，并且无需关闭句柄
    return {mem, vmem};
    #else
    mem_bytes mem = 0, vmem = 0;
    std::ifstream stat_file("/proc/self/stat");
    std::ifstream status_file("/proc/self/status");

    if (!stat_file.is_open() || !status_file.is_open()) {
        //std::cerr << "Failed to open /proc/self/stat or /proc/self/status\n";
        return {0, 0};
    }

    // 读取 /proc/self/stat 文件获取虚拟内存
    std::string line;
    std::getline(stat_file, line);
    std::istringstream stat_stream(line);
    std::string dummy;
    unsigned long vsize;

    // stat 文件的格式：[pid] (name) state ppid pgrp session tty_nr tpgid flags minflt cminflt majflt cmajflt utime stime cutime cstime priority nice num_threads itrealvalue starttime vsize rss
    // 我们需要 vsize（虚拟内存）和 rss（物理内存页数）
    for (int i = 0; i < 22; ++i) {
        stat_stream >> dummy;  // 忽略前面的字段
    }
    stat_stream >> vsize >> dummy;  // vsize 是虚拟内存（单位：KB）
    stat_stream >> mem; // rss 是物理内存的页数（单位：页面）

    // 将物理内存从页面数转换为字节
    long page_size = sysconf(_SC_PAGESIZE);
    mem *= page_size;

    // 读取 /proc/self/status 获取物理内存的更多信息
    while (std::getline(status_file, line)) {
        std::istringstream status_stream(line);
        std::string key;
        unsigned long value;

        status_stream >> key >> value;
        if (key == "VmRSS:") { // VmRSS 是物理内存使用（单位：KB）
            mem = value * 1024;  // 转换为字节
        } else if (key == "VmSize:") { // VmSize 是虚拟内存（单位：KB）
            vmem = value * 1024;  // 转换为字节
        }
    }

    return {mem, vmem};
    #endif
}

GlobalMemUsage Util::sys_getGlobalMemoryUsage() {
    #ifdef _WIN32
    MEMORYSTATUSEX statex;
    GlobalMemUsage ret = {0};
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);

    ret.physicalTotal = statex.ullTotalPhys;
    ret.physicalUsed = ret.physicalTotal - statex.ullAvailPhys;
    ret.virtualTotal = statex.ullTotalVirtual;
    ret.virtualUsed = ret.virtualTotal - statex.ullAvailVirtual;
    ret.pageTotal = statex.ullTotalPageFile;
    ret.pageUsed = statex.ullTotalPageFile - statex.ullAvailPageFile;
    ret.percent = statex.dwMemoryLoad;

    return ret;
    #else
    GlobalMemUsage ret = {0};

    // 打开 /proc/meminfo 文件
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) {
        //std::cerr << "Failed to open /proc/meminfo" << std::endl;
        return ret;
    }

    std::string line;
    unsigned long long memTotal = 0, memFree = 0, buffers = 0, cached = 0, swapTotal = 0, swapFree = 0;

    // 逐行解析 /proc/meminfo
    while (std::getline(meminfo, line)) {
        std::istringstream iss(line);
        std::string key;
        unsigned long long value;
        std::string unit;

        iss >> key >> value >> unit;

        if (key == "MemTotal:") {
            memTotal = value; // 单位是 KB
        } else if (key == "MemFree:") {
            memFree = value;
        } else if (key == "Buffers:") {
            buffers = value;
        } else if (key == "Cached:") {
            cached = value;
        } else if (key == "SwapTotal:") {
            swapTotal = value;
        } else if (key == "SwapFree:") {
            swapFree = value;
        }

        // 如果所有值已经解析，提前退出
        if (memTotal && memFree && buffers && cached && swapTotal && swapFree) {
            break;
        }
    }
    meminfo.close();

    // 计算物理内存
    ret.physicalTotal = memTotal * 1024; // 转换为字节
    ret.physicalUsed = (memTotal - memFree - buffers - cached) * 1024;

    // 计算虚拟内存（Linux 没有直接的虚拟内存字段，这里可以选择设置为 0）
    ret.virtualTotal = 0;
    ret.virtualUsed = 0;

    // 计算交换空间
    ret.pageTotal = swapTotal * 1024;
    ret.pageUsed = (swapTotal - swapFree) * 1024;

    // 计算内存使用百分比
    ret.percent = (double)ret.physicalUsed / ret.physicalTotal * 100.0;

    return ret;
    #endif
}

CPUInfo::CPUInfo() {
    this->CpuID = Util::sys_getCPUId();
}

std::string Util::str_toUpper(dstring in){
    std::string ret = "";
    ret.reserve(in.size());
    for(auto & ch : in){
        if(isalpha(ch)){
            ret += toupper(ch);
        }else ret += ch;
    }
    return ret;
}

std::string Util::str_toLower(dstring in){
    std::string ret = "";
    ret.reserve(in.size());
    for(auto & ch : in){
        if(isalpha(ch)){
            ret += tolower(ch);
        }else ret += ch;
    }
    return ret;
}

