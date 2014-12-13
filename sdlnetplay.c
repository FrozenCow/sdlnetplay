// vim: tabstop=4 shiftwidth=4 expandtab
#include <cstdlib>
#include <cstdio>
#include <SDL/SDL.h>
#include <dlfcn.h>
#include <cstdint>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <stdbool.h>
#include <cmath>
#include <climits>
#include <queue>
#include <vector>
#include <pthread.h>
#include <sys/time.h>
#include <GL/gl.h>
#include <sys/stat.h>

#include "utils.h"
#include "protocol.h"
#include "hash.h"

using namespace sdlnetplay;

// Global variables
static std::queue<SDL_Event> m_events;
static int framenumber = 0;
static int m_mouse_x = 0;
static int m_mouse_y = 0;
static Uint32 m_mouse_state = 0;
static pthread_t m_main_thread = 0;

static thread_local int m_masking = 0;
//typedef decltype(&SDL_PollEvent) SDL_PollEvent_type;

#define OVERRIDE_FUNC(TYPE) \
static decltype(&TYPE) TYPE ## _original; \
void TYPE ## _original_constructor()__attribute__((constructor)); \
void TYPE ## _original_constructor() { \
    TYPE ## _original = (decltype(&TYPE))dlsym(RTLD_NEXT, #TYPE); }


OVERRIDE_FUNC(SDL_PollEvent)
OVERRIDE_FUNC(SDL_PumpEvents)
OVERRIDE_FUNC(SDL_PeepEvents)
OVERRIDE_FUNC(SDL_GL_SwapBuffers)
OVERRIDE_FUNC(SDL_GetKeyState)
OVERRIDE_FUNC(SDL_GetMouseState)
OVERRIDE_FUNC(SDL_GetRelativeMouseState)
OVERRIDE_FUNC(SDL_Init)
OVERRIDE_FUNC(SDL_SetVideoMode)
OVERRIDE_FUNC(SDL_GetTicks)
OVERRIDE_FUNC(SDL_Flip)
OVERRIDE_FUNC(SDL_GetModState)
OVERRIDE_FUNC(SDL_Delay)
OVERRIDE_FUNC(SDL_OpenAudio)
OVERRIDE_FUNC(SDL_CreateThread)
OVERRIDE_FUNC(pthread_mutex_lock)
OVERRIDE_FUNC(pthread_create)
OVERRIDE_FUNC(time)
OVERRIDE_FUNC(clock)
OVERRIDE_FUNC(clock_gettime)
OVERRIDE_FUNC(gettimeofday)
OVERRIDE_FUNC(rand)
OVERRIDE_FUNC(glBegin)
OVERRIDE_FUNC(SDL_GetAppState)

#define FSYNC do { fsync(__func__); } while(0);

struct Unmask {
    Unmask() {
        debug("unmasking");
        m_masking++;
    };
    ~Unmask() {
        debug("masking");
        m_masking--;
    }
};

inline bool ismasking() { return m_masking == 0; }

void fsync(const char *name) {
return;
    if (pthread_self() != m_main_thread) {
        return;
    }
    FSyncPacket local_packet(name);
    write_to_channel(fsyncChannel, local_packet);
    FSyncPacket remote_packet;
    read_from_channel(fsyncChannel, &remote_packet);
    if (strcmp(local_packet.ptr(), remote_packet.ptr()) != 0) {
        debug("FAIL");
        debug(local_packet.ptr());
        debug(remote_packet.ptr());
        die("FSYNC Failed");
    }
}

int screen_width = 0;
int screen_height = 0;
size_t image_buf_size = 0;
static char *image_buf = NULL;
uint32_t screenhash() {
    size_t desired_buf_size = screen_width * screen_height * 3;
    if(desired_buf_size == 0) {
        // Screen is not initialized yet
        return 0;
    }
    if (desired_buf_size != image_buf_size) {
        if (image_buf) {
            free(image_buf);
        }
        image_buf = (char*)malloc(desired_buf_size);
        image_buf_size = desired_buf_size;
    }
    
    glReadPixels(0, 0, screen_width, screen_height, GL_RGB, GL_UNSIGNED_BYTE, image_buf);
    return SuperFastHash(image_buf, image_buf_size);
}

