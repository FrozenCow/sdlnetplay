#include <stdlib.h>
#include <stdio.h>
#include <SDL/SDL.h>
#include <dlfcn.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdbool.h>
#include <netdb.h>
#include "darray.h"
#include "circularbuffer.c"


void sterf(const char *text) {
    fprintf(stderr,"Error: %s\n", text);
    exit(EXIT_FAILURE);
}


ssize_t read_until(int fd, void*buf, size_t count) {
    ssize_t read_bytes = 0;
    do {
        ssize_t res = read(fd, &((uint8_t*)buf)[read_bytes], count - read_bytes);
        if(res < 0) {
            if(errno == EINTR) {
                continue;
            }
            return res;
        } else {
            read_bytes += res;
        }
    } while(read_bytes < count);
    return count;
}

ssize_t write_until(int fd, const void *buf, size_t count) {
    ssize_t written_bytes = 0;
    do {
        ssize_t res = write(fd, &((uint8_t*)buf)[written_bytes], count - written_bytes);
        if(res < 0) {
            if(errno == EINTR) {
                continue;
            }
            return res;
        } else {
            written_bytes += res;
        }
    } while(written_bytes < count);
    return count;
}


typedef int (*SDL_PollEvent_Func)(SDL_Event *event);
typedef void (*SDL_GL_SwapBuffers_Func)(void);
typedef Uint8 (*SDL_GetKeyState_Func)(int *numkeys);
typedef int (*SDL_Init_Func)(Uint32 flags);

static const void * get_func(const char * name)
{
    void * func = dlsym(RTLD_NEXT, name);
/*    if(func == NULL) {
        fprintf(stderr, "Preload error: Failed to get %s symbol.\n", name);
        exit(EXIT_FAILURE);
    }*/
    return func;
}


int fd;
int swap;
int framenumber;
enum { PACKET_SYNC, PACKET_EVENT };
typedef uint8_t PacketType;

typedef struct {
    uint8_t scancode;
    uint32_t sym;
    uint32_t mod;
    uint16_t unicode;
} KeySym;

typedef struct {
    uint8_t type;
    uint8_t state;
    KeySym keysym;
} KeyboardEvent;

typedef darray(KeyboardEvent) darray_event;

SDL_Event toSDLEvent(const KeyboardEvent *event) {
    SDL_Event result;
    result.type = event->type;
    result.key.state = event->state;
    result.key.keysym.scancode = event->keysym.scancode;
    result.key.keysym.sym = event->keysym.sym;
    result.key.keysym.mod = event->keysym.mod;
    result.key.keysym.unicode = event->keysym.unicode;
    return result;
}

KeyboardEvent fromSDLEvent(const SDL_Event *event) {
    KeyboardEvent result;
    result.type = event->type;
    result.state = event->key.state;
    result.keysym.scancode = event->key.keysym.scancode;
    result.keysym.sym = event->key.keysym.sym;
    result.keysym.mod = event->key.keysym.mod;
    result.keysym.unicode = event->key.keysym.unicode;
    return result;
}

