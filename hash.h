#ifndef HASH_H
#define HASH_H

#include <stdint.h>

namespace sdlnetplay {
    uint32_t SuperFastHash(const char *data, int len);
};

#endif
