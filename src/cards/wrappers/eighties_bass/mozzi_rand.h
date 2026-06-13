#pragma once
#include <stdlib.h>
inline int rand(int max) {
    if (max <= 0) return 0;
    return ::rand() % max;
}