void screensync() {
    uint32_t local_hash = screenhash();
    FSyncPacket local_packet;
    local_packet.mName.resize(1024);
    sprintf(local_packet.ptr(), "SCREENHASH_%X", local_hash);
    write_to_channel(fsyncChannel, local_packet);
    FSyncPacket remote_packet;
    read_from_channel(fsyncChannel, &remote_packet);
    if (strcmp(local_packet.ptr(), remote_packet.ptr()) != 0) {
        debug("Saving to /tmp/screen.rgb");
        int screenFile = open("/tmp/screen.rgb",O_RDWR | O_CREAT, S_IRUSR | S_IRGRP);
        write_until(screenFile,image_buf,image_buf_size);
        close(screenFile);
        
        debug("Converting to /tmp/screen.png");
        system("convert -size 800x600 -depth 8 /tmp/screen.rgb /tmp/screen.png");
        
        debug("Reading from /tmp/screen.png");
        struct stat stat_buf;
        stat("/tmp/screen.png", &stat_buf);
        fprintf(stderr, "/tmp/screen.png size: %ld\n", stat_buf.st_size);
        FSyncPacket local_screen_packet;
        local_screen_packet.mName.resize(stat_buf.st_size);
        screenFile = open("/tmp/screen.png",O_RDONLY);
        read_until(screenFile,local_screen_packet.ptr(),stat_buf.st_size);
        close(screenFile);
        
        debug("Sending /tmp/screen.png");
        write_to_channel(fsyncChannel, local_screen_packet);
        
        debug("Receiving /tmp/screen_other.png");
        FSyncPacket remote_screen_packet;
        read_from_channel(fsyncChannel, &remote_screen_packet);
        
        debug("Saving /tmp/screen_other.png");
        screenFile = open("/tmp/screen_other.png",O_RDWR | O_CREAT, S_IRUSR | S_IRGRP);
        write_until(screenFile,remote_screen_packet.ptr(),remote_screen_packet.mName.size());
        close(screenFile);
        
        debug("FAIL");
        debug(local_packet.ptr());
        debug(remote_packet.ptr());
        die("FSYNC Failed");
    }
}

bool caller_is_in_executable(void * caller_ret_addr) {

    return
        (sizeof(void*) == 8 && caller_ret_addr < (void*)0x500000) ||
        (sizeof(void*) == 4 && caller_ret_addr < (void*)0x09000000);
}

#define CALLER_IS_LIBRARY (!caller_is_in_executable(__builtin_return_address(0)))

Uint8 SDL_GetAppState(void) {
    return SDL_APPACTIVE | SDL_APPMOUSEFOCUS | SDL_APPINPUTFOCUS;
}

void SDL_Delay(Uint32 ticks) {
//    void * return_addr = __builtin_return_address(0);

    if (!ismasking() || CALLER_IS_LIBRARY) { return SDL_Delay_original(ticks); }
    //debug("SDL_Delay");
    FSYNC
    //nope
}

static thread_local uint64_t t = 0;
/*time_t time (time_t* timer) {
    debug("time");
    return t++;
}*/

clock_t clock (void) {
    debug("clock");
    FSYNC
    return t++;
}

//struct timespec {
//        time_t   tv_sec;        /* seconds */
//        long     tv_nsec;       /* nanoseconds */
//};

int clock_gettime(clockid_t clk_id, struct timespec *tp) {
    if (!ismasking() || CALLER_IS_LIBRARY) { return clock_gettime_original(clk_id, tp); }
    debug("clock_gettime");
    FSYNC
    t++;
    tp->tv_sec = t;
    tp->tv_nsec = t;

    return 0; // success
}


//struct timeval {
//    time_t      tv_sec;     /* seconds */
//    suseconds_t tv_usec;    /* microseconds */
//};

