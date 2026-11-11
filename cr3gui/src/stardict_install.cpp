#include "stardict_install.h"
#include "stardict.h"
#include <crengine.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <set>
namespace {
bool removeStage(const std::string &path) {
    DIR *d=opendir(path.c_str());if(!d)return errno==ENOENT;
    bool ok=true;while(auto *e=readdir64(d)){std::string n=e->d_name;if(n!="."&&n!=".."&&unlink((path+"/"+n).c_str()))ok=false;}
    closedir(d);return ok&&!rmdir(path.c_str());
}
bool suffix(const std::string&s,const std::string&tail){return s.size()>=tail.size()&&s.compare(s.size()-tail.size(),tail.size(),tail)==0;}
bool safePath(const std::string &name) {
    if(name.empty()||name.size()>1024||name[0]=='/'||name.find('\\')!=std::string::npos||name.find(':')!=std::string::npos)return false;
    size_t p=0;while(p<name.size()){size_t e=name.find('/',p);if(e==std::string::npos)e=name.size();std::string part=name.substr(p,e-p);if(part.empty()||part=="."||part=="..")return false;p=e+1;}return true;
}
}
class StarDictInstallation::Impl {
public:
    std::string archivePath,stage,target,failure;
    LVContainerRef archive;LVStreamRef input;FILE *output=NULL;
    std::vector<std::pair<int,std::string>> files;size_t index=0;lvsize_t remaining=0;
    bool started=false,done=false,success=false,ownsStage=false;
    Impl(const std::string &a,const std::string &d,const std::string &n):archivePath(a),stage(d+"/.coolreader-stardict-unpack"),target(d+"/"+n){}
    ~Impl(){if(output)fclose(output);input.Clear();archive.Clear();if(ownsStage)removeStage(stage);}
    bool fail(const char *s){failure=s;done=true;return true;}
};
StarDictInstallation::StarDictInstallation(const std::string&a,const std::string&d,const std::string&n):impl(new Impl(a,d,n)){}
StarDictInstallation::~StarDictInstallation(){}
bool StarDictInstallation::succeeded()const{return impl->success;}
const std::string &StarDictInstallation::error()const{return impl->failure;}
bool StarDictInstallation::poll() {
    auto &s=*impl;if(s.done)return true;
    if(!s.started) {
        s.started=true;
        s.archive=LVOpenArchieve(LVOpenFileStream(s.archivePath.c_str(),LVOM_READ));
        if(s.archive.isNull()||s.archive->GetObjectCount()>128)return s.fail("This StarDict download is not a supported ZIP archive.");
        uint64_t total=0;std::set<std::string> names;unsigned ifos=0;
        for(int i=0;i<s.archive->GetObjectCount();++i) {
            auto *info=s.archive->GetObjectInfo(i);std::string path=UnicodeToUtf8(lString32(info->GetName())).c_str();
            if(!safePath(path))return s.fail("The dictionary archive contains an unsafe filename.");
            if(path.back()=='/')continue;
            std::string name=path.substr(path.find_last_of('/')==std::string::npos?0:path.find_last_of('/')+1);
            bool accepted=false;for(const char *ext:{".ifo",".idx",".idx.gz",".dict",".dict.dz",".syn",".syn.gz"})if(suffix(name,ext))accepted=true;
            if(!accepted)continue;
            if(!names.insert(name).second||name.size()>240)return s.fail("The dictionary archive contains conflicting filenames.");
            if(suffix(name,".ifo"))++ifos;
            uint64_t n=info->GetSize();if(n>256*1024*1024)return s.fail("A dictionary file is too large for this reader.");
            total+=n;if(total>512*1024*1024)return s.fail("The unpacked dictionary is too large for this reader.");
            s.files.emplace_back(i,name);
        }
        if(ifos!=1||s.files.size()<3)return s.fail("The archive must contain one complete StarDict dictionary.");
        struct statvfs space;
        std::string parent=s.target.substr(0,s.target.find_last_of('/'));
        if(statvfs(parent.c_str(),&space)||uint64_t(space.f_bavail)*space.f_frsize<total+1024*1024)return s.fail("Not enough free storage to unpack this dictionary.");
        if(!removeStage(s.stage)||mkdir(s.stage.c_str(),0755))return s.fail("Could not prepare the dictionary folder.");
        s.ownsStage=true;
    }
    unsigned char buffer[65536];size_t budget=1024*1024;
    while(s.index<s.files.size()&&budget) {
        if(!s.output) {
            auto *info=s.archive->GetObjectInfo(s.files[s.index].first);
            s.input=s.archive->OpenStream(info->GetName(),LVOM_READ);
            if(s.input.isNull()||s.input->GetSize()!=info->GetSize())return s.fail("Could not unpack the dictionary.");
            s.remaining=s.input->GetSize();s.output=fopen((s.stage+"/"+s.files[s.index].second).c_str(),"wb");
            if(!s.output)return s.fail("Could not save dictionary files. Check the reader's storage.");
        }
        if(s.remaining) {
            lvsize_t n=0,wanted=std::min(lvsize_t(std::min(sizeof(buffer),budget)),s.remaining);
            if(s.input->Read(buffer,wanted,&n)!=LVERR_OK||!n||n>wanted||fwrite(buffer,1,n,s.output)!=n)return s.fail("The dictionary archive is incomplete or could not be saved.");
            s.remaining-=n;budget-=n;
        }
        if(!s.remaining) {
            bool ok=!fflush(s.output)&&!fsync(fileno(s.output));if(fclose(s.output))ok=false;s.output=NULL;s.input.Clear();
            if(!ok)return s.fail("Could not finish saving dictionary files.");
            ++s.index;
        }
    }
    if(s.index<s.files.size())return false;
    auto entries=stardict::discover(s.stage);stardict::Dictionary dictionary;
    if(entries.size()!=1||!dictionary.open(entries[0].path))return s.fail("This StarDict dictionary is incomplete or uses an unsupported format.");
    struct stat64 existing;if(!lstat64(s.target.c_str(),&existing))return s.fail("This dictionary is already installed. Your existing files have been kept.");
    if(rename(s.stage.c_str(),s.target.c_str()))return s.fail("Could not finish installing the dictionary.");
    s.ownsStage=false;s.success=s.done=true;return true;
}
