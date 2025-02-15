#include <iostream>
#include <stdlib.h>
#include <alib-g3/adata.h>
#include <alib-g3/alogger.h>
#include <alib-g3/aparser.h>
#include <fstream>
#include <unordered_map>
#include <time.h>
#include <string.h>
#include <algorithm>
#include <format>

using namespace alib::g3;

#define BLOCK(X)

struct Config{
    std::string player;//mpv suggested
    std::string vidpath;//last time
    bool verbose;
};

//type
//2 normal video.m4s audio.m4s
//1 flash 1.flv 2.flv
struct Media{
    std::string subdir;
    int type;
    std::string video;
    std::string audio;
    std::vector<std::string> flvs;
    std::string videoName;
    std::string dir;
    int page;

    Media(){
        type = 2;
        subdir = video = audio = dir = "";
        flvs = {};
    }
};

struct MediaGroup{
    std::string bvid;
    std::string groupName;
    std::string author;
    std::vector<Media> media;
};

Config cfg;
std::string configp;
const char * homep;
Logger logger;
LogFactory lg("BILI",logger);
std::unordered_map<std::string,MediaGroup> med;
std::vector<std::string> files;

void LoadVideos(const std::string& path);

int main(){
    srand(time(0));
    homep = getenv("HOME");
    configp = std::string(homep) + "/.config/";
    logger.appendLogOutputTarget("console",std::make_shared<log_output_targets::Console>());
    BLOCK(CheckConfig){
        //检查是否有文件夹
        if(!Util::io_checkExistence(configp + "simp-biliplayer")){
            system((std::string("mkdir '") + configp + "simp-biliplayer'").c_str());
            lg << "Executed " << std::string("mkdir '") + configp + "simp-biliplayer'" << endlog;
        }
        configp += "simp-biliplayer/";
        logger.appendLogOutputTarget("logFile",std::make_shared<log_output_targets::SingleFile>(configp + "log"));

        //default values
        cfg.player = "mpv";
        cfg.vidpath = "";
        cfg.verbose = false;

        //获取.config文件
        GDoc doc;
        if(doc.read_parseFileTOML(configp + "config.properties") != ALIB_EHAS_PARSE_ERROR){
            auto a = doc["General.player"];
            if(!!a)cfg.player = *a;
            a = doc["General.vidPath"];
            if(!!a)cfg.vidpath = *a;
            a = doc["General.verbose"];
            if(!!a){
                if(!strcmp(*a,"1")){
                    cfg.verbose = true;
                }else cfg.verbose = false;
            }
        }
    }
    logger.setLogOutputTargetStatus("console",cfg.verbose);

    atexit([]{
        std::ofstream ofs(configp + "config.properties");
        if(ofs.bad())return;
        ofs << "[General]" << "\n";
        ofs << "player = '" << cfg.player << "'\n";
        ofs << "vidPath = '" << cfg.vidpath << "'\n";
        ofs << "verbose = " << (cfg.verbose?"true":"false") << "\n";
        ofs.close();
    });

    if(cfg.vidpath.compare("")){
        std::vector<std::string> prePaths;
        Util::str_split(cfg.vidpath,';',prePaths);
        for(const auto & v : prePaths){
            LoadVideos(v);
            std::cout << "Loaded " << v << std::endl;
        }
    }

    std::string command;
    Parser parser;
    std::string head;
    std::vector<string> sep_args;
    std::string args;
    std::vector<std::string> cached;
    BLOCK(Display){
    while(true){
        Util::io_printColor("Enter command(",APCF_BLUE);
        Util::io_printColor(std::to_string(med.size()) + " videos loaded",APCF_YELLOW);
        Util::io_printColor("):",APCF_BLUE);
        std::getline(std::cin,command);
        sep_args.clear();
        args = "";
        head = "";
        parser.ParseCommand(command,head,args,sep_args);
        if(!head.compare("show") || !head.compare("s") || !head.compare("list") || !head.compare("ls")){
            bool random = false;
            int maxiumCount = 0;
            int beginV = 0;
            cached.clear();
            BLOCK(Show All Vids){
                std::string authft = "",titleft = "";
                if(sep_args.size() != 0){
                    unsigned int indice = 0;
                    bool matched;
                    while(indice < sep_args.size()){
                        matched = true;
                        if(!(sep_args[indice].compare("-a")) && sep_args.size() > ++indice)authft = sep_args[indice++];
                        else if(!(sep_args[indice].compare("-t")) && sep_args.size() > ++indice)titleft = sep_args[indice++];
                        else if(!(sep_args[indice].compare("-m")) && sep_args.size() > ++indice)maxiumCount = atoi(sep_args[indice++].c_str());
                        else if(!(sep_args[indice].compare("-b")) && sep_args.size() > ++indice)beginV = atoi(sep_args[indice++].c_str());
                        else if((!(sep_args[indice].compare("-r"))))random = true;
                        else matched = false;
                        ++indice;
                    }

                    if(beginV < 0)beginV = 0;

                    if(!matched){
                        titleft = authft = sep_args[0];
                    }
                }
                if(random){
                    std::vector<unsigned int> vidList;
                    std::vector<MediaGroup*> vecMeds;
                    for(auto & [_,gp] : med)vecMeds.push_back(&gp);
                    for(unsigned int i = 0;i < (maxiumCount<=0?med.size():maxiumCount);){
                        unsigned int newV = rand() % med.size();
                        //check conflict
                        if(std::find(vidList.begin(),vidList.end(),newV) == vidList.end()){
                            vidList.push_back(newV);
                            Util::io_printColor(std::string("[") + std::to_string(cached.size()) + "] ",APCF_GREEN);
                            std::cout << vecMeds[newV]->bvid << " " << vecMeds[newV]->groupName << " ";
                            cached.push_back(vecMeds[newV]->bvid);
                            Util::io_printColor(vecMeds[newV]->author,APCF_YELLOW);
                            std::cout << std::endl;
                            ++i;
                        }
                    }
                    continue;
                }
                int recorder = -beginV;
                for(auto & [_,gp] : med){
                    bool tvalid = titleft.compare("");
                    bool avalid = authft.compare("");
                    if(tvalid|avalid){
                        bool resulta = (avalid && Util::str_toUpper(gp.author).find(Util::str_toUpper(authft)) == std::string::npos);
                        bool resultt = (tvalid && Util::str_toUpper(gp.groupName).find(Util::str_toUpper(titleft)) == std::string::npos);
                        if((resulta || !avalid) && (resultt || !tvalid))continue;
                    }
                    recorder++;
                    if(recorder <= 0)continue;
                    else if(recorder > (maxiumCount<=0?med.size():maxiumCount))break;
                    Util::io_printColor(std::string("[") + std::to_string(cached.size()) + "] ",APCF_GREEN);
                    std::cout << gp.bvid << " " << gp.groupName << " ";
                    cached.push_back(gp.bvid);
                    Util::io_printColor(gp.author,APCF_YELLOW);
                    std::cout << std::endl;
                }
            }
        }else if(!head.compare("exit") || !head.compare("q")){
            break;
        }else if(!head.compare("detail") || !head.compare("d")){
            if(sep_args.size() == 0){
                std::cerr << "One more arg is needed!" << std::endl;
                continue;
            }
            auto it = med.find(sep_args[0]);
            if(it == med.end()){
                char * ptr = (char*)sep_args[0].c_str();
                int value = strtol(sep_args[0].c_str(),&ptr,0);
                if(ptr == sep_args[0].c_str()){
                    continue;
                }else{
                    //check cache
                    if(cached.size() == 0){
                        Util::io_printColor("No cache to read!\n",APCF_RED);
                        continue;
                    }
                    if(value < 0 || value >= cached.size())value = 0;
                    it = med.find(cached[value]);
                    if(it == med.end())continue;
                }
            }
            MediaGroup & mg = it->second;
            std::cout << "Detail of " << mg.bvid << " " << mg.groupName  << " " << mg.author << std::endl;
            for(unsigned int i = 0;i < mg.media.size();++i){
                std::cout << "[" << i << "]" << mg.media[i].videoName << "\n";
            }
        }else if(!head.compare("play") || !head.compare("p")){
            if(sep_args.size() == 0){
                std::cerr << "One more arg is needed!" << std::endl;
                continue;
            }
            auto it = med.find(sep_args[0]);
            if(med.size() <= 0)continue;
            if(it == med.end()){
                char * ptr = (char*)sep_args[0].c_str();
                int value = strtol(sep_args[0].c_str(),&ptr,0);
                if(ptr == sep_args[0].c_str()){
                    continue;
                }else{
                    //check cache
                    if(cached.size() == 0){
                        Util::io_printColor("No cache to read!\n",APCF_RED);
                        continue;
                    }
                    if(value < 0 || value >= cached.size())value = rand() % cached.size();
                    it = med.find(cached[value]);
                    if(it == med.end())continue;
                }
            }
            MediaGroup & mg = it->second;
            Media * tg = NULL;
            if(mg.media.size() == 0){
                std::cout << "Media is empty!" << std::endl;
            }
            if(mg.media.size() == 1){
                tg = & mg.media[0];
            }else{
                if(sep_args.size() < 2){
                    tg = & mg.media[0];
                }
                int index = atoi(sep_args[1].c_str());
                if(index < 0 || index >= mg.media.size())index = rand() % mg.media.size();
                tg = & mg.media[index];
            }

            //make commands
            std::string cmd = cfg.player;
            if(tg->type == 2){
                lg << "Normal(M4S) videos" << endlog;
                cmd += " '" + tg->video + "' --audio-file='" + tg->audio + "'";
                //system(cmd.c_str());
            }else if(tg->type == 1){
                lg << "Flv videos ct:" << tg->flvs.size() <<endlog;
                std::string app = tg->dir + tg->subdir + "/";
                //cout << app << endl;
                //system("read");
                for(auto & isn : tg->flvs){
                    //std::cout << app << isn << std::endl;
                    cmd += " '" + app + isn + "'";
                }
            }
            cmd += " --title='";
            cmd += tg->videoName;
            if(mg.media.size() > 1){
                cmd += " - " + mg.groupName;
            }
            cmd += "' ";
            if(sep_args.size() >= 3){
                cmd += sep_args[2];
            }
            std::cout << "Now playing " << mg.bvid << " " << mg.groupName << " " << tg->videoName << std::endl;
            lg << "Execute command:" << cmd << endlog;
            system(cmd.c_str());
        }else if(!head.compare("verbose") || !head.compare("v")){
            cfg.verbose = !cfg.verbose;
            logger.setLogOutputTargetStatus("console",cfg.verbose);
            std::cout << "Verbose:";
            if(cfg.verbose)Util::io_printColor("On",APCF_GREEN);
            else Util::io_printColor("Off",APCF_GRAY);
            std::cout << std::endl;
        }else if(!head.compare("cache") || !head.compare("c")){
            for(unsigned int i =0;i < cached.size();++i){
                auto it = med.find(cached[i]);
                if(it != med.end()){
                    MediaGroup&gp = it->second;
                    Util::io_printColor(std::string("[") + std::to_string(i) + "] ",APCF_GREEN);
                    std::cout << gp.bvid << " " << gp.groupName << " ";
                    Util::io_printColor(gp.author,APCF_YELLOW);
                    std::cout << std::endl;
                }else{
                    Util::io_printColor(std::string("[") + std::to_string(i) + "] ",APCF_GREEN);
                    Util::io_printColor("missing missing missing",APCF_RED);
                    std::cout << std::endl;
                }
            }
        }else if(!head.compare("load")){
            for(auto & od : sep_args){
                if(od.compare("def"))LoadVideos(od);
                else LoadVideos(cfg.vidpath);
            }
        }else if(!head.compare("clear")){
            med.clear();
            files.clear();
        }else if(!head.compare("player")){
            if(sep_args.size() <= 0){
                std::cout << "Currently:" << cfg.player << std::endl;
                continue;
            }
            cfg.player = sep_args[0];
        }else if(!head.compare("run")){
            if(sep_args.size() <= 0){
                system("");
                continue;
            }
            system(sep_args[0].c_str());
        }else if(!head.compare("defload")){
            if(sep_args.size() <= 0 ){
                std::cout << "Currently:" << cfg.vidpath << std::endl;
                continue;
            }
            ///支持vformat
            try{
                cfg.vidpath = std::vformat(sep_args[0],std::make_format_args(cfg.vidpath));
            }catch(...){
                //处理个屁
            }
        }else if(!head.compare("help") || !head.compare("h")){
            Util::io_printColor("Simple Bilibili Player by aaaa0ggmc ---- Version 0.0.1\n",APCF_YELLOW);
            Util::io_printColor("A set of string wrapped with {} will be considered as one argument.\n"
                                "Like {show sb} hello      =>      \"show sb\" and \"hello\"\n",APCF_BLUE);
            std::cout << "help,h                                |Get help\n";
            std::cout << "show,s,list,ls  [filter][filterString]|Show vids.Filter is optional.'-t' title '-a' author\n";
            std::cout << "                                      |Default for both;Will flush cache\n";
            std::cout << "                                      |-m [display_max_count:0 or below for all]; -r random vids,will flush cache\n";
            std::cout << "                                      |-b [begin at:uint]\n";
            std::cout << "verbose,v                             |Toggles whether logs will be outputed or not.\n";
            std::cout << "cache,c                               |List index-cached videos.\n";
            std::cout << "clear                                 |Clear all loaded videos.\n";
            std::cout << "play,p  [BVID or CacheID] [subvid_id] |Play the video with bvid or cache id.\n";
            std::cout << "                                      |subvid_id is optional.Default for 0.\n";
            std::cout << "detail,d  [BVID or CacheID]           |Play the video with bvid or cache id.\n";
            std::cout << "exit,q                                |Quit.\n";
            std::cout << "load [path0:please wrap with{}]  ...  |Load videos.If path0 == 'def',will load default vids.\n";
            std::cout << "defload [please wrap with{}]          |Set first-load paths,separate with ';' \n";
            std::cout << "                                      |You can wrap with {} or {0} to be replaced with previous values\n";
            std::cout << "                                      |Remember to wrap all the args with an additional {} \n";
            std::cout << "run   [Run terminal commands]         |Run commands,I suggest you warp commands with {}.\n";
            std::cout << "player [command]                      |Change the player default 'mpv'.\n";
            std::cout << "                                      |You can wrap with {} to set some prefixes.\n";

            std::cout << "Sample Output:\n" <<
            "[114] BV15v4y1E7Fr Title Author\n"
            "cacheid    bvid    title author\n";
        }else{
            Util::io_printColor("Unknown command!Enter 'help' or 'h' for help.\n",APCF_RED);
        }
    }
    }
    return 0;
}

