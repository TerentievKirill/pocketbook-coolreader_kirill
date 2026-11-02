#include "../cr3gui/src/ota_version.h"
#include <cassert>
int main() {
 unsigned p[3];
 assert(cr3ota::parseVersion("1.0.0", p));
 const char *bad[] = {"", "1.0", "1.0.0.", "1.0.0.1", "01.0.0", "1.-1.0", "1.0.0-beta", "9999999.0.0", "1.0.0/evil"};
 for (auto s : bad) assert(!cr3ota::parseVersion(s,p));
 assert(cr3ota::newer("1.10.0", "1.9.9"));
 assert(cr3ota::newer("2.0.0", "1.999.999"));
 assert(!cr3ota::newer("1.0.0", "1.0.0"));
 assert(!cr3ota::newer("0.9.9", "1.0.0"));
}