//struct timezone {
//    int tz_minuteswest;     /* minutes west of Greenwich */
//    int tz_dsttime;         /* type of DST correction */
//};

/*int gettimeofday(struct timeval *tv, struct timezone *tz) {
    debug("gettimeofday");
    tv->tv_sec = t;
    tv->tv_usec = t;
    tz->tz_minuteswest = 0;
    tz->tz_dsttime = 0;
    return 0; // success
}*/

void init_sdlnetplay()__attribute__((constructor));
void init_sdlnetplay() {
    //printf("RAND_MAX: %d\n", RAND_MAX);

}


int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine) (void *), void *arg) {
    fprintf(stderr, "start_routine: %p ", start_routine);
    debug("pthread_create");
    Unmask m;
    return pthread_create_original(thread, attr, start_routine, arg);
}

//int pthread_mutex_lock(pthread_mutex_t *mutex) {
/*    if(SDL_Init_original) {
        //fprintf(stdout, "thread: %lu, mutex: %x ", pthread_self(), (unsigned int)mutex);
        //debug("pthread_mutex_lock");
        //usleep(1000000);
    }*/
//    return pthread_mutex_lock_original(mutex);
//}


static thread_local uint32_t v = 0;
int rand() {
    if (!ismasking() || CALLER_IS_LIBRARY) { return rand_original(); }
    debug("rand");
    FSYNC
    v = abs((v + (RAND_MAX / 32) + 1) % RAND_MAX);
//    return (int)(cos(v)*RAND_MAX) ^ 1376312589;
    return v;
}

static Uint8 m_keystate[SDLK_LAST+1] = { 0 };
Uint8 *SDL_GetKeyState(int *numkeys) {
    if (!ismasking() || CALLER_IS_LIBRARY) { return SDL_GetKeyState_original(numkeys); }
    debug("SDL_GetKeyState");
    FSYNC
    if(numkeys) {
        *numkeys = sizeof(m_keystate);
    }
    return &m_keystate[0];
}
static SDLMod m_modstate = KMOD_NONE;
SDLMod SDL_GetModState(void) {
    if (!ismasking() || CALLER_IS_LIBRARY) { return SDL_GetModState_original(); }
    FSYNC
    return m_modstate;
}

DECLSPEC Uint8 SDLCALL SDL_GetMouseState(int *x, int *y) {
    if (!ismasking() || CALLER_IS_LIBRARY) { return SDL_GetMouseState_original(x, y); }
    debug("SDL_GetMouseState");
    FSYNC
    if(x) *x = m_mouse_x;
    if(y) *y = m_mouse_y;
    return m_mouse_state;
}

static int m_mouse_rel_x = 0;
static int m_mouse_rel_y = 0;
Uint8 SDLCALL SDL_GetRelativeMouseState(int *x, int *y) {
    if (!ismasking() || CALLER_IS_LIBRARY) { return SDL_GetRelativeMouseState_original(x, y); }
    debug("SDL_GetRelativeMouseState");
    FSYNC
    if(x) { *x = m_mouse_rel_x - m_mouse_x; }
    if(y) { *y = m_mouse_rel_y - m_mouse_y; }
    m_mouse_rel_x = m_mouse_x;
    m_mouse_rel_y = m_mouse_y;
    return m_mouse_state;
}

SDL_Surface *SDL_SetVideoMode(int width, int height, int bpp, Uint32 flags) {
    if (!ismasking() || CALLER_IS_LIBRARY) { return SDL_SetVideoMode_original(width, height, bpp, flags); }
    debug("SDL_SetVideoMode");
    FSYNC

    Unmask m;
    screen_width = width;
    screen_height = height;
    return SDL_SetVideoMode_original(width, height, bpp, flags);
}

