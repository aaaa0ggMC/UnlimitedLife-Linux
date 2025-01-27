#include <iostream>
#include <fstream>
#include <vector>
#include <alib-g3/aclock.h>
#include <alib-g3/autil.h>
#include <alib-g3/alogger.h>
#include <string.h>
#include <glm/glm.hpp>
#include <thread>
#include <chrono>
#include <map>
#include <unordered_map>
#include <tuple>

using namespace std;
using namespace alib::g3;

void test_autil();
void test_alogger();
void test_aclock();

int main(int argc,const char * argv[])
{
    if(argc < 2){
        char buf[256] = {0};
        const char * args[] = {"",buf};
        while(true){
            cout << "----------------------------------------" << endl;
            cout << "util      Test autil.h" << endl;
            cout << "logger    Test alogger.h" << endl;
            cout << "clock    Test aclock.h" << endl;
            cout << "q / Q    Quit" << endl;
            memset(buf,256,sizeof(char));
            scanf("%s",buf);
            cout << "****************************************" << endl;
            if(!strncasecmp("q",buf,1))return 0;
            main(2,args);
        }
    }
    if(!strcmp("util",argv[1])){
        test_autil();
    }else if(!strcmp("logger",argv[1])){
        test_alogger();
    }else if(!strcmp("clock",argv[1])){
        test_aclock();
    }
    return 0;
}

