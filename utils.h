#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>

namespace sdlnetplay {

void die(const char *text);
ssize_t read_until(int fd, void*buf, size_t count);
ssize_t write_until(int fd, const void *buf, size_t count);
void debug(const char *msg);
}
#endif