bool issdlinit = false;
int SDL_Init(Uint32 flags) {
    if (issdlinit) {
        return SDL_Init_original(flags);
    }
    issdlinit = true;
    void * return_addr = __builtin_return_address(0);
    fprintf(stdout, "Return addr: %p\n", return_addr);
    
    void * end_addr = dlsym(RTLD_DEFAULT, "_end");
    fprintf(stdout, "End addr: %p\n", end_addr);
    

    if (!ismasking() || CALLER_IS_LIBRARY) { return SDL_Init_original(flags); }
    debug("SDL_Init");
    m_main_thread = pthread_self();
    fprintf(stdout, "sizeof(NPEvent) = %u\n", (uint32_t)sizeof(NPEvent));
    fprintf(stdout, "sizeof(SDL_Event) = %u\n", (uint32_t)sizeof(SDL_Event));
    fflush(stdout);


    if (getenv("SDLNETPLAY_LISTEN")) {
        netplay_listen();
    } else if (getenv("SDLNETPLAY_CONNECT")) {
        if(getenv("SDLNETPLAY_HOSTNAME") == NULL) {
            die("No SDLNETPLAY_HOSTNAME");
        }
        netplay_connect();
    } else {
        die("No SDLNETPLAY_LISTEN or SDLNETPLAY_CONNECT");
    }

    FSYNC

    Unmask m;
    return SDL_Init_original(flags);
}

static Uint32 ticks = 0;
Uint32 SDL_GetTicks(void) {
    if (!ismasking() || CALLER_IS_LIBRARY) { return SDL_GetTicks_original(); }
    fprintf(stdout, "ticks: %u\n", ticks);
    debug("SDL_GetTicks");
    FSYNC
    return (ticks += 100);
}

static void printEvent(SDL_Event *event) {
    switch(event->type) {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        fprintf(stdout, "%d: %x > %s\n", framenumber, event->type, SDL_GetKeyName(event->key.keysym.sym));
        break;
    default:
        fprintf(stdout, "\n");
        break;
    }
}

static bool syncReceived = true;
static std::vector<NPEvent> local_events;

static void receiveSync() {
    if (syncReceived) { return; }
    // Receive remote packets.
    SyncPacket syncPacket;
    read_from_channel(syncChannel, &syncPacket);
    std::vector<NPEvent> &remote_events = syncPacket.events;

    if (is_server) {
        for(auto &it : local_events) {
            SDL_Event ev = toSDLEvent(&it);
            printf("local: "); printEvent(&ev);
            
            m_events.push(ev);
        }
        for(auto &it : remote_events) {
            SDL_Event ev = toSDLEvent(&it);
            printf("remote: "); printEvent(&ev);
            m_events.push(ev);
        }
    } else {
        for(auto &it : remote_events) {
            SDL_Event ev = toSDLEvent(&it);
            printf("remote: "); printEvent(&ev);
            m_events.push(ev);
        }
        for(auto &it : local_events) {
            SDL_Event ev = toSDLEvent(&it);
            printf("local: "); printEvent(&ev);
            m_events.push(ev);
        }
    }
    local_events.clear();
    
    syncReceived = true;
}

static void sendSync() {
    if (!syncReceived) {
        receiveSync();
    }
    
    SyncPacket packet;
    
    // Send events, empty buffer.
    fflush(stdout);
    framenumber++;
    ticks++;
    SDL_Event event;
    Unmask m;
    while(SDL_PollEvent_original(&event)) {
        // Send related events to remote.
        switch(event.type) {
            case SDL_MOUSEMOTION:
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_KEYDOWN:
            case SDL_KEYUP: { // Send event to remote.
                NPEvent npevent = fromSDLEvent(&event);
                packet.events.push_back(npevent);
                local_events.push_back(npevent);
            } break;
            case SDL_NOEVENT:
            case SDL_ACTIVEEVENT:
            case SDL_JOYAXISMOTION:
            case SDL_JOYBALLMOTION:
            case SDL_JOYHATMOTION:
            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
            case SDL_SYSWMEVENT:
            case SDL_VIDEORESIZE:
            case SDL_USEREVENT:
            case SDL_VIDEOEXPOSE: {
            	// Ignore these.
            } break;
            case SDL_QUIT: {
            	// Let these through.
            	m_events.push(event);
            } break;
            default: {
                die("Unknown SDL event");
            } break;
        }
    }

    packet.framenumber = framenumber;
    write_to_channel(syncChannel, packet);

    if (framenumber % 60) {
        screensync();
    }

    syncReceived = false;
}