/*
void test_alogger(){
    Logger logger;
    LogFactory lg("Test",logger);
    cout << "[Stage]Pre output" << endl;
    lg.trace("TraceTest");
    lg.info("InfoTest");
    lg.warn("WarnTest");
    lg.error("ErrorTest");
    lg.critical("CriticalTest");

    cout << "[Stage]Set output file test_data/logger_ouput" << endl;
    logger.setOutputFile("test_data/logger_output");
    lg.info("AfterTest");
    lg.info("MultiOpen:Should arise error");
    logger.setOutputFile("test_data/logger_output");

    cout << "Content color red" << endl;
    lg.setContentColor(APCF_RED);
    lg.info("ColorTest");
    lg.setContentColor(APCF_WHITE);

    cout << "GLM Test" << endl;
    {
        cout << "\e[100mvec2\e[0m" << endl;
        glm::vec2 v(1,2);
        lg << v << endlog;
    }
    {
        cout << "\e[100mvec3\e[0m" << endl;
        glm::vec3 v(1,2,3);
        lg << v << endlog;
    }
    {
        cout << "\e[100mvec4\e[0m" << endl;
        glm::vec4 v(1,2,3,4);
        lg << v << endlog;
    }
    {
        cout << "\e[100mmat2\e[0m" << endl;
        glm::mat2 v(1,2,3,4);
        lg << v << endlog;
    }
    {
        cout << "\e[100mmat3\e[0m" << endl;
        glm::mat3 v(1,2,3,4,5,6,7,8,9);
        lg << v << endlog;
    }
    {
        cout << "\e[100mmat4\e[0m" << endl;
        glm::mat4 v(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);
        lg << v << endlog;
    }

    cout << "DisableOutput" << endl;
    logger.setOutputToConsole(false);
    lg << "HelloWorld!" << endlog;
    logger.setOutputToConsole(true);

    cout << "Multithread Output Test:16 threads" << endl;
    auto fn = [&](int id){
        for(unsigned int i = 0;i < 16;++i){
            lg.info(string("HelloWorld!Th") + std::to_string(id) + " " + to_string(i));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };
    std::vector<std::thread> ths;
    for(unsigned int idx = 0;idx < 16;++idx){
        ths.emplace_back(fn,idx);
    }
    for(unsigned int i = 0;i < 17;++i){
        lg.info(string("HelloWorld!Main") + to_string(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    for(unsigned int idx = 0;idx < 16;++idx){
        if(ths[idx].joinable())ths[idx].join();
    }

    cout << "Test split files,close & reopen logger..." << endl;
    logger.close();
    logger.setSplitFiles(true);
    logger.setSingleFileMaxSize(128);
    cout << "Split:true MaxFileSize:32 file0: test_data/multi/logs.log" << endl;
    logger.setOutputFile("test_data/multi/logs.log");

    cout << "Now writes " << logger.getCurrentLogFile() << endl;
    lg.info("TestTestTestTestTestTestTestTestTesTestTestTestTestTesTestTestTestTestTesTestTestTestTestTesTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTest");
    cout << "Now writes " << logger.getCurrentLogFile() << endl;
    lg.info("TestTestTestTestTestTestTestTestTestTest");
    cout << "Now writes " << logger.getCurrentLogFile() << endl;
    lg.info("TestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTest");
    cout << "Now writes " << logger.getCurrentLogFile() << endl;
    lg.info("TestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTestTest");

    logger.close();
    logger.setOutputFile("test_data/template_logger_output");
    cout << "Not show container name" << endl;
    cout << "\e[100mTesting templates\e[0m" << endl;
    {
        cout << "vector<int>" << endl;
        vector<int> vec = {1,2,3,4,5,6};
        lg << vec << endlog;
    }
    {
        cout << "vector<string>" << endl;
        vector<string> vec = {"123","hhh","cnm","111"};
        lg << vec << endlog;
    }
    {
        cout << "vector<char*>" << endl;
        vector<const char *> vec = {"123","hhh","cn","111"};
        lg << vec << endlog;
    }
    {
        cout << "vector<char>" << endl;
        vector<char> vec = {'1','2','3','4'};
        lg << vec << endlog;
    }
    {
        cout << "vector<vector<double>>" << endl;
        vector<vector<double>> vec = {{1.23},{1.444,2.34354,2.43534},{3.45453},{1.53,2.454,4.543},{5.543},{6.54343}};
        lg << vec << endlog;
    }
    {
        cout << "map<string,int>" << endl;
        map<string,int> vec = {{"cn",1},{"wd",2}};
        lg << vec << endlog;
    }
    {
        cout << "map<string,vector<string>>" << endl;
        map<string,vector<string>> vec = {{"cn",{"123","234"}},{"cm",{"wd","11"}}};
        lg << vec << endlog;
    }
    {
        cout << "unordered_map<string,vector<string>>" << endl;
        unordered_map<string,vector<string>> vec = {{"cn",{"123","234"}},{"cm",{"wd","11"}}};
        lg << vec << endlog;
    }
    cout << "Show container name:demangled" << endl;
    lg.setShowContainerName(true);
    {
        cout << "tuple<string,int,double,map<string,string>>" << endl;
        tuple<string,int,double,map<string,string>> vec = std::make_tuple(std::string("123"),1,2.0123,
                map<string,string>({{"123","456"},{"kkk","jjj"}}));
        lg << vec << endlog;
    }
}*/

void test_alogger(){
    Logger logger;
    logger.appendLogOutputTarget("console",std::make_shared<log_output_targets::Console>());
    logger.appendLogOutputTarget("single_file",std::make_shared<log_output_targets::SingleFile>("test_data/logger_newVer"));
    //logger.appendLogFilter("level",std::make_shared<log_filters::LogLevel>());
    logger.appendLogFilter("replace",std::make_shared<log_filters::KeywordsReplacerMono>(false,'*',"[Sensitive]",std::vector<std::string>({"Hello"})));
    LogFactory lg("Test",logger);

    lg(LOG_INFO) << "Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!" << endlog;
    logger.setLogFiltlerStatus("replace",false);
    lg(LOG_INFO) << "Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!" << endlog;
    lg(LOG_INFO) << "Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!" << endlog;
    lg(LOG_INFO) << "Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!Hello World!" << endlog;
    lg(LOG_TRACE) << "TRACE" << endlog;
    lg(LOG_DEBUG) << "DEBUG" << endlog;
}

