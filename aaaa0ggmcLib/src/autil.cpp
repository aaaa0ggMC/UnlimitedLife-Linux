#include <alib-g3/autil.h>
#include <time.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <sstream>
#include <string>
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
using namespace alib::ng;

int Util::io_printColor(dstring message,int color) {
    #ifdef _WIN32
    static CONSOLE_SCREEN_BUFFER_INFO info;
    [[maybe_unused]] static BOOL v = GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),&info);

    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),(WORD)color);
    int rt = printf("%s",message.c_str());
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),info.wAttributes);
    return rt;
    #else
    //低性能实现
    //background
    if(color & APCB_BLACK){
        printf("\e[40m");
    }else if(color & APCB_RED){
        printf("\e[41m");
    }else if(color & APCB_GREEN){
        printf("\e[42m");
    }else if(color & APCB_YELLOW){
        printf("\e[43m");
    }else if(color & APCB_BLUE){
        printf("\e[44m");
    }else if(color & APCB_MAGENTA){
        printf("\e[45m");
    }else if(color & APCB_CYAN){
        printf("\e[46m");
    }else if(color & APCB_GRAY){
        printf("\e[47m");
    }else if(color & APCB_LIGHT_RED){
        printf("\e[101m");
    }else if(color & APCB_LIGHT_GREEN){
        printf("\e[102m");
    }else if(color & APCB_LIGHT_YELLOW){
        printf("\e[103m");
    }else if(color & APCB_LIGHT_BLUE){
        printf("\e[104m");
    }else if(color & APCB_LIGHT_MAGENTA){
        printf("\e[105m");
    }else if(color & APCB_LIGHT_CYAN){
        printf("\e[106m");
    }else if(color & APCB_BRIGHT_WHITE){
        printf("\e[107m");
    }
    //foreground
    if(color & APCF_BLACK){
        printf("\e[30m");
    }else if(color & APCF_RED){
        printf("\e[31m");
    }else if(color & APCF_GREEN){
        printf("\e[32m");
    }else if(color & APCF_YELLOW){
        printf("\e[33m");
    }else if(color & APCF_BLUE){
        printf("\e[34m");
    }else if(color & APCF_MAGENTA){
        printf("\e[35m");
    }else if(color & APCF_CYAN){
        printf("\e[36m");
    }else if(color & APCF_GRAY){
        printf("\e[37m");
    }else if(color & APCF_LIGHT_RED){
        printf("\e[91m");
    }else if(color & APCF_LIGHT_GREEN){
        printf("\e[92m");
    }else if(color & APCF_LIGHT_YELLOW){
        printf("\e[93m");
    }else if(color & APCF_LIGHT_BLUE){
        printf("\e[94m");
    }else if(color & APCF_LIGHT_MAGENTA){
        printf("\e[95m");
    }else if(color & APCF_LIGHT_CYAN){
        printf("\e[96m");
    }else if(color & APCF_BRIGHT_WHITE){
        printf("\e[97m");
    }
    int rt = printf("%s",message.c_str());
    printf("\e[0m");
    return rt;
    #endif // _WIN32
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

std::string Util::sys_GetCPUId(){
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

void Util::io_traverseFiles(dstring path, std::vector<std::string>& files,int traverseDepth,dstring appender) {
    #ifdef _WIN32
    //文件句柄 注意：我发现有些文章代码此处是long类型，实测运行中会报错访问异常
    intptr_t hFile = 0;
    //文件信息
    struct _finddata_t fileinfo;
    std::string p;
    p.reserve(MAX_PATH);
    if ((hFile = _findfirst(p.assign(path).append("\\*").c_str(), &fileinfo)) != -1) {
        do {
            p.clear();
            //如果是目录,递归查找
            //如果不是,把文件绝对路径存入vector中
            if (traverseSubdir && (fileinfo.attrib & _A_SUBDIR)) {
                if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0)
                    getFileNames(p.assign(path).append("\\").append(fileinfo.name), files);
            } else {
                files.push_back(p.append(path).append("\\").append(fileinfo.name));
            }
        } while (_findnext(hFile, &fileinfo) == 0);
        _findclose(hFile);
    }
    #else
    DIR * dir = opendir(path.c_str());
    if(dir == nullptr)return;
    struct dirent * entry;
    while((entry = readdir(dir)) != NULL){
        if(!strncmp(".",entry->d_name,1))continue;
        else if(!strncmp("..",entry->d_name,2))continue;
        if(traverseDepth > 0 && entry->d_type == DT_DIR){
            std::string newpath = path;
            std::string appenderpp = appender;
            if(newpath[newpath.size() - 1] != '/'){
                newpath += "/";
            }
            if(appenderpp[appenderpp.size() - 1] != '/'){
                appenderpp += "/";
            }
            newpath += entry->d_name;
            appenderpp += entry->d_name;
            appenderpp += "/";
            ///遍历
            io_traverseFiles(newpath,files,traverseDepth-1,appenderpp);
        }else if(entry->d_type == DT_REG){
            files.push_back(appender + entry->d_name);
        }
    }
    #endif // _WIN32

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
    if(ret != 0) return ALIB_ERROR;//获取失败。
    return statbuf.st_size;//返回文件大小。
}

int Util::io_writeAll(dstring fth,dstring s) {
    std::ofstream of;
    of.open(fth);
    if(!of.is_open())return ALIB_ERROR;
    of.write(s.c_str(),s.length());
    of.close();
    return ALIB_SUCCESS;
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

    return ALIB_SUCCESS;
}

int Util::io_readAll(dstring fpath,std::string & ss) {
    unsigned long size = io_fileSize(fpath);

    std::ifstream reader(fpath);
    if(!reader.good())return ALIB_ERROR;

    char * buf = new char[size+1];
    memset(buf,0,sizeof(char) * (size+1));

    reader.read(buf,size);
    buf[size] = 0;
    reader.close();

    ss.append(buf);

    delete [] buf;

    return ALIB_SUCCESS;
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
            str_trim(st);
            vct.push_back(st);
            start = i + 1;
            end = 0;
        } else
            end++;
    }
    if(end > 0) {
        string st = line.substr(start,end);
        str_trim(st);
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

MemTp Util::sys_getProgramMemoryUsage() {
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

GlMem Util::sys_getGlobalMemoryUsage() {
    #ifdef _WIN32
    MEMORYSTATUSEX statex;
    GlMem ret = {0};
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);

    ret.phy_all = statex.ullTotalPhys;
    ret.phy_used = ret.phy_all - statex.ullAvailPhys;
    ret.vir_all = statex.ullTotalVirtual;
    ret.vir_used = ret.vir_all - statex.ullAvailVirtual;
    ret.page_all = statex.ullTotalPageFile;
    ret.page_used = statex.ullTotalPageFile - statex.ullAvailPageFile;
    ret.percent = statex.dwMemoryLoad;

    return ret;
    #else
    GlMem ret = {0};

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
    ret.phy_all = memTotal * 1024; // 转换为字节
    ret.phy_used = (memTotal - memFree - buffers - cached) * 1024;

    // 计算虚拟内存（Linux 没有直接的虚拟内存字段，这里可以选择设置为 0）
    ret.vir_all = 0;
    ret.vir_used = 0;

    // 计算交换空间
    ret.page_all = swapTotal * 1024;
    ret.page_used = (swapTotal - swapFree) * 1024;

    // 计算内存使用百分比
    ret.percent = (double)ret.phy_used / ret.phy_all * 100.0;

    return ret;
    #endif
}

CPUInfo::CPUInfo() {
    this->CpuID = Util::sys_GetCPUId();
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