int SDL_PeepEvents(SDL_Event *events, int numevents, SDL_eventaction action, Uint32 mask) {
    if (!ismasking() || CALLER_IS_LIBRARY) { return SDL_PeepEvents_original(events, numevents, action, mask); }
    debug("SDL_PeepEvents");
    die("SDL_PeepEvents not implemented");
    return 0;
}

void SDL_PumpEvents(void) {
    if (!ismasking() || CALLER_IS_LIBRARY) { return SDL_PumpEvents_original(); }
    debug("SDL_PumpEvents");
    FSYNC
    return;
}

int SDL_PollEvent(SDL_Event *event) {
    debug("SDL_PollEvent");
    FSYNC
    // Trek m_events leeg
    if(m_events.empty()) {
        sendSync();
        return 0;
    } else {
        *event = m_events.front();
        m_events.pop();
        switch(event->type) {
            case SDL_KEYDOWN:
                m_keystate[event->key.keysym.sym] = 1;
                m_modstate = event->key.keysym.mod;
                break;
            case SDL_KEYUP:
                m_keystate[event->key.keysym.sym] = 0;
                m_modstate = event->key.keysym.mod;
                break;
            case SDL_MOUSEMOTION:
                m_mouse_state = event->motion.state;
                m_mouse_x = event->motion.x;
                m_mouse_y = event->motion.y;
                break;
            case SDL_MOUSEBUTTONUP:
                m_mouse_state &= ~SDL_BUTTON(event->button.button);
                m_mouse_x = event->button.x;
                m_mouse_y = event->button.y;
                break;
            case SDL_MOUSEBUTTONDOWN:
                m_mouse_state |= SDL_BUTTON(event->button.button);
                m_mouse_x = event->button.x;
                m_mouse_y = event->button.y;
                break;
            default:
                break;
        }
        printEvent(event);
        return 1;
    }
    return 1;
}

int SDLCALL SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained) {
    if (!ismasking() || CALLER_IS_LIBRARY) { return SDL_OpenAudio_original(desired, obtained); }
    debug("SDL_OpenAudio");
    FSYNC
    Unmask m;
    return SDL_OpenAudio_original(desired, obtained);
}

int SDL_Flip(SDL_Surface* screen) {
    if (!ismasking() || CALLER_IS_LIBRARY) { return SDL_Flip_original(screen); }
    debug("SDL_Flip");
    FSYNC
    receiveSync();
    Unmask m;
    return SDL_Flip_original(screen);
}

void SDL_GL_SwapBuffers(void) {
    if (!ismasking() || CALLER_IS_LIBRARY) { return SDL_GL_SwapBuffers_original(); }
    debug("SDL_GL_SwapBuffers");
    FSYNC
    receiveSync();
    Unmask m;
    SDL_GL_SwapBuffers_original();
}
/*
void glBegin(GLenum mode) {
    if (!ismasking() || CALLER_IS_LIBRARY) { return glBegin_original(mode); }
    debug("glBegin");
    FSYNC
    glBegin_original(mode);
}
*/

SDL_GrabMode SDL_WM_GrabInput(SDL_GrabMode mode) {
	return SDL_GRAB_OFF;
}

SDL_Thread * SDL_CreateThread(int (SDLCALL *fn)(void *), void *data) {
    if (!ismasking() || CALLER_IS_LIBRARY) { return SDL_CreateThread_original(fn, data); }
    debug("SDL_CreateThread");
    fprintf(stdout, "thread_func: %p ", fn);
    FSYNC
    return SDL_CreateThread_original(fn, data);
}