void test_autil(){
        cout << "\e[100mTest autil.h\e[0m" << endl;
    cout << "\e[93;100m  IO Phase\e[0m" << endl;
    cout << "\e[100m    io_checkExistence (True)\e[0m";
    cout << (Util::io_checkExistence("test_data/existence")?"true" : "false") << endl;
    cout << "\e[100m    io_checkExistence (False)\e[0m";
    cout << (Util::io_checkExistence("test_data/not_existence")?"true" : "false") << endl;

    cout << "\e[100m    io_fileSize (86bytes)\e[0m";
    cout << Util::io_fileSize("test_data/file_size") << endl;

    cout << "\e[100m    io_printColor\e[0m" << endl;
    Util::io_printColor("TestRBL ",APCF_RED | APCB_BLACK);
    Util::io_printColor("TestRB ",APCF_RED | APCB_BLUE);
    Util::io_printColor("TestRW+ ",APCF_RED | APCB_BRIGHT_WHITE);
    Util::io_printColor("TestRC ",APCF_RED | APCB_CYAN);
    Util::io_printColor("TestRGRAY ",APCF_RED | APCB_GRAY);
    Util::io_printColor("TestRG ",APCF_RED | APCB_GREEN);
    Util::io_printColor("TestRB+ ",APCF_RED | APCB_LIGHT_BLUE);
    Util::io_printColor("TestRCYAN+ ",APCF_RED | APCB_LIGHT_CYAN);
    Util::io_printColor("TestRM+ ",APCF_RED | APCB_LIGHT_MAGENTA);
    Util::io_printColor("TestRR+ ",APCF_RED | APCB_LIGHT_RED);
    Util::io_printColor("TestRY+ ",APCF_RED | APCB_LIGHT_YELLOW);
    Util::io_printColor("TestRR ",APCF_RED | APCB_RED);
    Util::io_printColor("TestRW ",APCF_RED | APCB_WHITE);
    Util::io_printColor("TestRY ",APCF_RED | APCB_YELLOW);
    Util::io_printColor("TestRM ",APCF_RED | APCB_MAGENTA);
    Util::io_printColor("TestBLW ",APCF_BLACK | APCB_WHITE);
    Util::io_printColor("TestWBL ",APCF_WHITE | APCB_BLACK);
    Util::io_printColor("TestBBL ",APCF_BLUE | APCB_BLACK);
    Util::io_printColor("TestW+BL ",APCF_BRIGHT_WHITE | APCB_BLACK);
    Util::io_printColor("TestCYANBL ",APCF_CYAN | APCB_BLACK);
    Util::io_printColor("TestGRAYBL ",APCF_GRAY | APCB_BLACK);
    Util::io_printColor("TestGBL ",APCF_GREEN | APCB_BLACK);
    Util::io_printColor("TestB+BL ",APCF_LIGHT_BLUE | APCB_BLACK);
    Util::io_printColor("TestCYAN+BL ",APCF_LIGHT_CYAN | APCB_BLACK);
    Util::io_printColor("TestG+BL ",APCF_LIGHT_GREEN | APCB_BLACK);
    Util::io_printColor("TestM+BL ",APCF_LIGHT_MAGENTA | APCB_BLACK);
    Util::io_printColor("TestR+BL ",APCF_LIGHT_RED | APCB_BLACK);
    Util::io_printColor("TestY+BL ",APCF_LIGHT_YELLOW | APCB_BLACK);
    Util::io_printColor("TestM+BL ",APCF_MAGENTA | APCB_BLACK);
    Util::io_printColor("TestRBL ",APCF_RED | APCB_BLACK);
    Util::io_printColor("TestWBL ",APCF_WHITE | APCB_BLACK);
    Util::io_printColor("TestYBL ",APCF_YELLOW | APCB_BLACK);
    cout << "End Test(ShouldBeInNormal)" << endl;

    cout << "\e[100m    io_readAll (filename)(86bytes)\e[0m" << endl;
    {
        string out = "";
        int ret = Util::io_readAll("test_data/file_size",out);
        cout << "Ret:" << ret << endl;
        cout << "Content:" << endl << out << endl;
    }
    cout << "\e[100m    io_readAll (ifstream)(86bytes)\e[0m" << endl;
    {
        ifstream ifs("test_data/file_size");
        string out = "";
        int ret = Util::io_readAll(ifs,out);
        cout << "Ret:" << ret << endl;
        cout << "Content:" << endl << out << endl;
        ifs.close();
    }

    cout << "\e[100m    io_writeAll\e[0m" << "TestDatadhdhahua9we8q9ha" << endl;
    cout << "Ret:" << Util::io_writeAll("test_data/write_all","TestDatadhdhahua9we8q9ha") << endl;


    cout << "\e[100m    io_traverseFiles (No Loop No Slash test_data)\e[0m" << endl;
    {
        vector<string> files;
        Util::io_traverseFiles("test_data",files);
        for(auto & s : files){
            cout << s << "\n";
        }
        cout << endl;
    }
    cout << "\e[100m    io_traverseFiles (No Loop Slash test_data/)\e[0m" << endl;
    {
        vector<string> files;
        Util::io_traverseFiles("test_data/",files);
        for(auto & s : files){
            cout << s << "\n";
        }
        cout << endl;
    }


    cout << "\e[100m    io_traverseFiles (No Loop No Slash test_data fixed_append/)\e[0m" << endl;
    {
        vector<string> files;
        Util::io_traverseFiles("test_data",files,100,"fixed_append/");
        for(auto & s : files){
            cout << s << "\n";
        }
        cout << endl;
    }
    cout << "\e[100m    io_traverseFiles (No Loop Slash test_data/ fixed_append/)\e[0m" << endl;
    {
        vector<string> files;
        Util::io_traverseFiles("test_data/",files,100,"fixed_append/");
        for(auto & s : files){
            cout << s << "\n";
        }
        cout << endl;
    }


    cout << "\e[93;100m  OT Phase\e[0m" << endl;
    cout << "\e[100m    ot_formatDuration\e[0m" << endl << "1234567898s =";
    cout << Util::ot_formatDuration(1234567898) << endl;


    cout << "\e[100m    ot_getTime\e[0m" << endl;
    cout << Util::ot_getTime() << endl;


    cout << "\e[100m  DataString Phase\e[0m" << endl;
    cout << "\e[100m    str_encAnsiToUTF8 (DISABLED || WIP) \e[0m" << endl;
    cout << Util::str_encAnsiToUTF8("测试") << endl;
    cout << "\e[100m    str_encUTF8ToAnsi (DISABLED || WIP) \e[0m" << endl;
    cout << Util::str_encUTF8ToAnsi("测试") << endl;
    cout << "Sample:"<< endl;
    cout << "12345;12345;sad123;good;nice123" << endl;
    cout << "\e[100m    str_split (char ;) \e[0m" << endl;
    {
        vector<string> store;
        Util::str_split("12345;12345;sad123;good;nice123",';',store);
        cout << "Result:" << endl;
        for(auto & c : store){
            cout << c << endl;
        }
    }
    cout << "\e[100m    str_split (string 123) \e[0m" << endl;
    {
        vector<string> store;
        Util::str_split("12345;12345;sad123;good;nice123","123",store);
        cout << "Result:" << endl;
        for(auto & c : store){
            cout << c << endl;
        }
    }

    cout << "Sample:" << "aBcDeFg这不是字母HiJklMn" << endl;
    cout << "\e[100m    str_toLower\e[0m" << endl;
    cout << Util::str_toLower("aBcDeFg这不是字母HiJklMn") << endl;
    cout << "\e[100m    str_toUpper\e[0m" << endl;
    cout << Util::str_toUpper("aBcDeFg这不是字母HiJklMn") << endl;

    cout << "Sample:" << "                  aBcDeFg这不是字母HiJklMn                 " << endl;
    cout << "\e[100m    str_trim_rt\e[0m" << endl;
    string data = "                  aBcDeFg这不是字母HiJklMn                 ";
    cout << Util::str_trim_rt(data) << endl;


    cout << "Sample:" << "testunescape\\nhhhh" << endl;
    cout << "\e[100m    str_unescape\e[0m" << endl;
    cout << Util::str_unescape("testunescape\\nhhhh") << endl;

    cout << "\e[93;100m  Sys Phase\e[0m" << endl;
    cout << "\e[100m    sys_etCPUId\e[0m" << endl;
    cout << Util::sys_getCPUId() << endl;

    cout << "\e[100m    sys_getGlobalMemoryUsage\e[0m" << endl;
    GlobalMemUsage mem = Util::sys_getGlobalMemoryUsage();
    cout << "All Page Memory:" << mem.pageTotal << endl;
    cout << "Used Page Memory:" << mem.pageUsed << endl;
    cout << "All Phy Memory:" << mem.physicalTotal << endl;
    cout << "Used Phy Memory:" << mem.physicalUsed << endl;
    cout << "Phy Memory Percent:" << mem.percent << endl;
    cout << "All Virt Memory:" << mem.virtualTotal << endl;
    cout << "Used Virt Memory:" << mem.virtualUsed << endl;


    cout << "\e[100m    sys_getProgramMemoryUsage\e[0m" << endl;
    ProgramMemUsage mtp =  Util::sys_getProgramMemoryUsage();
    cout << "Memory Usage:" << mtp.memory << endl;
    cout << "VMemory Usage:" << mtp.virtualMemory << endl;
}

