#include "crengine.h"
#include "testutil.h"
#include <unistd.h>
#include <cstdio>
#include <cstring>
#define FLASHDIR "/tmp/cr3-ota-regression"
#include "../cr3gui/src/ota_fw6_core.h"
lString32 pbSkinFileName, openedCacheFile, currentCacheDir;
int main(int argc, char **argv) {
 CHECK(argc == 2);
 otaRemoveTree(FLASHDIR);
 CHECK(mkdir(FLASHDIR,0755)==0);
 CHECK(mkdir(FLASHDIR "/system",0755)==0);
 CHECK(mkdir(FLASHDIR "/system/share",0755)==0);
 const char *cases[]={"good","wrong-version","traversal","missing-binary","wrong-family","wrong-firmware"};
 for (auto name:cases) {
  std::ifstream in(std::string(argv[1])+"/ota-"+name+".zip",std::ios::binary);
  std::string bytes((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
  CHECK(!bytes.empty());
  std::ofstream out(FLASHDIR "/.cr3-ota.zip",std::ios::binary);out.write(bytes.data(),bytes.size());out.close();
  unsigned char digest[32];SHA256((const unsigned char*)bytes.data(),bytes.size(),digest);
  char hex[65];for(int i=0;i<32;++i)sprintf(hex+2*i,"%02x",digest[i]);
  CHECK(!otaStagePackage(std::string(64,'0'),"1.0.0","632","U632.6.11.2001"));
  CHECK(otaStagePackage(hex,"1.0.0","632","U632.6.11.2001") == (strcmp(name,"good")==0
#ifndef CR3_SHARED_PACKAGE
 || strcmp(name,"wrong-family")==0 || strcmp(name,"wrong-firmware")==0
#endif
 ));
  CHECK(access(FLASHDIR "/system/share/escaped",F_OK)!=0);
  if (strcmp(name,"good")==0) {
   CHECK(access(FW6_OTA_STAGE "/bin/cr3-pb.app",X_OK)==0);
   CHECK(access(FW6_OTA_STAGE "/bin/cr3-pb-executable.app",X_OK)==0);
  }
 }
 CHECK(otaRemoveTree(FLASHDIR));
 puts("PASS: OTA SHA-256 rejection, valid extraction, traversal, missing binary and wrong version");
}
