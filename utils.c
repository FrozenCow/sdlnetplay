#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>

#include "utils.h"

namespace sdlnetplay {

void die(const char *text) {
    fflush(stdout);
    fflush(stderr);

    fprintf(stderr,"Error: %s, errno: %s\n", text,strerror(errno));
    fflush(stderr);
    abort();
    //exit(EXIT_FAILURE);
}

ssize_t read_until(int fd, void*buf, size_t count) {
    assert(count > 0);
    size_t read_bytes = 0;
    do {
        ssize_t res = read(fd, &((uint8_t*)buf)[read_bytes], count - read_bytes);
        if(res < 0) {
            if(errno == EINTR) {
                continue;
            }
            fprintf(stderr, "bailing on read from thread: %lu \n",  pthread_self());
            return res;
        } else {
            read_bytes += res;
        }
    } while(read_bytes < count);
    return count;
}

ssize_t write_until(int fd, const void *buf, size_t count) {
    assert(count > 0);
    size_t written_bytes = 0;
    do {
        ssize_t res = write(fd, &((uint8_t*)buf)[written_bytes], count - written_bytes);
        if(res < 0) {
            if(errno == EINTR) {
                continue;
            }
            fprintf(stderr, "bailing on write from thread: %lu \n",  pthread_self());
            return res;
        } else {
            written_bytes += res;
        }
    } while(written_bytes < count);
    return count;
}


void debug(const char *msg) {
    fprintf(stderr, "%lu ",  pthread_self());
    fprintf(stderr, "%s\n", msg);
    fflush(stderr);
    fflush(stdout);
}

}