void LoadVideos(const std::string& path_in){
    std::string path = path_in;
    BLOCK(Traverse Files){
        std::vector<std::string> filestmp;
        if(path[path.size()-1] != '/')path += '/';
        Util::io_traverseFilesOnly(path,filestmp,2,path);
        lg(LOG_INFO) << "File count:" << filestmp.size() << endlog;
        for(auto & i : filestmp){
            if(i.find("entry.json") != std::string::npos){
                lg(LOG_TRACE) << "Inserted " << i << endlog;
                files.push_back(i);
            }
        }
    }

    lg(LOG_INFO) << "File size " << files.size() << endlog;

    BLOCK(Analyse Videos){
        for(auto & s : files){
            GDoc doc;
            MediaGroup rgp;
            MediaGroup*grp = &rgp;
            Media mdx = Media();
            bool old = false;
            bool season = false;
            int ecode = doc.read_parseFileJSON(s);
            if(ecode == ALIB_EHAS_PARSE_ERROR){
                lg(LOG_ERROR) << "Has error when parsing " << s <<  " Code:" << ecode << endlog;
            }
            auto a = doc["media_type"];
            if(!!a){
                std::string v = *a;
                mdx.type = atoi(v.c_str());
            }
            std::string bvid = "";
            a = doc["bvid"];
            if(!a){
                a = doc["season_id"];
                if(!!a){
                    //cout << "Season" << endl;
                    season = true;
                }else{
                    bvid = std::to_string(std::hash<std::string>()(s));
                    a = bvid.c_str();
                }
            }else bvid = *a;
            if(!season && !Util::str_trim_rt(bvid).compare("")){
                bvid = std::to_string(std::hash<std::string>()(s));
                a = bvid.c_str();
            }
            if(!!a && med.find(*a) != med.end()){
                grp = &(med.find(*a)->second);
                old = true;
            }
            if(!!a)grp->bvid = *a;

            a = doc["title"];
            if(!!a){
                grp->groupName = *a;
            }

            a = doc["page_data.part"];
            if(!!a){
                mdx.videoName = *a;
            }else if(season){
                a = doc["ep.sort_index"];
                //std::cout << *a << std::endl;
                if(!!a)mdx.videoName += *a;
            }

            a = doc["page_data.page"];
            if(!!a){
                mdx.page = atoi(*a);
            }else mdx.page = 1;

            a = doc["type_tag"];
            if(!!a){
                mdx.subdir = *a;
            }

            a = doc["owner_name"];
            if(!!a){
                grp->author = *a;
            }


            std::string path;
            path = s.substr(0,s.find_last_of('/')+1);
            if(mdx.type == 2){
                mdx.video = path + mdx.subdir + "/video.m4s";
                mdx.audio = path + mdx.subdir + "/audio.m4s";
            }else if(mdx.type == 1){
                std::vector<std::string> flvs;
                Util::io_traverseFilesOnly(path+mdx.subdir,flvs,2
                                           );
                lg << path << mdx.subdir << " " << flvs.size() << endlog;
                for(auto & subs : flvs){
                    //std::cout << subs << endl;
                    //system("read");
                    if(subs.find("blv") != std::string::npos){
                        mdx.flvs.push_back(subs);
                    }
                }

                std::sort(mdx.flvs.begin(),mdx.flvs.end(),[](const string & a,const string &b)->bool{
                    int ia = atoi(a.c_str());
                    int ib = atoi(b.c_str());
                    return ia <= ib;
                });
                //std::cout << mdx.flvs.size() << " " <<  countc << endl;
                //system("read");
            }

            mdx.dir = path;

            if(!mdx.videoName.compare("")){
                mdx.videoName = grp->groupName;
            }

            //check conflict
            bool push = true;
            for(auto & media : grp->media){
                if(!media.videoName.compare(mdx.videoName))push = false;
            }
            if(!push){
                lg << "Conflict meida " << grp->bvid << " " << mdx.videoName << endlog;
                continue;
            }
            grp->media.push_back(mdx);
            //arange seasons
            std::sort(grp->media.begin(),grp->media.end(),[](const Media & a,const Media &b)->bool{
                //int ia = atoi(a.videoName.c_str());
                //int ib = atoi(b.videoName.c_str());
                return (a.page <= b.page);// (ia <= ib);
            });
            if(!old){
                med.emplace(grp->bvid,*grp);
            }
            lg(LOG_TRACE) << "Inserted Meida " << grp->bvid << " " << mdx.videoName  << " " << mdx.dir  << " " <<
            mdx.audio << " " << mdx.video << " flashc:" << mdx.flvs.size() << endlog;
        }
    }
}
