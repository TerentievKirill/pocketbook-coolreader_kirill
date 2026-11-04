#include "../cr3gui/src/ota_version.h"
#include <cassert>
#include <fstream>
#include <iostream>
int main(int argc,char **argv) {
 if(argc==5 && std::string(argv[1])=="--manifest") {
  std::ifstream file(argv[2]);std::string text((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
  cr3ota::SharedRelease release;
  if(!cr3ota::parseSharedRelease(text,release) || !cr3ota::compatible(release,"pocketbook-armv7-softfp",argv[3],argv[4]))return 2;
  std::cout<<release.version<<" "<<release.checksum<<"\n";return 0;
 }
 unsigned p[3];
 assert(cr3ota::parseVersion("1.0.0", p));
 const char *bad[] = {"", "1.0", "1.0.0.", "1.0.0.1", "01.0.0", "1.-1.0", "1.0.0-beta", "9999999.0.0", "1.0.0/evil"};
 for (auto s : bad) assert(!cr3ota::parseVersion(s,p));
 assert(cr3ota::newer("1.10.0", "1.9.9"));
 assert(cr3ota::newer("2.0.0", "1.999.999"));
 assert(!cr3ota::newer("1.0.0", "1.0.0"));
 assert(!cr3ota::newer("0.9.9", "1.0.0"));
 cr3ota::SharedRelease release;
 std::string descriptor="CR3-OTA-SHARED-1\npocketbook-armv7-softfp\n1.0.3\n"+std::string(64,'a')+"\n632 U632.6.11.2001\n634 U634.6.10.3425\n700 U700.6.10.3209\n743G U743g.6.11.1683\n";
 assert(cr3ota::parseSharedRelease(descriptor,release));
 assert(cr3ota::compatible(release,"pocketbook-armv7-softfp","634","U634.6.10.3425"));
 assert(!cr3ota::compatible(release,"pocketbookhf","634","U634.6.10.3425"));
 assert(!cr3ota::compatible(release,"pocketbook-armv7-softfp","634","U634.6.9.850"));
 assert(!cr3ota::parseSharedRelease(descriptor+"632 U632.6.11.2001\n",release));
 assert(!cr3ota::parseSharedRelease(descriptor+"700",release));
 assert(!cr3ota::parseSharedRelease("1.0.3\n"+std::string(64,'a'),release));
}