void test_aclock(){
    Clock clk(false);
    cout << "\e[100mBasicUsage:\e[0m" << endl;
    clk.start();
    cout << "Sleep2S:" << endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "All " << clk.getAllTime() << " | Offset " << clk.getOffset() << endl;
    cout << "Clear offset:" << endl;
    clk.clearOffset();
    std::cout << "All " << clk.getAllTime() << " | Offset " << clk.getOffset() << endl;
    auto st = clk.stop();
    cout << "Stop clock";
    std::cout << "All " << st.all << " | Offset " << st.offset << endl;
    clk.start();
    cout << "Restart clock & Sleep 150ms then clearOffset" << endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    std::cout << "All " << clk.getAllTime() << " | Offset " << clk.getOffset() << endl;
    clk.clearOffset();

    cout << "\e[100mPause&Resume: print sleep 150ms,print pause & sleep for 150ms & print,resume print & sleep for 150ms & print\e[0m" << endl;
    std::cout << "All " << clk.getAllTime() << " | Offset " << clk.getOffset() << endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    std::cout << "All " << clk.getAllTime() << " | Offset " << clk.getOffset() << endl;
    clk.pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    std::cout << "All " << clk.getAllTime() << " | Offset " << clk.getOffset() << endl;
    clk.resume();
    std::cout << "All " << clk.getAllTime() << " | Offset " << clk.getOffset() << endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    std::cout << "All " << clk.getAllTime() << " | Offset " << clk.getOffset() << endl;

    clk.reset();
    cout << "Reset clock" << endl;
    cout << "\e[100mTrigger 1000ms:\e[0m" << endl;
    ///Trigger
    Trigger trig(clk,1000);
    std::cout << "StartAll " << clk.getAllTime() << " | Offset " << clk.getOffset() << endl;
    while(!trig.test()){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::cout << "EndAll " << clk.getAllTime() << " | Offset " << clk.getOffset() << endl;

    ///RateLimiter
    cout << "\e[100mRateLimiter: 60.5fps\e[0m" << endl;
    RateLimiter rl(60.5);
    unsigned int counter = 0;
    clk.clearOffset();
    std::cout << "StartAll " << clk.getAllTime() << " | Offset " << clk.getOffset() << endl;
    while(true){
        rl.wait();
        counter++;
        if(counter >= 6000)break;
    }
    double fps = counter / clk.getOffset();
    std::cout << "EndAll " << clk.getAllTime() << " | Offset " << clk.getOffset() << endl;
    cout << "Fps:" << fps << endl;
}
