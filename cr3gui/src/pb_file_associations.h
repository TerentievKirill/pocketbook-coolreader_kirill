#ifndef CR3_PB_FILE_ASSOCIATIONS_H
#define CR3_PB_FILE_ASSOCIATIONS_H

#include "lvstring.h"

namespace pbassoc {

enum Group : unsigned {
    FictionBook = 1u << 0,
    Epub = 1u << 1,
    Documents = 1u << 2,
    Web = 1u << 3,
    Palm = 1u << 4,
    Text = 1u << 5,
    All = FictionBook | Epub | Documents | Web | Palm | Text,
};
enum class Result { Updated, AlreadyDefault, Error };

Result makeDefault(unsigned groups, const lString32 &userConfig, const lString32 &systemConfig);

}

#endif
