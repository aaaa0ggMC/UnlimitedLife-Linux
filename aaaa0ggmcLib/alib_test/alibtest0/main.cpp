#include <iostream>
#include <fstream>
#include <vector>
#include <alib-g3/autil.h>
#include <alib-g3/alogger.h>
#include <string.h>
#include <glm/glm.hpp>
#include <thread>
#include <chrono>

using namespace std;
using namespace alib::ng;

void test_autil();
void test_alogger();

int main(int argc,const char * argv[])
{
    if(argc < 2){
        cout << "util      Test autil.h" << endl;
        cout << "logger    Test alogger.h" << endl;
        return 0;
    }
    if(!strcmp("util",argv[1])){
        test_autil();
    }else if(!strcmp("logger",argv[1])){
        test_alogger();
    }
    return 0;
}

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
    logger.toggleConsoleOutput(false);
    lg << "HelloWorld!" << endlog;
    logger.toggleConsoleOutput(true);

    cout << "Multithread Output Test" << endl;
    std::thread th([&]{
        for(unsigned int i = 0;i < 16;++i){
            lg.info(string("HelloWorld!Th") + to_string(i));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    for(unsigned int i = 0;i < 17;++i){
        lg.info(string("HelloWorld!Main") + to_string(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    th.join();

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
    cout << "\e[100m    sys_GetCPUId\e[0m" << endl;
    cout << Util::sys_GetCPUId() << endl;

    cout << "\e[100m    sys_getGlobalMemoryUsage\e[0m" << endl;
    GlMem mem = Util::sys_getGlobalMemoryUsage();
    cout << "All Page Memory:" << mem.page_all << endl;
    cout << "Used Page Memory:" << mem.page_used << endl;
    cout << "All Phy Memory:" << mem.phy_all << endl;
    cout << "Used Phy Memory:" << mem.phy_used << endl;
    cout << "Phy Memory Percent:" << mem.percent << endl;
    cout << "All Virt Memory:" << mem.vir_all << endl;
    cout << "Used Virt Memory:" << mem.vir_used << endl;


    cout << "\e[100m    sys_getProgramMemoryUsage\e[0m" << endl;
    MemTp mtp =  Util::sys_getProgramMemoryUsage();
    cout << "Memory Usage:" << mtp.mem << endl;
    cout << "VMemory Usage:" << mtp.vmem << endl;
}
