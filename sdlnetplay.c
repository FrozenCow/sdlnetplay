// vim: tabstop=4 shiftwidth=4 expandtab
#include <cstdlib>
#include <cstdio>
#include <SDL/SDL.h>
#include <dlfcn.h>
#include <cstdint>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <stdbool.h>
#include <cmath>
#include <climits>
#include <queue>
#include <vector>

#include "utils.h"
#include "protocol.h"

using namespace sdlnetplay;

// Global variables
static std::queue<SDL_Event> m_events;
static int framenumber = 0;

//typedef decltype(&SDL_PollEvent) SDL_PollEvent_type;

#define OVERRIDE_FUNC(TYPE) \
static decltype(&TYPE) TYPE ## _original; \
void TYPE ## _original_constructor()__attribute__((constructor)); \
void TYPE ## _original_constructor() { \
    TYPE ## _original = (decltype(&TYPE))dlsym(RTLD_NEXT, #TYPE); }


OVERRIDE_FUNC(SDL_PollEvent)
OVERRIDE_FUNC(SDL_GL_SwapBuffers)
OVERRIDE_FUNC(SDL_GetKeyState)
OVERRIDE_FUNC(SDL_Init)
OVERRIDE_FUNC(SDL_SetVideoMode)
OVERRIDE_FUNC(SDL_GetTicks)
OVERRIDE_FUNC(SDL_Flip)
OVERRIDE_FUNC(SDL_GetModState)



void init_sdlnetplay()__attribute__((constructor));
void init_sdlnetplay() {
    //printf("RAND_MAX: %d\n", RAND_MAX);
}

void debug(const char *msg) {
    fprintf(stdout, "%s\n", msg);
    fflush(stdout);
}

static uint32_t v = 0;
int rand() {
    debug("rand");
    v = abs((v + (RAND_MAX / 32) + 1) % RAND_MAX);
//    return (int)(cos(v)*RAND_MAX) ^ 1376312589;
    return v;
}

static Uint8 m_keystate[SDLK_LAST+1] = { 0 };
Uint8 *SDL_GetKeyState(int *numkeys) {
    debug("SDL_GetKeyState");
    if(numkeys) {
        *numkeys = sizeof(m_keystate);
    }
    return &m_keystate[0];
}
static SDLMod m_modstate = KMOD_NONE;
SDLMod SDL_GetModState(void) {
    return m_modstate;
}

SDL_Surface *SDL_SetVideoMode(int width, int height, int bpp, Uint32 flags) {
    debug("SDL_SetVideoMode");
    SDL_Surface *result = SDL_SetVideoMode_original(width, height, bpp, flags);
    
    return result;
}

int SDL_Init(Uint32 flags) {
    debug("SDL_Init");
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

    return SDL_Init_original(flags);
}

static Uint32 ticks = 0;
Uint32 SDL_GetTicks(void) {
    debug("SDL_GetTicks");
    return ticks++;
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
    bool sync_packet_received = false;
    std::vector<NPEvent> remote_events;
    while(!sync_packet_received) {
        PacketType packet_type = 0; 
        ssize_t read_bytes = read_until(fd, &packet_type, sizeof(PacketType));
        if(read_bytes < 0) {
            die("End of remote stream");
        }
        switch(packet_type) {
            case PACKET_SYNC: {
                fprintf(stdout,"%d: sync\n", framenumber);
                uint32_t remote_framenumber;
                if (read_until(fd,&remote_framenumber,sizeof(remote_framenumber)) < 0) {
                    fprintf(stdout,"Desync detected: %d != %d\n", remote_framenumber, framenumber);
                    die("Desync");
                }
                sync_packet_received = true;
            } break;
            case PACKET_EVENT: {
                NPEvent recv_event;
                read_bytes = read_until(fd, (uint8_t*)&recv_event, sizeof(recv_event));
                if (read_bytes < 0) {
                    die("End of remote stream");
                }
                remote_events.push_back(recv_event);
            } break;
            default:
                die("No such packet type");
                break;
        }
    }
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
    // Send events, empty buffer.
    fflush(stdout);
    framenumber++;
    ticks++;
    SDL_Event event;
    while(SDL_PollEvent_original(&event)) {
        // Send related events to remote.
        switch(event.type) {
            case SDL_MOUSEMOTION:
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_KEYDOWN:
            case SDL_KEYUP: { // Send event to remote.
                PacketType packetType = PACKET_EVENT;
                write_until(fd,&packetType,sizeof(PacketType));
                NPEvent npevent = fromSDLEvent(&event);
                write_event(fd, &npevent);
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
            case SDL_VIDEOEXPOSE: {
            	// Ignore these.
            } break;
            case SDL_USEREVENT:
            case SDL_QUIT: {
            	// Let these through.
            	m_events.push(event);
            } break;
            default: {
                die("Unknown SDL event");
            } break;
        }
    }

    { // Send sync to remote.
        PacketType packetType = PACKET_SYNC;
        write_until(fd,&packetType,sizeof(packetType));
        uint32_t local_framenumber = framenumber;
        write_until(fd,&local_framenumber,sizeof(local_framenumber));
    }

    syncReceived = false;
}

int SDL_PollEvent(SDL_Event *event) {
    debug("SDL_PollEvent");
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
            default:
                break;
        }
        printEvent(event);
        return 1;
    }
    return 1;
}

int SDL_Flip(SDL_Surface* screen) {
    debug("SDL_Flip");
    receiveSync();
    return SDL_Flip_original(screen);
}

void SDL_GL_SwapBuffers(void) {
    debug("SDL_GL_SwapBuffers");
    receiveSync();
    SDL_GL_SwapBuffers_original();
}
