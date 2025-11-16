#include <iostream>
#include <fstream>
#include <vector>
#include <alib-g3/aclock.h>
#include <alib-g3/autil.h>
#include <alib-g3/alogger.h>
#include <alib-g3/atranslator.h>
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
void test_atranslator();

int main(int argc,const char * argv[]){
    if(argc < 2){
        char buf[256] = {0};
        const char * args[] = {"",buf};
        while(true){
            cout << "----------------------------------------" << endl;
            cout << "util      Test autil.h" << endl;
            cout << "logger    Test alogger.h" << endl;
            cout << "clock    Test aclock.h" << endl;
            cout << "translator    Test atranslator.h" << endl;
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
    }else if(!strcmp("clock",argv[1])){
        test_aclock();
    }else if(!strcmp("translator",argv[1])){
        test_atranslator();
    }
    return 0;
}

void test_atranslator(){
    cout << "Translations are assumed to be in: ./test_data/trans/";
    Translator ts("en_us");
    cout << "Ret:" << ts.readTranslationFiles("test_data/trans") << endl;

    Logger logger;
    LogMsgConfig cfg;
    cfg.disable_extra_information = true;
    LogFactory lgf(logger,LogFactoryConfig("Output",2,nullptr,cfg));

    logger.append_mod<lot::Console>("console");

    for(auto & [key,tm] : ts.translations){
        lgf << key << ":" << tm << endlog;
    }
    string value = "";
    ts.loadTranslation("en_us");

    cout << "NoArgs:" << ts.translate("test") << endl;
    cout << "Args:" << ts.translate_args("test",value,0,"HelloWorld") << endl;
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
    Util::io_printColor("TestRBL ",ACP_RED ACP_BG_BLACK);
    Util::io_printColor("TestRB ",ACP_RED  ACP_BG_BLUE);
    Util::io_printColor("TestRW+ ",ACP_RED  ACP_BG_LWHITE);
    Util::io_printColor("TestRC ",ACP_RED  ACP_BG_CYAN);
    Util::io_printColor("TestRGRAY ",ACP_RED  ACP_BG_GRAY);
    Util::io_printColor("TestRG ",ACP_RED  ACP_BG_GREEN);
    Util::io_printColor("TestRB+ ",ACP_RED  ACP_BG_LBLUE);
    Util::io_printColor("TestRCYAN+ ",ACP_RED  ACP_BG_LCYAN);
    Util::io_printColor("TestRM+ ",ACP_RED  ACP_BG_LMAGENTA);
    Util::io_printColor("TestRR+ ",ACP_RED  ACP_BG_LRED);
    Util::io_printColor("TestRY+ ",ACP_RED  ACP_BG_LYELLOW);
    Util::io_printColor("TestRR ",ACP_RED  ACP_BG_RED);
    Util::io_printColor("TestRW ",ACP_RED  ACP_BG_WHITE);
    Util::io_printColor("TestRY ",ACP_RED  ACP_BG_YELLOW);
    Util::io_printColor("TestRM ",ACP_RED  ACP_BG_MAGENTA);
    Util::io_printColor("TestBLW ",ACP_BLACK  ACP_BG_WHITE);
    Util::io_printColor("TestWBL ",ACP_WHITE  ACP_BG_BLACK);
    Util::io_printColor("TestBBL ",ACP_BLUE  ACP_BG_BLACK);
    Util::io_printColor("TestW+BL ",ACP_LWHITE  ACP_BG_BLACK);
    Util::io_printColor("TestCYANBL ",ACP_CYAN  ACP_BG_BLACK);
    Util::io_printColor("TestGRAYBL ",ACP_GRAY  ACP_BG_BLACK);
    Util::io_printColor("TestGBL ",ACP_GREEN  ACP_BG_BLACK);
    Util::io_printColor("TestB+BL ",ACP_LBLUE  ACP_BG_BLACK);
    Util::io_printColor("TestCYAN+BL ",ACP_LCYAN  ACP_BG_BLACK);
    Util::io_printColor("TestG+BL ",ACP_LGREEN  ACP_BG_BLACK);
    Util::io_printColor("TestM+BL ",ACP_LMAGENTA  ACP_BG_BLACK);
    Util::io_printColor("TestR+BL ",ACP_LRED  ACP_BG_BLACK);
    Util::io_printColor("TestY+BL ",ACP_LYELLOW  ACP_BG_BLACK);
    Util::io_printColor("TestM+BL ",ACP_MAGENTA  ACP_BG_BLACK);
    Util::io_printColor("TestRBL ",ACP_RED  ACP_BG_BLACK);
    Util::io_printColor("TestWBL ",ACP_WHITE  ACP_BG_BLACK);
    Util::io_printColor("TestYBL ",ACP_YELLOW  ACP_BG_BLACK);
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


    cout << "\e[100m    io_traverseFiles (Loop No Slash test_data fixed_append/)\e[0m" << endl;
    {
        vector<string> files;
        Util::io_traverseFiles("test_data",files,-1,"fixed_append/");
        for(auto & s : files){
            cout << s << "\n";
        }
        cout << endl;
    }
    cout << "\e[100m    io_traverseFiles (Loop Slash test_data/ fixed_append/)\e[0m" << endl;
    {
        vector<string> files;
        Util::io_traverseFiles("test_data/",files,-1,"fixed_append/");
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
