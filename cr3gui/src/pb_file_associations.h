#ifndef CR3_PB_FILE_ASSOCIATIONS_H
#define CR3_PB_FILE_ASSOCIATIONS_H

#include "lvstring.h"

namespace pbassoc {

enum class Group { All, FictionBook, Epub, Documents, Web, Palm, Text };
enum class Result { Updated, AlreadyDefault, Error };

Result makeDefault(Group group, const lString32 &userConfig, const lString32 &systemConfig);

}

#endif