void netplay_listen() {
    swap = 0;
    int serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0) {
        sterf("Error creating socket");
    }
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8008);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    int optval = 1;
    setsockopt(serverfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    if (bind(serverfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        sterf("Error binding socket");
    }
    if (listen(serverfd,1) < 0) {
        sterf("Error listening on socket");
    }
    


    fprintf(stdout,"Accepting clients...\n");
    fd = accept(serverfd, (struct sockaddr *) NULL, NULL);
    if (fd < 0) {
        sterf("Error accepting socket");
    }
    fprintf(stdout,"Accepted client.\n");
}


static inline double intnoise(uint64_t x) {
        x = (x << 13) ^ x;
            return (1.0 - ((x * (x * x * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0);
}





static inline uint32_t ufindnoise2(int y) {
    int n = y * 57;
    n = (n << 13) ^ n;
    uint32_t nn = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
    return nn;
}
#include <math.h>
#include <limits.h>
uint32_t v = 0;
int rand() {
    v++;
    return (int)(cos(v)*RAND_MAX) ^ 1376312589;
    // v = ufindnoise2(v);
    // return v;
}

void netplay_connect() {
    swap = 1;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        sterf("Error creating socket");
    }
    struct hostent *server = gethostbyname("89.99.57.228");
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(8008);
    fprintf(stdout,"Connecting to server...\n");
    if (connect(fd,&serv_addr,sizeof(serv_addr)) < 0) {
        sterf("Error connecting to address");
    }
    fprintf(stdout,"Connected\n");
}

static CircularBuffer m_events;
static SDL_PollEvent_Func SDL_PollEvent_original = NULL;
static SDL_GL_SwapBuffers_Func original = NULL;
static SDL_GetKeyState_Func SDL_GetKeyState_original = NULL;
static SDL_Init_Func SDL_Init_original = NULL;


void koekinit()__attribute__((constructor));
void koekinit() {
    SDL_PollEvent_original = get_func("SDL_PollEvent");
    original = get_func("SDL_GL_SwapBuffers");
    SDL_GetKeyState_original = get_func("SDL_GetKeyState");
    SDL_Init_original = get_func("SDL_Init");

    cbInit(&m_events, 256);
}

int SDL_Init(Uint32 flags) {
    fprintf(stdout, "sizeof(KeyboardEvent) = %u\n", (uint32_t)sizeof(KeyboardEvent));
    fprintf(stdout, "sizeof(SDL_Event) = %u\n", (uint32_t)sizeof(SDL_Event));
    fflush(stdout);

    if (getenv("SDLNETPLAY_LISTEN")) {
        netplay_listen();
    } else if (getenv("SDLNETPLAY_CONNECT")) {
        netplay_connect();
    } else {
        sterf("No SDLNETPLAY_LISTEN or SDLNETPLAY_CONNECT");
    }

    { // Disable Nagle buffering algorithm.
        int flag = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    }

    return SDL_Init_original(flags);
}

void printEvent(SDL_Event *event) {
    fprintf(stdout, "%d: %x > %s\n", framenumber, event->type, SDL_GetKeyName(event->key.keysym.sym));
}

int SDL_PollEvent(SDL_Event *event) {
    // Trek m_events leeg
    if(cbIsEmpty(&m_events)) {
        return 0;
    } else {
        cbRead(&m_events,event);
        printEvent(event);
        return 1;
    }
}

void write_event(int fd, KeyboardEvent *event) {
    write_until(fd, event, sizeof(KeyboardEvent));  
}

void SDL_GL_SwapBuffers(void) {
    fflush(stdout);
    framenumber++;
    SDL_Event event;
    darray_event local_events = darray_new();
    while(SDL_PollEvent_original(&event)) {
        

        // Send related events to remote.
        switch(event.type) {
            case SDL_KEYDOWN:
            case SDL_KEYUP: { // Send event to remote.
                PacketType packetType = PACKET_EVENT;
                write_until(fd,&packetType,sizeof(PacketType));

                KeyboardEvent kb_event = fromSDLEvent(&event);
                write_event(fd, &kb_event);
                darray_push(local_events, kb_event);
            } break;
        }
    }

    { // Send sync to remote.
        PacketType packetType = PACKET_SYNC;
        write_until(fd,&packetType,sizeof(packetType));
        uint32_t local_framenumber = framenumber;
        write_until(fd,&local_framenumber,sizeof(local_framenumber));
    }

    // Receive remote packets.
    bool sync_received = false;
    darray_event remote_events = darray_new();
    while(!sync_received) {
        PacketType packet_type = 0; 
        ssize_t read_bytes = read_until(fd, &packet_type, sizeof(PacketType));
        if(read_bytes < 0) {
            sterf("End of remote stream");
        }
        switch(packet_type) {
            case PACKET_SYNC: {
                fprintf(stdout,"%d: sync\n", framenumber);
                uint32_t remote_framenumber;
                if (read_until(fd,&remote_framenumber,sizeof(remote_framenumber)) < 0) {
                    fprintf(stdout,"Desync detected: %d != %d\n", remote_framenumber, framenumber);
                    sterf("Desync");
                }
                sync_received = true;
            } break;
            case PACKET_EVENT: {
                KeyboardEvent recv_event;
                read_bytes = read_until(fd, (uint8_t*)&recv_event, sizeof(recv_event));
                if (read_bytes < 0) {
                    sterf("End of remote stream");
                }
                darray_push(remote_events, recv_event);
            } break;
            default:
                sterf("No such packet type");
                break;
        }
    }
    if (swap) {
        KeyboardEvent *it;
        darray_foreach(it, local_events) {
            SDL_Event ev = toSDLEvent(it);
            printf("local: "); printEvent(&ev);
            cbWrite(&m_events, &ev);
        }
        darray_foreach(it, remote_events) {
            SDL_Event ev = toSDLEvent(it);
            printf("remote: "); printEvent(&ev);
            cbWrite(&m_events, &ev);
        }
    } else {
        KeyboardEvent *it;
        darray_foreach(it, remote_events) {
            SDL_Event ev = toSDLEvent(it);
            printf("remote: "); printEvent(&ev);
            cbWrite(&m_events, &ev);
        }
        darray_foreach(it, local_events) {
            SDL_Event ev = toSDLEvent(it);
            printf("local: "); printEvent(&ev);
            cbWrite(&m_events, &ev);
        }
    }
    

    darray_free(local_events);
    darray_free(remote_events);

    // Call SDL_GL_SwapBuffers
    original();
}
