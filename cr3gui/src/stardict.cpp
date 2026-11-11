#include "stardict.h"
#include "lvstring.h"
#include "lvxmlutils.h"
#include "pbdictionary.h"
#include <zlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <algorithm>
#include <cstdint>
#include <climits>
#include <cstring>
#include <map>
#include <set>
#include <limits>
namespace stardict {
static const size_t indexLimit = 256 * 1024 * 1024, articleLimit = 1024 * 1024;
static uint32_t be32(const unsigned char *p) { return uint32_t(p[0])<<24 | uint32_t(p[1])<<16 | uint32_t(p[2])<<8 | p[3]; }
static unsigned le16(const unsigned char *p) { return p[0] | unsigned(p[1])<<8; }
static uint32_t le32(const unsigned char *p) { return le16(p) | uint32_t(le16(p+2))<<16; }
static bool ends(const std::string &s, const std::string &tail) { return s.size()>=tail.size() && s.compare(s.size()-tail.size(),tail.size(),tail)==0; }
static int ci(const char *a, const char *b) {
    for (;;++a,++b) {
        unsigned x=static_cast<unsigned char>(*a),y=static_cast<unsigned char>(*b);
        if (x>='A'&&x<='Z') x+=32;
        if (y>='A'&&y<='Z') y+=32;
        if (x!=y || !x) return x<y ? -1 : x>y ? 1 : 0;
    }
}
static bool number(const std::string &s, uint64_t &n) {
    if (s.empty() || s.size()>20) return false;
    n=0; for (unsigned char c:s) { if(c<'0'||c>'9'||n>(UINT64_MAX-(c-'0'))/10) return false; n=n*10+c-'0'; }
    return true;
}
static bool metadata(const std::string &path, std::map<std::string,std::string> &out) {
    FILE *f=fopen(path.c_str(),"rb"); if(!f) return false;
    char buffer[65537]; size_t n=fread(buffer,1,sizeof(buffer),f); bool ok=!ferror(f); fclose(f);
    if(!ok||n>65536||!n||memchr(buffer,0,n)) return false;
    std::string text(buffer,n); size_t pos=0,end=text.find('\n');
    std::string magic=text.substr(0,end); if(!magic.empty()&&magic.back()=='\r')magic.pop_back();
    if(magic!="StarDict's dict ifo file"||end==std::string::npos) return false;
    pos=end+1;
    while(pos<text.size()) {
        end=text.find('\n',pos); if(end==std::string::npos)end=text.size();
        std::string line=text.substr(pos,end-pos);if(!line.empty()&&line.back()=='\r')line.pop_back();
        size_t eq=line.find('='); if(eq!=std::string::npos&&!out.emplace(line.substr(0,eq),line.substr(eq+1)).second)return false;
        pos=end+1;
    }
    return (out["version"]=="2.4.2"||out["version"]=="3.0.0")&&!out["bookname"].empty()&&out["bookname"].size()<512;
}
static void scan(const std::string &dir,int depth,std::vector<Info> &out) {
    if(depth>5||out.size()>=512)return;
    DIR *d=opendir(dir.c_str());if(!d)return;
    while(auto *e=readdir64(d)) {
        std::string name=e->d_name;if(name.empty()||name[0]=='.'||name=="res")continue;
        std::string path=dir+"/"+name;struct stat64 st;if(lstat64(path.c_str(),&st))continue;
        if(S_ISDIR(st.st_mode))scan(path,depth+1,out);
        else if(S_ISREG(st.st_mode)&&ends(name,".ifo")) {
            std::map<std::string,std::string> m;if(metadata(path,m))out.push_back({m["bookname"],path});
        }
        if(out.size()>=512)break;
    }
    closedir(d);
}
std::vector<Info> discover(const std::string &directory) {
    std::vector<Info> out;scan(directory,0,out);
    std::sort(out.begin(),out.end(),[](const Info&a,const Info&b){return a.path<b.path;});return out;
}
std::string escape(const std::string &s) {
    std::string out;for(char c:s) { switch(c){case '&':out+="&amp;";break;case '<':out+="&lt;";break;case '>':out+="&gt;";break;case '\n':out+="<br/>";break;default:if(static_cast<unsigned char>(c)>=32||c=='\t')out+=c;} }return out;
}
std::string preview(const std::string &html) {
    return UnicodeToUtf8(PbDictionary::preview(html.c_str())).c_str();
}
// Mapped indexes use one small offset per 32 records, rather than a heap object
// for every word or inflection. File-backed pages can be reclaimed by the OS.
class Index {
    int fd=-1; const unsigned char *data=NULL;size_t length=0;bool mapped=false;
    std::vector<unsigned char> unpacked;std::vector<uint32_t> pages;
    unsigned tail=0;uint64_t count=0;
public:
    ~Index(){if(mapped)munmap(const_cast<unsigned char*>(data),length);if(fd>=0)close(fd);}
    struct Record { const char *word; uint64_t offset; uint32_t size; };
    bool open(const std::string &path,unsigned recordTail,uint64_t expected,uint64_t expectedBytes,uint64_t bound) {
        tail=recordTail;struct stat64 st;
        if(!stat64(path.c_str(),&st)) {
            if(!S_ISREG(st.st_mode)||st.st_size<0||uint64_t(st.st_size)>indexLimit)return false;
            if(!st.st_size)return expected==0&&!expectedBytes;
            fd=::open(path.c_str(),O_RDONLY);if(fd<0)return false;length=st.st_size;
            void *p=mmap(NULL,length,PROT_READ,MAP_PRIVATE,fd,0);if(p==MAP_FAILED)return false;
            data=static_cast<const unsigned char*>(p);mapped=true;
        } else {
            gzFile f=gzopen((path+".gz").c_str(),"rb");if(!f)return false;
            unsigned char buffer[65536];int n;
            while((n=gzread(f,buffer,sizeof(buffer)))>0) {
                if(unpacked.size()+n>indexLimit){gzclose(f);return false;}
                unpacked.insert(unpacked.end(),buffer,buffer+n);
            }
            bool ok=n==0&&gzeof(f);if(gzclose(f)!=Z_OK||!ok)return false;
            length=unpacked.size();data=unpacked.data();
        }
        if(!length)return expected==0&&!expectedBytes;
        if((expectedBytes&&length!=expectedBytes)||expected>16000000)return false;
        size_t pos=0;const char *previous=NULL;
        while(pos<length) {
            const unsigned char *end=static_cast<const unsigned char*>(memchr(data+pos,0,std::min(size_t(1025),length-pos)));
            if(!end||end==data+pos||size_t(end-data)+1+tail>length)return false;
            const char *word=reinterpret_cast<const char*>(data+pos);
            if(previous){int order=ci(previous,word);if(order>0||(order==0&&strcmp(previous,word)>0))return false;}
            const unsigned char *record=end+1;uint64_t off=be32(record);
            if(tail==12)off=off<<32|be32(record+4);
            if(tail==4){if(off>=bound)return false;}
            else if(off>bound||be32(record+tail-4)>bound-off)return false;
            if(!(count%32))pages.push_back(pos);
            previous=word;pos=(end-data)+1+tail;++count;
        }
        return count==expected;
    }
    uint64_t size()const{return count;}
    Record at(uint64_t i)const {
        if(i>=count)return {NULL,0,0};
        size_t pos=pages[i/32];for(unsigned j=0;j<i%32;++j)pos+=strlen(reinterpret_cast<const char*>(data+pos))+1+tail;
        const char *word=reinterpret_cast<const char*>(data+pos);const unsigned char *p=data+pos+strlen(word)+1;
        if(tail==4)return {word,be32(p),0};
        uint64_t off=be32(p);if(tail==12)off=off<<32|be32(p+4);
        return {word,off,be32(p+tail-4)};
    }
    uint64_t lower(const std::string &query)const {
        uint64_t a=0,b=count;while(a<b){uint64_t m=a+(b-a)/2;if(ci(at(m).word,query.c_str())<0)a=m+1;else b=m;}return a;
    }
};
class Data {
    FILE *file=NULL;uint64_t length=0;bool compressed=false;
    unsigned chunkSize=0;std::vector<uint64_t> offsets;std::vector<unsigned> sizes;
    std::string cached;unsigned cachedChunk=UINT_MAX;
    bool readAt(uint64_t pos,void *out,size_t n){return pos<=uint64_t(INT64_MAX)&&!fseeko64(file,pos,SEEK_SET)&&fread(out,1,n,file)==n;}
public:
    ~Data(){if(file)fclose(file);}
    uint64_t size()const{return length;}
    bool open(const std::string &base) {
        file=fopen((base+".dict").c_str(),"rb");
        if(file){if(fseeko64(file,0,SEEK_END))return false;auto n=ftello64(file);if(n<0)return false;length=n;return true;}
        file=fopen((base+".dict.dz").c_str(),"rb");if(!file)return false;
        if(fseeko64(file,0,SEEK_END))return false;auto total=ftello64(file);if(total<20)return false;
        unsigned char head[12],footer[8];if(!readAt(0,head,12)||head[0]!=31||head[1]!=139||head[2]!=8||(head[3]&0xe0)||!(head[3]&4))return false;
        unsigned extra=le16(head+10);std::vector<unsigned char> fields(extra);
        if(!readAt(12,fields.data(),fields.size()))return false;
        bool found=false;size_t p=0;
        while(p+4<=fields.size()) {
            unsigned n=le16(fields.data()+p+2);if(n>fields.size()-p-4)return false;
            if(fields[p]=='R'&&fields[p+1]=='A') {
                if(found||n<6||le16(fields.data()+p+4)!=1)return false;
                found=true;chunkSize=le16(fields.data()+p+6);unsigned count=le16(fields.data()+p+8);
                if(!chunkSize||!count||n!=6+count*2)return false;
                for(unsigned i=0;i<count;++i){unsigned size=le16(fields.data()+p+10+i*2);if(!size)return false;sizes.push_back(size);}
            }
            p+=4+n;
        }
        if(p!=fields.size()||!found)return false;
        uint64_t start=12+extra;
        for(unsigned flag: {8u,16u})if(head[3]&flag) {
            unsigned char c;unsigned n=0;do{if(++n>4096||!readAt(start++,&c,1))return false;}while(c);
        }
        if(head[3]&2)start+=2;
        for(unsigned n:sizes){offsets.push_back(start);start+=n;}
        if(start>uint64_t(total)-8||!readAt(total-8,footer,8))return false;
        length=le32(footer+4);
        if(!length||((length+chunkSize-1)/chunkSize)!=sizes.size())return false;
        compressed=true;return true;
    }
    bool read(uint64_t offset,size_t size,std::string &out) {
        out.clear();if(offset>length||size>length-offset||size>articleLimit)return false;
        if(!compressed){out.resize(size);return readAt(offset,&out[0],size);}
        while(size) {
            unsigned chunk=offset/chunkSize,within=offset%chunkSize;
            if(chunk!=cachedChunk) {
                std::vector<unsigned char> packed(sizes[chunk]);if(!readAt(offsets[chunk],packed.data(),packed.size()))return false;
                size_t expected=std::min(uint64_t(chunkSize),length-uint64_t(chunk)*chunkSize);
                cached.assign(expected,'\0');z_stream z={};if(inflateInit2(&z,-MAX_WBITS)!=Z_OK)return false;
                z.next_in=packed.data();z.avail_in=packed.size();z.next_out=reinterpret_cast<unsigned char*>(&cached[0]);z.avail_out=cached.size();
                int rc=inflate(&z,Z_FINISH);bool ok=(rc==Z_STREAM_END||rc==Z_OK||rc==Z_BUF_ERROR)&&z.total_out==expected;
                inflateEnd(&z);if(!ok){cachedChunk=UINT_MAX;return false;}cachedChunk=chunk;
            }
            size_t n=std::min(size,cached.size()-within);if(!n)return false;
            out.append(cached,within,n);offset+=n;size-=n;
        }
        return true;
    }
};
static bool fields(const std::string &input,const std::string &sequence,std::string &html) {
    size_t pos=0,typeIndex=0;html.clear();
    while(pos<input.size()) {
        char type;bool last=false;
        if(sequence.empty())type=input[pos++];else{if(typeIndex>=sequence.size())return false;type=sequence[typeIndex++];last=typeIndex==sequence.size();}
        size_t n;
        if(last)n=input.size()-pos;
        else if(type>='A'&&type<='Z'){if(input.size()-pos<4)return false;n=be32(reinterpret_cast<const unsigned char*>(input.data()+pos));pos+=4;}
        else {size_t end=input.find('\0',pos);if(end==std::string::npos)return false;n=end-pos;}
        if(n>input.size()-pos)return false;
        if(type>='a'&&type<='z') {
            std::string text=input.substr(pos,n);
            if(!html.empty())html+="<br/>";
            if(type=='h'||type=='g'||type=='x')html+=text;else html+=escape(text);
        }
        pos+=n;if(!last&&!(type>='A'&&type<='Z'))++pos;
    }
    return sequence.empty()||typeIndex==sequence.size();
}
class Dictionary::Impl {
public:
    Index index,syn;Data data;std::string sequence,failure;uint64_t cursor=0;bool hasSyn=false;
    bool article(uint64_t i,std::string &word,std::string &html) {
        auto r=index.at(i);if(!r.word)return false;std::string raw;
        if(!data.read(r.offset,r.size,raw)||!fields(raw,sequence,html))return false;
        word=r.word;cursor=i;return true;
    }
};
Dictionary::Dictionary():impl(new Impl()){}
Dictionary::~Dictionary(){}
const std::string &Dictionary::error()const{return impl->failure;}
bool Dictionary::open(const std::string &path) {
    impl.reset(new Impl());auto &d=*impl;d.failure="This StarDict dictionary is incomplete or uses an unsupported format.";
    std::map<std::string,std::string> meta;if(!ends(path,".ifo")||!metadata(path,meta))return false;
    uint64_t words,bytes,bits=32,synonyms=0;
    if(!number(meta["wordcount"],words)||!words||!number(meta["idxfilesize"],bytes))return false;
    if(meta.count("idxoffsetbits")&&(!number(meta["idxoffsetbits"],bits)||(bits!=32&&bits!=64)))return false;
    std::string base=path.substr(0,path.size()-4);d.sequence=meta["sametypesequence"];
    if(d.sequence.size()>32||!d.data.open(base)||!d.index.open(base+".idx",bits==64?12:8,words,bytes,d.data.size()))return false;
    for(char c:d.sequence)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')))return false;
    struct stat64 st;
    if(!stat64((base+".syn").c_str(),&st)||!stat64((base+".syn.gz").c_str(),&st)) {
        if(!number(meta["synwordcount"],synonyms)||!d.syn.open(base+".syn",4,synonyms,0,words))return false;
        d.hasSyn=true;
    } else if(meta.count("synwordcount")&&(!number(meta["synwordcount"],synonyms)||synonyms))return false;
    d.failure.clear();return true;
}
bool Dictionary::lookup(const std::string &query,bool exact,std::string &word,std::string &html) {
    auto &d=*impl;if(!d.failure.empty()||query.empty()||query.size()>1024)return false;
    std::vector<std::string> variants{query};lString32 q=Utf8ToUnicode(query.c_str());q.lowercase();variants.push_back(UnicodeToUtf8(q).c_str());
    q.uppercase();variants.push_back(UnicodeToUtf8(q).c_str());q.lowercase();if(!q.empty()){lString32 first=q.substr(0,1);first.uppercase();q=first+q.substr(1);variants.push_back(UnicodeToUtf8(q).c_str());}
    std::set<uint64_t> records;
    for(auto &v:variants) {
        uint64_t n=d.index.lower(v);for(unsigned count=0;n<d.index.size()&&count<64&&ci(d.index.at(n).word,v.c_str())==0;++n,++count)records.insert(n);
        if(d.hasSyn){n=d.syn.lower(v);for(unsigned count=0;n<d.syn.size()&&count<64&&ci(d.syn.at(n).word,v.c_str())==0;++n,++count)records.insert(d.syn.at(n).offset);}
        if(!records.empty())break;
    }
    if(records.empty()&&!exact){uint64_t n=d.index.lower(query);if(n<d.index.size())records.insert(n);}
    html.clear();bool found=false;
    for(uint64_t n:records){std::string term,body;if(!d.article(n,term,body))continue;if(!found)word=term;if(html.size()+body.size()>articleLimit)break;if(found)html+="<hr/>";html+=body;found=true;}
    if(found&&html.empty())html="<p>This entry contains no displayable text.</p>";
    return found;
}
bool Dictionary::move(int direction,std::string &word,std::string &html) {
    auto &d=*impl;if(!d.failure.empty()||(direction<0&&!d.cursor)||(direction>0&&d.cursor+1>=d.index.size()))return false;
    return d.article(direction<0?d.cursor-1:d.cursor+1,word,html);
}
}
